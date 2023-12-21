

#include "db_common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int list_parents(db *db, MDB_txn *ptxn, size_t *out_len,
                 db_parent **out_items) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;
  MDB_cursor *cursor;
  MDB_stat stat;
  db_parent *out_data, *data_ptr;

  *out_len = 0;
  *out_items = NULL;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  rc = mdb_stat(txn, db->parent, &stat);
  if (rc) {
    fprintf(stderr, "Failed to get database stat: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }
  out_data = (db_parent *)malloc(stat.ms_entries * sizeof(db_parent));
  if (!out_data) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto _defer_1;
  }
  memset(out_data, 0, stat.ms_entries * sizeof(db_parent));

  rc = mdb_cursor_open(txn, db->parent, &cursor);
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

int get_parent(db *db, MDB_txn *ptxn, uint64_t item_id, db_parent *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->parent, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get item: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  memcpy(out_item, data.mv_data, data.mv_size);

_defer_1:
  mdb_txn_abort(txn);

  return rc;
}

int put_parent(db *db, MDB_txn *ptxn, db_parent *in_out_item) {
  MDB_txn *txn = NULL;
  int rc;
  MDB_val key, data;
  db_parent prev;
  uint64_t id = in_out_item->id;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  if (id == 0) {
    // new item
    rc = db_new_pk(db, PK_parent, txn, &id);
    if (rc)
      goto _defer_1;
    memset(&prev, 0, sizeof(prev));
    in_out_item->id = id;
  } else {
    // fetch previous one
    key = (MDB_val){sizeof(uint64_t), &id};
    rc = mdb_get(txn, db->parent, &key, &data);
    if (rc) {
      fprintf(stderr, "Failed to get previous item: %s\n", mdb_strerror(rc));
      goto _defer_1;
    }
    memcpy(&prev, data.mv_data, data.mv_size);
  }

  key = (MDB_val){sizeof(uint64_t), &id};
  data = (MDB_val){sizeof(db_parent), in_out_item};
  rc = mdb_put(txn, db->parent, &key, &data, 0);
  if (rc) {
    fprintf(stderr, "Failed to insert item: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  // secondary indexes

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

int delete_parent(db *db, MDB_txn *ptxn, uint64_t item_id,
                  db_parent *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->parent, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get original from database: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }
  memcpy(out_item, data.mv_data, data.mv_size);

  rc = mdb_del(txn, db->parent, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to delete item from database: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }

  {
    MDB_cursor *cursor;
    rc = mdb_cursor_open(txn, db->parent_children, &cursor);
    if (rc) {
      fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
      goto _defer_1;
    }

    key = (MDB_val){sizeof(uint64_t), &out_item->id};

    while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_SET)) == 0) {
      MDB_val key2 = {sizeof(uint64_t), data.mv_data};
      MDB_val data2;
      rc = mdb_get(txn, db->child, &key2, &data2);
      if (rc) {
        fprintf(stderr, "Failed to update child index: %s\n", mdb_strerror(rc));
        goto _si_defer_parent_children;
      }
      db_child target_item;
      memcpy(&target_item, data2.mv_data, data2.mv_size);
      target_item.parent_id = 0;
      data2 = (MDB_val){sizeof(target_item), &target_item};
      rc = mdb_put(txn, db->child, &key2, &data2, 0);
      if (rc) {
        fprintf(stderr, "Failed to update child index: %s\n", mdb_strerror(rc));
        goto _si_defer_parent_children;
      }

      rc = mdb_cursor_del(cursor, 0);
      if (rc) {
        fprintf(stderr, "Failed to delete at cursor: %s\n", mdb_strerror(rc));
        goto _si_defer_parent_children;
      }
    }
    mdb_cursor_close(cursor);
    cursor = NULL;
    if (rc != MDB_NOTFOUND) {
      fprintf(stderr, "Failed to move cursor: %s\n", mdb_strerror(rc));
      goto _defer_1;
    }
  _si_defer_parent_children:
    if (cursor)
      mdb_cursor_close(cursor);
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
