# LMDB generator

_LMDB C code generator from a database schema_

This is a C code generator the creates bindings to handle LMDB databases
from a database/table schema.

This is not intended to be a full relational engine/framework/library on
top of LMDB, but rather a quick and easy way to bootstrap the needed
functions to operate on a basic database.

With that said, some relational features are supported.

### Quick intro

From a database schema

```
table person {
  id ID PK,
  name CHAR(64)
}

table card {
  id ID PK,
  owner_id ID FK(person.id),
  number CHAR(32)
}

index(card) person_card {
  owner_id
}
```

This will generate the C structs and the functions

```
struct db {
  // more or less internal
};

struct db_person {
  uint64_t id;
  char name[64];
};

struct db_card {
  uint64_t id;
  uint64_t owner_id;
  char number[32];
};

int list_persons(db *db, MDB_txn *ptxn, size_t *out_len, db_person **out_items);
int get_person(db *db, MDB_txn *ptxn, uint64_t item_id, db_person *out_item);
int put_person(db *db, MDB_txn *ptxn, db_person *in_out_item);
int delete_person(db *db, MDB_txn *ptxn, uint64_t item_id, db_person *out_item)


int list_cards(db *db, MDB_txn *ptxn, size_t *out_len, db_card **out_items);
int get_card(db *db, MDB_txn *ptxn, uint64_t item_id, db_card *out_item);
int put_card(db *db, MDB_txn *ptxn, db_card *in_out_item);
int delete_card(db *db, MDB_txn *ptxn, uint64_t item_id, db_card *out_item)

// and the index function
int get_person_cards(db *db, MDB_txn *ptxn, uint64_t owner_id,
                     size_t *out_len, db_card **out_items);

// and some DB initialization and internal functions
```

## Relations

This tool supports 2 types of relations:
1. `1-m`: with (explicit) indexes;
2. `1-1`: with direct "links".

In general, relations, foreign keys and such are not enforced, but
a few mechanisms are provided to handle them.

### Indexes

As the example above, you can create an index with the `index` keyword.

The format is
```
index(<target_table>) <index_name> {
  <tables_index_field>
}
```

In the example above, it will provide a way to get the list of a person's
cards using the person's id (`owner_id`).

Indexes are updated "automatically" when operations on both ends are
executed:
- Updating the `owner_id` will modify the index;
- Deleting a `person` will remove all related entries on the index
  and will set the respective cards' `owner_id` field to 0 (NULL).

Indexes are not automatically created/deduced and must be defined explicitely

### Links

Links are just 1-1 relations, not illustrated on the example above.

The best example would be a "romantic" relationship, where a person
is "connected" to another person,

Or a country that has only one government/president.

They are implicitely defined, but not enforced.

When an item is deleted, if a link is found (table A has a FK to another
table B, which in turn has a FK to the table A) _AND_ both items point to each
other, then the not deleted item will have said FK field set to 0.

### Caveats

Because the generated code aims to be simple, as said previously,
relations are not completely enforced.

It may be not possible for the generator to find FK's to be updated
(set to 0 when an entry is deleted) without iterating all the items
in a table. A warning is issued at generation time.

## TODO and future work

I think that (generated) functions to handle JSON (de)serialization
would be advantageous for working with webservices or other high level
operations.

The ability to add "embed" capabilites on the JSON functions could
also be useful.

Rename the final binary name.
