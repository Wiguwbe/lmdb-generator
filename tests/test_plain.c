
/*
	Partial
*/

MU_TEST(test_plain_get)
{
	db *db = db_global;
	db_plain item;

	mu_check(get_plain(db, NULL, (uint64_t)1, &item) == 0);

	mu_check(item.id == 1);
	mu_assert_string_eq(item.name, "Alex");
	mu_check(item.age == 60);

	mu_check(get_plain(db, NULL, (uint64_t)2, &item) == 0);

	mu_check(item.id == 2);
	mu_assert_string_eq(item.name, "Beatrice");
	mu_check(item.age == 22);
}

MU_TEST(test_plain_list)
{
	db *db = db_global;
	db_plain *items;
	size_t item_count;

	mu_check(list_plains(db, NULL, &item_count, &items) == 0);

	mu_check(item_count == 2);

	mu_check(items[0].id == 1);
	mu_assert_string_eq(items[0].name, "Alex");
	mu_check(items[0].age == 60);

	mu_check(items[1].id == 2);
	mu_assert_string_eq(items[1].name, "Beatrice");
	mu_check(items[1].age == 22);

	free(items);
}

MU_TEST(test_plain_put)
{
	db *db = db_global;
	db_plain new_item = {0, "Charles", 99};

	// insert
	mu_check(put_plain(db, NULL, &new_item) == 0);

	mu_check(new_item.id == 3);

	// update age
	new_item.age = 100;
	mu_check(put_plain(db, NULL, &new_item) == 0);

	mu_check(new_item.id == 3);
	mu_assert_string_eq(new_item.name, "Charles");
	mu_check(new_item.age == 100);

	// fetch
	db_plain fetched;
	mu_check(get_plain(db, NULL, (uint64_t)3, &fetched) == 0);

	mu_check(fetched.id == 3);
	mu_assert_string_eq(fetched.name, "Charles");
	mu_check(fetched.age == 100);
}

MU_TEST(test_plain_delete)
{
	db *db = db_global;
	db_plain old_item;

	mu_check(delete_plain(db, NULL, (uint64_t)3, &old_item) == 0);

	mu_check(old_item.id == 3);
	mu_assert_string_eq(old_item.name, "Charles");
	mu_check(old_item.age == 100);

	// list and get count
	db_plain *arr;
	size_t count;
	mu_check(list_plains(db, NULL, &count, &arr) == 0);
	mu_check(count == 2);

	free(arr);
}

MU_TEST_SUITE(test_plain)
{
	MU_RUN_TEST(test_plain_get);
	MU_RUN_TEST(test_plain_list);
	MU_RUN_TEST(test_plain_put);
	MU_RUN_TEST(test_plain_delete);
}

