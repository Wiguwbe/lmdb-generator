{*
	Template for initialization functions
*}
{#
#include "structs.h"
#}
{$
	struct table *tables;
	int tables_len;
	struct index *indexes;
	int indexes_len;
	char *prefix;
$}
#include <stdio.h>
#include <string.h>
// lmdb is included there
#include "{>s $prefix >}common.h"

// internal structure for initialization
struct _dbi_name
{
	char *name;
	MDB_dbi *dbi;
	unsigned int flags;
};

static int init_pk_table(db *db, MDB_txn *txn);

int db_init(db *db, char *path)
{
	int rc;
	// create environment
	rc = mdb_env_create(&db->env);
	if(rc)
	{
		fprintf(stderr, "Error creating DB environment: %s\n", mdb_strerror(rc));
		return 1;
	}

	rc = mdb_env_set_maxdbs(db->env, (MDB_dbi){>d $tables_len + $indexes_len + 1 >});
	if(rc)
	{
		fprintf(stderr, "Error setting max dbs: %s\n", mdb_strerror(rc));
		return 1;
	}

	rc = mdb_env_set_mapsize(db->env, 1024*1024*10);	// 10m
	if(rc)
	{
		fprintf(stderr, "Error setting map size: %s\n", mdb_strerror(rc));
		return 1;
	}

	rc = mdb_env_open(db->env, path, MDB_NOTLS, 0644);
	if(rc)
	{
		fprintf(stderr, "Error opening DB environment: %s\n", mdb_strerror(rc));
		return 1;
	}

	// databases creation
	MDB_txn *txn;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if(rc)
	{
		fprintf(stderr, "Error beginning write transaction: %s\n", mdb_strerror(rc));
		return 1;
	}
	struct _dbi_name dbi_name_array[] = {
		{% for int i=0;i<$tables_len;i++ %}
			{
				"{>s $tables[i].name >}",
				&db->{>s $tables[i].name >},
				MDB_CREATE|MDB_INTEGERKEY
			},
		{% endfor %}
		{% for int i=0;i<$indexes_len;i++ %}
			{
				"{>s $indexes[i].name >}",
				&db->{>s $indexes[i].name >},
				MDB_CREATE|MDB_INTEGERKEY|MDB_DUPSORT|MDB_INTEGERDUP
			},
		{% endfor %}
		{ "__auto_increment", &db->_auto_increment, MDB_CREATE|MDB_INTEGERKEY },
		{ NULL, NULL, 0 }
	};
	for(struct _dbi_name *dnm = dbi_name_array; dnm->name; dnm++)
	{
		rc = mdb_dbi_open(txn, dnm->name, dnm->flags, dnm->dbi);
		if(rc)
		{
			fprintf(stderr, "Error opening database '%s': %s\n", dnm->name, mdb_strerror(rc));
			mdb_txn_abort(txn);
			return 1;
		}
	}

	rc = init_pk_table(db, txn);
	if(rc)
	{
		fprintf(stderr, "Failed to initialize primary key table: %s\n", mdb_strerror(rc));
		mdb_txn_abort(txn);
		return 1;
	}

	rc = mdb_txn_commit(txn);
	if(rc)
	{
		fprintf(stderr, "Error commiting transaction: %s\n", mdb_strerror(rc));
		mdb_txn_abort(txn);
		return 1;
	}
	return 0;
}

void db_close(db *db)
{
	mdb_env_close(db->env);
}

static int init_pk_table(db *db, MDB_txn *txn)
{
	uint64_t keys[] = {
		{% for int i=0;i<$tables_len;i++ %}
			PK_{>s $tables[i].name >},
		{% endfor %}
	};
	MDB_val key, data;
	uint64_t one = 1;
	int rc;

	for(int i=0;i<{>d $tables_len >};i++)
	{
		key = (MDB_val){ sizeof(uint64_t), (void*)keys+i };
		data = (MDB_val){ sizeof(uint64_t), &one };
		rc = mdb_put(txn, db->_auto_increment, &key, &data, MDB_NOOVERWRITE);
		if(rc == MDB_KEYEXIST || rc == 0)
			continue;
		// else
		return rc;
	}
	return 0;
}

int db_new_pk(db *db, uint64_t table, MDB_txn *txn, uint64_t *out_pk)
{
	MDB_val key = { sizeof(uint64_t), &table };
	MDB_val data;

	int rc = mdb_get(txn, db->_auto_increment, &key, &data);
	if(rc)
	{
		fprintf(stderr, "Failed to get primary key: %s\n", mdb_strerror(rc));
		return rc;
	}
	memcpy(out_pk, data.mv_data, data.mv_size);

	// and now increment
	uint64_t new_key = (*out_pk) + 1;
	key = (MDB_val){ sizeof(uint64_t), &table };
	data = (MDB_val){ sizeof(uint64_t), &new_key };
	rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
	if(rc)
	{
		fprintf(stderr, "Failed to increment primary key: %s\n", mdb_strerror(rc));
		return rc;
	}
	return 0;
}
