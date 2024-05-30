#include <stdio.h>
#include <assert.h>

#include <sqlite3.h>

int main(void) {
	sqlite3 *db;
	sqlite3_stmt *stmt;
	
	sqlite3_open("arcface.db", &db);
	assert(0 != db);

	//printf("Performing query...\n");
	sqlite3_prepare_v2(db, "select * from arcface", -1, &stmt, NULL);
	
	//printf("Got results:\n");
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		int i;
		int num_cols = sqlite3_column_count(stmt);
		//printf("num_cols=%d \n", num_cols);

		for (i = 0; i < num_cols; i++) {
			switch (sqlite3_column_type(stmt, i))
			{
			case (SQLITE3_TEXT):
				printf("%s ", sqlite3_column_text(stmt, i));
				break;
			case (SQLITE_INTEGER):
				printf("%d ", sqlite3_column_int(stmt, i));
				break;
			case (SQLITE_FLOAT):
				printf("%g ", sqlite3_column_double(stmt, i));
				break;
			default:
				break;
			}
		}
		printf("\n");

	}

	sqlite3_finalize(stmt);

	sqlite3_close(db);

	//getc(stdin);
	return 0;
}
