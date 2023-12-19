{*
	structs and function definitions
*}
{#
#include <string.h>
#include "structs.h"
#}
{$
	struct table *tables;
	int tables_len;
	struct index *indexes;
	int indexes_len;
$}

#ifndef _DB_STRUCTS_H
#define _DB_STRUCTS_H

#include <lmdb.h>

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

// for auto increment table
{% for int i=0;i<$tables_len;i++ %}
#define PK_{>s $tables[i].name >} {>d i+1 >}
{% endfor %}

{*
	base structs
*}
{% for int i=0;i<$tables_len; i++ %}
	{{ struct table *t = $tables+i; }}
	struct db_{>s t->name >} {
		{% for struct field_list *fl = t->fields; fl; fl=fl->next %}
			{{ struct field *f = &fl->item; }}
			{>s f->type.tt == TYPE_INT ? "int" : (f->type.tt == TYPE_ID ? "uint64_t" : "char")
			>} {>s f->name >}{% if f->type.tt == TYPE_CHAR %}[{>d f->type.size >}]{% fi %};
		{% endfor %}
	};
	typedef struct db_{>s t->name >} db_{>s t->name >};
{% endfor %}

{*
	"internal" DBIs struct
*}
struct db
{
	MDB_env *env;
	// primary databases
	{% for int i=0;i<$tables_len;i++ %}
		MDB_dbi {>s $tables[i].name >};
	{% endfor %}
	// secondary indexes
	{% for int i=0;i<$indexes_len;i++ %}
		MDB_dbi {>s $indexes[i].name >};
	{% endfor %}
	// auto increment table
	MDB_dbi _auto_increment;
};
typedef struct db db;

{*
	base functions
	(list, get, put, delete)
*}
{% for int i=0;i<$tables_len; i++ %}
	{{ struct table *t = $tables+i; }}
	int list_{>s t->name >}s(db *db, MDB_txn *ptxn, size_t *out_len, db_{>s t->name >} **out_items);
	int get_{>s t->name >}(db *db, MDB_txn *ptxn, uint64_t item_id, db_{>s t->name >} *out_item);
	int put_{>s t->name >}(db *db, MDB_txn *ptxn, db_{>s t->name >} *in_out_item);
	int delete_{>s t->name >}(db *db, MDB_txn *ptxn, uint64_t item_id, db_{>s t->name >} *out_item);
{% endfor %}

{*
	secondary index functions
	get child items from parent
	<parent>_<item>s
*}
{% for int i=0;i<$indexes_len; i++ %}
	{{ struct index *n = $indexes+i; }}
	int get_{>s n->name >}s(db *db, MDB_txn *ptxn, uint64_t parent_id, size_t *out_len, db_{>s n->target >} **out_items);
{% endfor %}

{*
	initialization function
*}
int db_init(db *db, char *path);
void db_close(db *db);

// for auto-increment primary keys
int db_new_pk(db *db, uint64_t table, MDB_txn *txn, uint64_t *out_pk);

#endif
