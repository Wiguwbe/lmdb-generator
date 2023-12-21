

#ifndef _DB_STRUCTS_H
#define _DB_STRUCTS_H

#include <lmdb.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// for auto increment table

#define PK_plain 1

#define PK_link_a 2

#define PK_link_b 3

#define PK_parent 4

#define PK_child 5

#define PK_bastard 6

struct db_plain {

  uint64_t id;

  char name[16];

  int age;
};
typedef struct db_plain db_plain;

struct db_link_a {

  uint64_t id;

  uint64_t link_id;

  int data;
};
typedef struct db_link_a db_link_a;

struct db_link_b {

  uint64_t id;

  uint64_t link_id;

  int data;
};
typedef struct db_link_b db_link_b;

struct db_parent {

  uint64_t id;

  int data;
};
typedef struct db_parent db_parent;

struct db_child {

  uint64_t id;

  uint64_t parent_id;

  int data;
};
typedef struct db_child db_child;

struct db_bastard {

  uint64_t id;

  uint64_t parent_id;

  int data;
};
typedef struct db_bastard db_bastard;

struct db {
  MDB_env *env;
  // primary databases

  MDB_dbi plain;

  MDB_dbi link_a;

  MDB_dbi link_b;

  MDB_dbi parent;

  MDB_dbi child;

  MDB_dbi bastard;

  // secondary indexes

  MDB_dbi parent_children;

  // auto increment table
  MDB_dbi _auto_increment;
};
typedef struct db db;

int list_plains(db *db, MDB_txn *ptxn, size_t *out_len, db_plain **out_items);
int get_plain(db *db, MDB_txn *ptxn, uint64_t item_id, db_plain *out_item);
int put_plain(db *db, MDB_txn *ptxn, db_plain *in_out_item);
int delete_plain(db *db, MDB_txn *ptxn, uint64_t item_id, db_plain *out_item);

int list_link_as(db *db, MDB_txn *ptxn, size_t *out_len, db_link_a **out_items);
int get_link_a(db *db, MDB_txn *ptxn, uint64_t item_id, db_link_a *out_item);
int put_link_a(db *db, MDB_txn *ptxn, db_link_a *in_out_item);
int delete_link_a(db *db, MDB_txn *ptxn, uint64_t item_id, db_link_a *out_item);

int list_link_bs(db *db, MDB_txn *ptxn, size_t *out_len, db_link_b **out_items);
int get_link_b(db *db, MDB_txn *ptxn, uint64_t item_id, db_link_b *out_item);
int put_link_b(db *db, MDB_txn *ptxn, db_link_b *in_out_item);
int delete_link_b(db *db, MDB_txn *ptxn, uint64_t item_id, db_link_b *out_item);

int list_parents(db *db, MDB_txn *ptxn, size_t *out_len, db_parent **out_items);
int get_parent(db *db, MDB_txn *ptxn, uint64_t item_id, db_parent *out_item);
int put_parent(db *db, MDB_txn *ptxn, db_parent *in_out_item);
int delete_parent(db *db, MDB_txn *ptxn, uint64_t item_id, db_parent *out_item);

int list_childs(db *db, MDB_txn *ptxn, size_t *out_len, db_child **out_items);
int get_child(db *db, MDB_txn *ptxn, uint64_t item_id, db_child *out_item);
int put_child(db *db, MDB_txn *ptxn, db_child *in_out_item);
int delete_child(db *db, MDB_txn *ptxn, uint64_t item_id, db_child *out_item);

int list_bastards(db *db, MDB_txn *ptxn, size_t *out_len,
                  db_bastard **out_items);
int get_bastard(db *db, MDB_txn *ptxn, uint64_t item_id, db_bastard *out_item);
int put_bastard(db *db, MDB_txn *ptxn, db_bastard *in_out_item);
int delete_bastard(db *db, MDB_txn *ptxn, uint64_t item_id,
                   db_bastard *out_item);

int get_parent_childrens(db *db, MDB_txn *ptxn, uint64_t parent_id,
                         size_t *out_len, db_child **out_items);

int db_init(db *db, char *path);
void db_close(db *db);

// for auto-increment primary keys
int db_new_pk(db *db, uint64_t table, MDB_txn *txn, uint64_t *out_pk);

#endif
