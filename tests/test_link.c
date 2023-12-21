
/*
	gets should be the same, focus on delete
*/

MU_TEST(test_link_delete_simple)
{
	db *db = db_global;
	db_link_a a;
	db_link_b b;

	mu_check(delete_link_a(db, NULL, (uint64_t)1, &a) == 0);

	mu_check(a.id == 1);
	mu_check(a.link_id == 1);
	mu_check(a.data == 42);

	// b should set to 0

	mu_check(get_link_b(db, NULL, a.link_id, &b) == 0);

	mu_check(b.id == a.link_id);
	mu_check(b.link_id == 0);
	mu_check(b.data == 43);
}

MU_TEST(test_link_delete_zero)
{
	db *db = db_global;
	db_link_a a;
	db_link_b b;

	// delete the "cool guy" (got 0 on the relation)
	mu_check(delete_link_a(db, NULL, (uint64_t)2, &a) == 0);

	mu_check(a.id == 2);
	mu_check(a.link_id == 0);	// always was
	mu_check(a.data == 69);

	// check the friend
	mu_check(get_link_b(db, NULL, (uint64_t)2, &b) == 0);

	mu_check(b.id == 2);
	mu_check(b.link_id == a.id);	// still
	mu_check(b.data == 70);

	// kind of a corner case
}

MU_TEST_SUITE(test_link)
{
	MU_RUN_TEST(test_link_delete_simple);
	MU_RUN_TEST(test_link_delete_zero);
}
