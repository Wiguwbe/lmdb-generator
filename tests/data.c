
/*
	partial
*/

struct tuple {
	uint64_t a;
	uint64_t b;
};
typedef struct tuple tuple;

// not much needed here
static db_plain plain_data[] = {
	{1, "Alex", 60},
	{2, "Beatrice", 22},
	{0, "", 0}
};

/*
	for links
*/

static db_link_a links_a[] = {
	// simple 1-1
	{1, 1, 42},
	// the cool guy
	{2, 0, 69},
	// the follower
	{3, 3, 666},

	{0, 0, 0}
};
static db_link_b links_b[] = {
	{1, 1, 43},
	{2, 2, 70},
	{3, 0, 616},

	{0, 0, 0}
};

/*
	for indexes
*/
static db_parent parents[] = {
	{1, 42},
	{2, 69},
	{0, 0}
};
static db_child children[] = {
	{1, 1, 43},
	{2, 1, 44},
	{3, 1, 45},
	{4, 2, 70},
	{5, 2, 71},
	{0, 0, 0}
};
static tuple parent_children[] = {
	{1, 1},
	{1, 2},
	{1, 3},
	{2, 4},
	{2, 5},
	{0, 0}
};
static db_bastard bastards[] = {
	{1, 1, 46},
	{2, 2, 72},
	{0, 0,  0}
};



int populate_db(db *db)
{
	MDB_txn *txn;
	uint64_t last_pk = 0;
	int rc;
	MDB_val key, data;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if(rc)
	{
		fprintf(stderr, "failed to begin transaction: %s\n", mdb_strerror(rc));
		return rc;
	}
	{
		fprintf(stderr, "populating plain\n");
		last_pk = 0;
		for(db_plain *ptr = plain_data; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_plain), ptr};
			rc = mdb_put(txn, db->plain, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_plain;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	{
		fprintf(stderr, "populating link_a\n");
		last_pk = 0;
		for(db_link_a *ptr = links_a; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_link_a), ptr};
			rc = mdb_put(txn, db->link_a, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_link_a;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	{
		fprintf(stderr, "populating link_b\n");
		last_pk = 0;
		for(db_link_b *ptr = links_b; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_link_b), ptr};
			rc = mdb_put(txn, db->link_b, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_link_b;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	{
		fprintf(stderr, "populating parent\n");
		last_pk = 0;
		for(db_parent *ptr = parents; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_parent), ptr};
			rc = mdb_put(txn, db->parent, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_parent;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	{
		fprintf(stderr, "populating child\n");
		last_pk = 0;
		for(db_child *ptr = children; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_child), ptr};
			rc = mdb_put(txn, db->child, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_child;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	{
		fprintf(stderr, "populating parent_children index\n");
		for(tuple *ptr = parent_children; ptr->a; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->a};
			data = (MDB_val){sizeof(uint64_t), &ptr->b};
			rc = mdb_put(txn, db->parent_children, &key, &data, 0);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
		}
	}
	{
		fprintf(stderr, "populating bastard\n");
		last_pk = 0;
		for(db_bastard *ptr = bastards; ptr->id; ptr++)
		{
			key = (MDB_val){sizeof(uint64_t), &ptr->id};
			data = (MDB_val){sizeof(db_bastard), ptr};
			rc = mdb_put(txn, db->bastard, &key, &data, MDB_APPEND);
			if(rc)
			{
				fprintf(stderr, "failed to put item: %s\n", mdb_strerror(rc));
				goto _defer_1;
			}
			last_pk = ptr->id;
		}
		uint64_t pk_key = PK_bastard;
		last_pk ++ ;
		key = (MDB_val){sizeof(pk_key), &pk_key};
		data = (MDB_val){sizeof(last_pk), &last_pk};
		rc = mdb_put(txn, db->_auto_increment, &key, &data, 0);
		if(rc)
		{
			fprintf(stderr, "failed to set auto-increment table: %s\n", mdb_strerror(rc));
			goto _defer_1;
		}
	}
	rc = mdb_txn_commit(txn);
	if(rc)
	{
		fprintf(stderr, "failed to commit transaction: %s\n", mdb_strerror(rc));
		goto _defer_1;
	}
	return 0;
_defer_1:
	mdb_txn_abort(txn);
	return rc;
}
