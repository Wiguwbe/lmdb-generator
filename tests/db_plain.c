

#include "db_common.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int list_plains(db *db, MDB_txn *ptxn, size_t *out_len, db_plain **out_items) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;
  MDB_cursor *cursor;
  MDB_stat stat;
  db_plain *out_data, *data_ptr;

  *out_len = 0;
  *out_items = NULL;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  rc = mdb_stat(txn, db->plain, &stat);
  if (rc) {
    fprintf(stderr, "Failed to get database stat: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }
  out_data = (db_plain *)malloc(stat.ms_entries * sizeof(db_plain));
  if (!out_data) {
    fprintf(stderr, "Failed to allocate memory\n");
    goto _defer_1;
  }
  memset(out_data, 0, stat.ms_entries * sizeof(db_plain));

  rc = mdb_cursor_open(txn, db->plain, &cursor);
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

int get_plain(db *db, MDB_txn *ptxn, uint64_t item_id, db_plain *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->plain, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get item: %s\n", mdb_strerror(rc));
    goto _defer_1;
  }

  memcpy(out_item, data.mv_data, data.mv_size);

_defer_1:
  mdb_txn_abort(txn);

  return rc;
}

int put_plain(db *db, MDB_txn *ptxn, db_plain *in_out_item) {
  MDB_txn *txn = NULL;
  int rc;
  MDB_val key, data;
  db_plain prev;
  uint64_t id = in_out_item->id;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  if (id == 0) {
    // new item
    rc = db_new_pk(db, PK_plain, txn, &id);
    if (rc)
      goto _defer_1;
    memset(&prev, 0, sizeof(prev));
    in_out_item->id = id;
  } else {
    // fetch previous one
    key = (MDB_val){sizeof(uint64_t), &id};
    rc = mdb_get(txn, db->plain, &key, &data);
    if (rc) {
      fprintf(stderr, "Failed to get previous item: %s\n", mdb_strerror(rc));
      goto _defer_1;
    }
    memcpy(&prev, data.mv_data, data.mv_size);
  }

  key = (MDB_val){sizeof(uint64_t), &id};
  data = (MDB_val){sizeof(db_plain), in_out_item};
  rc = mdb_put(txn, db->plain, &key, &data, 0);
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

int delete_plain(db *db, MDB_txn *ptxn, uint64_t item_id, db_plain *out_item) {
  MDB_txn *txn;
  int rc;
  MDB_val key, data;

  rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
  if (rc) {
    fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
    return rc;
  }

  key = (MDB_val){sizeof(uint64_t), &item_id};
  rc = mdb_get(txn, db->plain, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to get original from database: %s\n",
            mdb_strerror(rc));
    goto _defer_1;
  }
  memcpy(out_item, data.mv_data, data.mv_size);

  rc = mdb_del(txn, db->plain, &key, &data);
  if (rc) {
    fprintf(stderr, "Failed to delete item from database: %s\n",
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
