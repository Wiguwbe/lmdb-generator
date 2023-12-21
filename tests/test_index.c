
MU_TEST(test_index_put)
{
	db *db = db_global;
	db_child *children;
	size_t child_count;

	// check how many children of 1
	mu_check(get_parent_childrens(db, NULL, (uint64_t)1, &child_count, &children) == 0);

	mu_check(child_count == 3);

	// we're gonna update this later
	db_child child = children[0];
	free(children);

	// check how many children on 2
	mu_check(get_parent_childrens(db, NULL, (uint64_t)2, &child_count, &children) == 0);

	mu_check(child_count == 2);
	free(children);

	// now make a child change parent

	child.parent_id = 2;

	mu_check(put_child(db, NULL, &child) == 0);

	// check number of children again

	// on 1
	mu_check(get_parent_childrens(db, NULL, (uint64_t)1, &child_count, &children) == 0);

	mu_check(child_count == 2);
	free(children);

	// on 2
	mu_check(get_parent_childrens(db, NULL, (uint64_t)2, &child_count, &children) == 0);

	mu_check(child_count == 3);
}

MU_TEST(test_index_delete)
{
	db *db = db_global;
	db_child *children;
	size_t child_count;

	// let's get the children first
	mu_check(get_parent_childrens(db, NULL, (uint64_t)2, &child_count, &children) == 0);
	mu_check(child_count > 0);

	// let's delete the parent (2)
	db_parent parent;
	mu_check(delete_parent(db, NULL, (uint64_t)2, &parent) == 0);

	mu_check(parent.id == 2);

	// we still have the previous children on the array
	db_child *children2;
	size_t child_count2;
	// the children were orphaned
	mu_check(get_parent_childrens(db, NULL, parent.id, &child_count2, &children2) == 0);
	mu_check(child_count2 == 0);

	// now check the children
	for(int i=0;i<child_count;i++)
	{
		db_child child;
		mu_check(get_child(db, NULL, children[i].id, &child) == 0);
		mu_check(child.parent_id == 0);
	}

	free(children);
	free(children2);
}

MU_TEST_SUITE(test_index)
{
	MU_RUN_TEST(test_index_put);
	MU_RUN_TEST(test_index_delete);
}
