

#include "db_common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int list_childs(db *db, MDB_txn *ptxn, size_t *out_len, db_child **out_items) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;
  MDB_cursor *cursor;
  MDB_stat stat;
  db_child *out_data, *data_ptr;

  *out_len = 0;
  *out_items = NULL;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  rc = mdb_stat(txn, db->child, &stat);
  if (rc) {
    fprintf(stderr, "Failed to get database stat: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }
  out_data = (db_child *)malloc(stat.ms_entries * sizeof(db_child));
  if (!out_data) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto _defer_1;
  }
  memset(out_data, 0, stat.ms_entries * sizeof(db_child));

  rc = mdb_cursor_open(txn, db->child, &cursor);
  if (rc) {
    fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
    free(out_data);
    goto _defer_1;
  }

  rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
  if (rc == MDB_NOTFOUND) {
    // empty (not error)
    rc = 0;
    goto _defer_2;
  }
  if (rc) {
    fprintf(stderr, "Error setting cursor: %s\n", mdb_strerror(rc));
    free(out_data);
    goto _defer_2;
  }

  data_ptr = out_data;
  while (1) {
    memcpy(data_ptr, data.mv_data, data.mv_size);

    // next
    data_ptr++;
    rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
    if (rc == 0)
      continue;
    if (rc == MDB_NOTFOUND) {
      rc = 0;
      break;
    }
    // else, error
    fprintf(stderr, "Failed to iterate on cursor: %s\n", mdb_strerror(rc));
    free(out_data);
    goto _defer_2;
  }

  *out_len = stat.ms_entries;
  *out_items = out_data;
_defer_2:
  mdb_cursor_close(cursor);
_defer_1:
  mdb_txn_abort(txn);

  return rc;
}

int get_child(db *db, MDB_txn *ptxn, uint64_t item_id, db_child *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->child, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get item: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  memcpy(out_item, data.mv_data, data.mv_size);

_defer_1:
  mdb_txn_abort(txn);

  return rc;
}

int put_child(db *db, MDB_txn *ptxn, db_child *in_out_item) {
  MDB_txn *txn = NULL;
  int rc;
  MDB_val key, data;
  db_child prev;
  uint64_t id = in_out_item->id;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  if (id == 0) {
    // new item
    rc = db_new_pk(db, PK_child, txn, &id);
    if (rc)
      goto _defer_1;
    memset(&prev, 0, sizeof(prev));
    in_out_item->id = id;
  } else {
    // fetch previous one
    key = (MDB_val){sizeof(uint64_t), &id};
    rc = mdb_get(txn, db->child, &key, &data);
    if (rc) {
      fprintf(stderr, "Failed to get previous item: %s\n", mdb_strerror(rc));
      goto _defer_1;
    }
    memcpy(&prev, data.mv_data, data.mv_size);
  }

  key = (MDB_val){sizeof(uint64_t), &id};
  data = (MDB_val){sizeof(db_child), in_out_item};
  rc = mdb_put(txn, db->child, &key, &data, 0);
  if (rc) {
    fprintf(stderr, "Failed to insert item: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  // secondary indexes

  if (prev.parent_id != in_out_item->parent_id) {
    if (prev.parent_id != 0) {
      key = (MDB_val){sizeof(uint64_t), &prev.parent_id};
      data = (MDB_val){sizeof(uint64_t), &prev.id};
      rc = mdb_del(txn, db->parent_children, &key, &data);
      if (rc) {
        fprintf(stderr, "Failed to update secondary index: %s\n",
                mdb_strerror(rc));
        goto _defer_1;
      }
    }
    if (in_out_item->parent_id != 0) {
      key = (MDB_val){sizeof(uint64_t), &in_out_item->parent_id};
      data = (MDB_val){sizeof(uint64_t), &in_out_item->id};
      rc = mdb_put(txn, db->parent_children, &key, &data, 0);
      if (rc) {
        fprintf(stderr, "Failed to update secondary index: %s\n",
                mdb_strerror(rc));
        goto _defer_1;
      }
    }
  }

  rc = mdb_txn_commit(txn);
  if (rc) {
    fprintf(stderr, "Failed to commit transaction: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }
  txn = NULL;

_defer_1:
  if (txn)
    mdb_txn_abort(txn);

  return rc;
}

int delete_child(db *db, MDB_txn *ptxn, uint64_t item_id, db_child *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->child, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get original from database: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }
  memcpy(out_item, data.mv_data, data.mv_size);

  rc = mdb_del(txn, db->child, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to delete item from database: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }

  key = (MDB_val){sizeof(uint64_t), &out_item->parent_id};
  data = (MDB_val){sizeof(uint64_t), &out_item->id};
  rc = mdb_del(txn, db->parent_children, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to update secondary indexes: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }

  rc = mdb_txn_commit(txn);
  if (rc) {
    fprintf(stderr, "Failed to commit transaction: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }
  txn = NULL;
_defer_1:
  if (txn)
    mdb_txn_abort(txn);
  return rc;
}

int get_parent_childrens(db *db, MDB_txn *ptxn, uint64_t parent_id,
                         size_t *out_len, db_child **out_items) {
  MDB_txn *txn;
  MDB_cursor *cursor;
  int rc;
  MDB_val key, data;
  db_child *out_array;

  *out_len = 0;
  *out_items = NULL;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  rc = mdb_cursor_open(txn, db->parent_children, &cursor);
  if (rc) {
    fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  key = (MDB_val){sizeof(uint64_t), &parent_id};
  rc = mdb_cursor_get(cursor, &key, &data, MDB_SET);
  if (rc == MDB_NOTFOUND) {
    rc = 0;
    goto _defer_2;
  }
  if (rc) {
    fprintf(stderr, "Failed to position cursor: %s\n", mdb_strerror(rc));
    goto _defer_2;
  }

  rc = mdb_cursor_count(cursor, out_len);
  if (rc) {
    fprintf(stderr, "Failed to get cursor count: %s\n", mdb_strerror(rc));
    goto _defer_2;
  }

  out_array = (db_child *)malloc(sizeof(db_child) * (*out_len));
  if (!out_array) {
    rc = ENOMEM;
    fprintf(stderr, "Failed to allocate memory\n");
    goto _defer_2;
  }

  db_child *data_ptr = out_array;
  for (int i = 0; i < *out_len; i++) {
    uint64_t item_id = *((uint64_t *)data.mv_data);
    // NULL as this is a read transaction
    rc = get_child(db, NULL, item_id, out_array + i);
    if (rc) {
      free(out_array);
      goto _defer_2;
    }
    rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT_DUP);
    if (rc == 0)
      continue;
    if ((rc == MDB_NOTFOUND && i < ((*out_len) - 1)) || rc != MDB_NOTFOUND) {
      fprintf(stderr, "Failed to move cursor: %s\n", mdb_strerror(rc));
      free(out_array);
      goto _defer_2;
    }
    // else
    rc = 0;
  }

  *out_items = out_array;
_defer_2:
  mdb_cursor_close(cursor);
_defer_1:
  mdb_txn_abort(txn);

  return rc;
}
