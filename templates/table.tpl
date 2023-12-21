{*
	functions for table operations
	(and associated indexes)
*}
{#
#include <string.h>
#include "structs.h"
#}
{$
	struct table *table;
	struct table *tables;
	int tables_len;
	struct index *indexes;
	int indexes_len;
	char *prefix;
$}

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "{>s $prefix >}common.h"

int list_{>s $table->name >}s(db *db, MDB_txn *ptxn, size_t *out_len, db_{>s $table->name >} **out_items)
{
	MDB_txn *txn;
	int rc;
	MDB_val key, data;
	MDB_cursor *cursor;
	MDB_stat stat;
	db_{>s $table->name >} *out_data, *data_ptr;

	*out_len = 0;
	*out_items = NULL;

	rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
	if(rc)
	{
		fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
		return rc;
	}
	{* defer mdb_txn_abort(txn) *}

	rc = mdb_stat(txn, db->{>s $table->name >}, &stat);
	if(rc)
	{
		fprintf(stderr, "Failed to get database stat: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}
	out_data = (db_{>s $table->name >}*)malloc(stat.ms_entries * sizeof(db_{>s $table->name >}));
	if(!out_data)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		goto _defer_1;
	}
	memset(out_data, 0, stat.ms_entries * sizeof(db_{>s $table->name >}));

	rc = mdb_cursor_open(txn, db->{>s $table->name >}, &cursor);
	if(rc)
	{
		fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
		free(out_data);
		goto _defer_1;
	}
	{* defer mdb_cursor_close(cursor); *}
	rc = mdb_cursor_get(cursor, &key, &data, MDB_FIRST);
	if(rc == MDB_NOTFOUND)
	{
		// empty (not error)
		rc = 0;
		goto _defer_2;
	}
	if(rc)
	{
		fprintf(stderr, "Error setting cursor: %s\n", mdb_strerror(rc));
		free(out_data);
		goto _defer_2;
	}

	data_ptr = out_data;
	while(1)
	{
		memcpy(data_ptr, data.mv_data, data.mv_size);

		// next
		data_ptr++;
		rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT);
		if(rc == 0)
			continue;
		if(rc == MDB_NOTFOUND)
		{
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

int get_{>s $table->name >}(db *db, MDB_txn *ptxn, uint64_t item_id, db_{>s $table->name >} *out_item)
{
	MDB_txn *txn;
	int rc;
	MDB_val key, data;

	rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
	if(rc)
	{
		fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
		return rc;
	}
	{* defer mdb_txn_abort(txn) *}

	key = (MDB_val){sizeof(uint64_t), &item_id};
	rc = mdb_get(txn, db->{>s $table->name >}, &key, &data);
	if(rc)
	{
		fprintf(stderr, "Failed to get item: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}

	memcpy(out_item, data.mv_data, data.mv_size);

_defer_1:
	mdb_txn_abort(txn);

	return rc;
}

int put_{>s $table->name >}(db *db, MDB_txn *ptxn, db_{>s $table->name >} *in_out_item)
{
	MDB_txn *txn = NULL;
	int rc;
	MDB_val key, data;
	db_{>s $table->name >} prev;
	uint64_t id = in_out_item->{>s $table->pk->name >};

	rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
	if(rc)
	{
		fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
		return rc;
	}
	{* defer if(txn) mdb_txn_abort(txn); *}

	if(id == 0)
	{
		// new item
		rc = db_new_pk(db, PK_{>s $table->name >}, txn, &id);
		if(rc)
			goto _defer_1;
		memset(&prev, 0, sizeof(prev));
		in_out_item->{>s $table->pk->name >} = id;
	}
	else
	{
		// fetch previous one
		key = (MDB_val){ sizeof(uint64_t), &id };
		rc = mdb_get(txn, db->{>s $table->name >}, &key, &data);
		if(rc)
		{
			fprintf(stderr, "Failed to get previous item: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
		memcpy(&prev, data.mv_data, data.mv_size);
	}

	key = (MDB_val){sizeof(uint64_t), &id};
	data = (MDB_val){sizeof(db_{>s $table->name >}), in_out_item};
	rc = mdb_put(txn, db->{>s $table->name >}, &key, &data, 0);
	if(rc)
	{
		fprintf(stderr, "Failed to insert item: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}

	// secondary indexes
	{% for int i=0;i<$indexes_len;i++ %}
		{{ struct index *ndx = $indexes + i; }}
		{% if strcmp($table->name, ndx->target) %}
			{{ continue; }}
		{% fi %}
		if(prev.{>s ndx->field >} != in_out_item->{>s ndx->field >})
		{
			if(prev.{>s ndx->field >} != 0)
			{
				key = (MDB_val){sizeof(uint64_t), &prev.{>s ndx->field >}};
				data = (MDB_val){sizeof(uint64_t), &prev.{>s $table->pk->name >}};
				rc = mdb_del(txn, db->{>s ndx->name >}, &key, &data);
				if(rc)
				{
					fprintf(stderr, "Failed to update secondary index: %s\n", mdb_strerror(rc));
					goto _defer_1;
				}
			}
			if(in_out_item->{>s ndx->field >} != 0)
			{
				key = (MDB_val){sizeof(uint64_t), &in_out_item->{>s ndx->field >}};
				data = (MDB_val){sizeof(uint64_t), &in_out_item->{>s $table->pk->name >}};
				rc = mdb_put(txn, db->{>s ndx->name >}, &key, &data, 0);
				if(rc)
				{
					fprintf(stderr, "Failed to update secondary index: %s\n", mdb_strerror(rc));
					goto _defer_1;
				}
			}
		}
	{% endfor %}

	{*
		TODO do LINKs
		(see below on delete_*)
		if `this_table.FK(other_table)` changed:
			set `other_table(prev.FK(other_table))` to 0
			set `other_table(cur.FK(other_table))` to cur.id
	*}

	rc = mdb_txn_commit(txn);
	if(rc)
	{
		fprintf(stderr, "Failed to commit transaction: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}
	txn = NULL;

_defer_1:
	if(txn)
		mdb_txn_abort(txn);

	return rc;
}

int delete_{>s $table->name >}(db *db, MDB_txn *ptxn, uint64_t item_id, db_{>s $table->name >} *out_item)
{
	MDB_txn *txn;
	int rc;
	MDB_val key, data;

	rc = mdb_txn_begin(db->env, ptxn, 0, &txn);
	if(rc)
	{
		fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
		return rc;
	}
	{* defer if(txn) mdb_txn_abort(txn) *}

	key = (MDB_val){sizeof(uint64_t), &item_id};
	rc = mdb_get(txn, db->{>s $table->name >}, &key, &data);
	if(rc)
	{
		fprintf(stderr, "Failed to get original from database: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}
	memcpy(out_item, data.mv_data, data.mv_size);

	rc = mdb_del(txn, db->{>s $table->name >}, &key, &data);
	if(rc)
	{
		fprintf(stderr, "Failed to delete item from database: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}

	{*
		TODO delete secondary indexes
	*}
	{% for int i=0;i<$indexes_len;i++ %}
		{{ struct index *ndx = $indexes+i; }}
		{% if strcmp(ndx->target, $table->name) %}{{ continue; }}{% fi %}
		key = (MDB_val){sizeof(uint64_t), &out_item->{>s ndx->field >}};
		data = (MDB_val){sizeof(uint64_t), &out_item->{>s $table->pk->name >}};
		rc = mdb_del(txn, db->{>s ndx->name >}, &key, &data);
		if(rc)
		{
			fprintf(stderr, "Failed to update secondary indexes: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	{% endfor %}

	{*
		TODO set 0 on FK
		for each `other_table` which has FK to `this_table`:
			if `this_table` also has FK to `other_table`:
				// it's a "LINK" (1-1 relation)
				set `other_table.FK(this_table)` = 0
			else:
				// there should be an index with `target (other_table)`
				// AND field `other_table.FK(this_table)`
				// it's a CHILD (1-m relation)
				find `other_table.id` from index
	*}
	{% for int i=0;i<$tables_len;i++ %}
		{{ struct table *other_table = $tables+i; }}
		{% for struct field_list *o_fl=other_table->fields;o_fl;o_fl=o_fl->next %}
			{% if o_fl->item.annotation.fk.table == NULL %}
				{{ continue; }}
			{% fi %}
			{% if strcmp(o_fl->item.annotation.fk.table, $table->name) %}
				{{ continue; }}
			{% fi %}
			{* got an FK for this table *}
			{{ int has_link = 0; }}
			{* check if it's a LINK *}
			{% for struct field_list *m_fl = $table->fields;m_fl;m_fl=m_fl->next %}
				{% if m_fl->item.annotation.fk.table == NULL %}
					{{continue;}}
				{%fi%}
				{% if strcmp(m_fl->item.annotation.fk.table, other_table->name) %}
					{{continue;}}
				{%fi%}
				{{ has_link = 1; }}
				if(out_item->{>s m_fl->item.name >} != 0)
				{
					key = (MDB_val){sizeof(uint64_t), &out_item->{>s m_fl->item.name >}};
					rc = mdb_get(txn, db->{>s other_table->name >}, &key, &data);
					if(rc)
					{
						fprintf(stderr, "Failed to update {>s other_table->name >}s: %s\n", mdb_strerror(rc));
						goto _defer_1;
					}
					db_{>s other_table->name >} link_item;
					memcpy(&link_item, data.mv_data, data.mv_size);
					if(link_item.{>s o_fl->item.name >} == out_item->{>s m_fl->item.name >})
					{
						link_item.{>s o_fl->item.name >} = 0;
						key = (MDB_val){sizeof(uint64_t), &link_item.{>s other_table->pk->name >}};
						data = (MDB_val){sizeof(db_{>s other_table->name >}), &link_item};
						rc = mdb_put(txn, db->{>s other_table->name >}, &key, &data, 0);
						if(rc)
						{
							fprintf(stderr, "Failed to update {>s other_table->name >}s: %s\n", mdb_strerror(rc));
							goto _defer_1;
						}
					}
				}
				{{ break; }}
			{% endfor %}
			{% if has_link %}
				{{ continue; }}
			{% fi %}
			{* otherwise, no link, look for indexes *}
			{{ int has_index = 0; }}
			{% for int i=0;i<$indexes_len;i++ %}
				{{ struct index *ndx = $indexes+i; }}
				{% if strcmp(ndx->target, other_table->name) %}
					{{ continue; }}
				{% fi %}
				{% if strcmp(ndx->field, o_fl->item.name) %}
					{{ continue; }}
				{% fi %}
				{* found it *}
				{{ has_index = 1; }}
				{
					MDB_cursor *cursor;
					rc = mdb_cursor_open(txn, db->{>s ndx->name >}, &cursor);
					if(rc)
					{
						fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
						goto _defer_1;
					}
					{* defer if(cursor) cursor.close() *}
					key = (MDB_val){sizeof(uint64_t), &out_item->{>s $table->pk->name >}};

					while((rc = mdb_cursor_get(cursor, &key, &data, MDB_SET)) == 0)
					{
						MDB_val key2 = {sizeof(uint64_t), data.mv_data };
						MDB_val data2;
						rc = mdb_get(txn, db->{>s ndx->target >}, &key2, &data2);
						if(rc)
						{
							fprintf(stderr, "Failed to update child index: %s\n", mdb_strerror(rc));
							goto _si_defer_{>s ndx->name >};
						}
						db_{>s ndx->target >} target_item;
						memcpy(&target_item, data2.mv_data, data2.mv_size);
						target_item.{>s ndx->field >} = 0;
						data2 = (MDB_val){sizeof(target_item), &target_item};
						rc = mdb_put(txn, db->{>s ndx->target >}, &key2, &data2, 0);
						if(rc)
						{
							fprintf(stderr, "Failed to update child index: %s\n", mdb_strerror(rc));
							goto _si_defer_{>s ndx->name >};
						}

						{* TODO delete at cursor *}
						rc = mdb_cursor_del(cursor, 0);
						if(rc)
						{
							fprintf(stderr, "Failed to delete at cursor: %s\n", mdb_strerror(rc));
							goto _si_defer_{>s ndx->name >};
						}
					}
					mdb_cursor_close(cursor);
					cursor = NULL;
					if(rc != MDB_NOTFOUND)
					{
						fprintf(stderr, "Failed to move cursor: %s\n", mdb_strerror(rc));
						goto _defer_1;
					}
				_si_defer_{>s ndx->name >}:
					if(cursor)
						mdb_cursor_close(cursor);
				}
				{{ break; }}
			{% endfor %}
			{% if !has_index %}
				{{ fprintf(stderr, "unable to update %s.%s at %s removal\n", other_table->name, o_fl->item.name, $table->name); }}
			{% fi %}
		{% endfor %}
	{% endfor %}
	rc = mdb_txn_commit(txn);
	if(rc)
	{
		fprintf(stderr, "Failed to commit transaction: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}
	txn = NULL;
_defer_1:
	if(txn)
		mdb_txn_abort(txn);
	return rc;
}

{* TODO index functions *}

{% for int i=0;i<$indexes_len;i++ %}
	{{ struct index *ndx = $indexes+i; }}
	{% if strcmp(ndx->target, $table->name) %}
		{{continue;}}
	{% fi %}
	int get_{>s ndx->name >}s(db *db, MDB_txn *ptxn, uint64_t parent_id, size_t *out_len, db_{>s $table->name >} **out_items)
	{
		MDB_txn *txn;
		MDB_cursor *cursor;
		int rc;
		MDB_val key, data;
		db_{>s $table->name >} *out_array;

		*out_len = 0;
		*out_items = NULL;

		rc = mdb_txn_begin(db->env, ptxn, MDB_RDONLY, &txn);
		if(rc)
		{
			fprintf(stderr, "Failed to begin transaction: %s\n", mdb_strerror(rc));
			return rc;
		}
		{* defer mdb_txn_abort(txn) *}

		rc = mdb_cursor_open(txn, db->{>s ndx->name >}, &cursor);
		if(rc)
		{
			fprintf(stderr, "Failed to open cursor: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
		{* defer mdb_cursor_close(cursor) *}
		key = (MDB_val){sizeof(uint64_t), &parent_id};
		rc = mdb_cursor_get(cursor, &key, &data, MDB_SET);
		if(rc == MDB_NOTFOUND)
		{
			rc = 0;
			goto _defer_2;
		}
		if(rc)
		{
			fprintf(stderr, "Failed to position cursor: %s\n", mdb_strerror(rc));
			goto _defer_2;
		}

		rc = mdb_cursor_count(cursor, out_len);
		if(rc)
		{
			fprintf(stderr, "Failed to get cursor count: %s\n", mdb_strerror(rc));
			goto _defer_2;
		}

		out_array = (db_{>s $table->name >}*)malloc(sizeof(db_{>s $table->name>}) * (*out_len));
		if(!out_array)
		{
			rc = ENOMEM;
			fprintf(stderr, "Failed to allocate memory\n");
			goto _defer_2;
		}

		db_{>s $table->name >} *data_ptr = out_array;
		for(int i=0;i<*out_len;i++)
		{
			uint64_t item_id = *((uint64_t*)data.mv_data);
			// NULL as this is a read transaction
			rc = get_{>s $table->name >}(db, NULL, item_id, out_array+i);
			if(rc)
			{
				free(out_array);
				goto _defer_2;
			}
			rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT_DUP);
			if(rc == 0)
				continue;
			if((rc == MDB_NOTFOUND && i < ((*out_len)-1)) || rc != MDB_NOTFOUND)
			{
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
{% endfor %}
