#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/stat.h>

#include "db_common.h"

#include "minunit.h"

static db *db_global = NULL;

#include "data.c"

#include "test_plain.c"
#include "test_link.c"
#include "test_index.c"

#define DB_PATH "db"

int main(int argc, char **argv)
{
	if(mkdir(DB_PATH, 0755))
	{
		fprintf(stderr, "failed to create directory\n");
		return 1;
	}

	// init db
	db db_static;
	// global
	db_global = &db_static;
	db *db = db_global;

	if(db_init(db, DB_PATH))
	{
		fprintf(stderr, "failed to initialize database\n");
		return 1;
	}

	if(populate_db(db))
	{
		fprintf(stderr, "failed to set initial db state\n");
		return 1;
	}

	// run tests
	MU_RUN_SUITE(test_plain);
	MU_RUN_SUITE(test_index);
	MU_RUN_SUITE(test_link);

	MU_REPORT();

	db_close(db);
	return MU_EXIT_CODE;
}
