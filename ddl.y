%{
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "structs.h"
#include "cli.h"

#include "templates/tpl-common.h"
#include "templates/tpl-init.h"
#include "templates/tpl-table.h"

static struct table *tables = NULL;
static int tables_len = 0;
static struct index *indexes = NULL;
static int indexes_len = 0;

static struct gengetopt_args_info args_info;

void add_field(struct field_list *fl, struct field field);
void add_table(struct table *table);
void add_index(struct index *index);

void generate(void);

int yyerror(char*);
extern int yylex(void);

extern FILE *yyin;

%}

%token KW_TABLE
%token KW_PK
%token KW_FK
%token KW_INDEX
%token TY_ID
%token TY_INT
%token TY_CHAR
%token OP_PAREN
%token CL_PAREN
%token OP_BRACE
%token CL_BRACE
%token DOT
%token COMMA
%token IDENTIFIER
%token NUMBER

%define api.value.type { union lval }
%start file

%%

file:
	tables YYEOF	{ generate(); }

tables:
	table	{ add_table(&$1.table); }
|	index	{ add_index(&$1.index); }
|	tables table	{ add_table(&$2.table); }
|	tables index	{ add_index(&$2.index); }
;

table:
	KW_TABLE IDENTIFIER OP_BRACE table-definition CL_BRACE
		{
			$$.table.name = $2.str;
			$$.table.fields = $4.field_list;
			$$.table.pk = NULL;
		}
;

index:
	KW_INDEX OP_PAREN IDENTIFIER CL_PAREN IDENTIFIER OP_BRACE index-definition CL_BRACE
	{
		$$.index.target = $3.str;
		$$.index.name = $5.str;
		$$.index.field = $7.str;
	}
;

table-definition:
	field	{
		struct field_list *fl = malloc(sizeof(struct field_list));
		fl->item = $1.field;
		fl->next = NULL;
		$$.field_list = fl;
	}
|	table-definition COMMA field	{
		add_field($$.field_list, $3.field);
	}
;

index-definition:
	IDENTIFIER	{ $$.str = $1.str; }
;

field:
	IDENTIFIER type annotations
	{
		$$.field.name = $1.str;
		$$.field.type = $2.type;
		$$.field.annotation = $3.annotation;
	}
;

type:
	TY_INT	{ $$.type.tt = TYPE_INT; }
|	TY_ID	{ $$.type.tt = TYPE_ID; }
|	TY_CHAR OP_PAREN NUMBER CL_PAREN
	{
		$$.type.tt = TYPE_CHAR;
		$$.type.size = atoi($3.str);
	}
;

annotations:
	%empty
	{
		$$.annotation.pk = 0;
		$$.annotation.fk.table = $$.annotation.fk.field = NULL;
	}
|	annotations KW_PK	{ $$.annotation.pk = 1; }
|	annotations KW_FK OP_PAREN IDENTIFIER DOT IDENTIFIER CL_PAREN
	{
		$$.annotation.fk.table = $4.str;
		$$.annotation.fk.field = $6.str;
	}
;

%%

int main(int argc, char**argv)
{
	// global args_info
	if(cmdline_parser(argc, argv, &args_info))
	{
		fprintf(stderr, "failed to parse command line arguments\n");
		return 1;
	}
	if(args_info.inputs_num > 0)
	{
		if(args_info.inputs_num > 1)
			fprintf(stderr, "multiple input files, using only '%s'\n", args_info.inputs[0]);
		FILE *in_file = fopen(args_info.inputs[0], "r");
		if(!in_file)
		{
			fprintf(stderr, "failed to open '%s'\n", args_info.inputs[0]);
			return 1;
		}
		yyin = in_file;
	}
	yyparse();
	return 0;
}

int yyerror(char *s)
{
	fprintf(stderr, "error: %s\n", s);
	return 0;
}

void add_field(struct field_list *fl, struct field field)
{
	// TODO check if PK/FK and not ID type
	while(fl->next)
		fl = fl->next;
	struct field_list *ni = (struct field_list*)malloc(sizeof(struct field_list));
	if(!ni)
	{
		fprintf(stderr, "failed to allocate memory\n");
		exit(1);
	}
	fl->next = ni;
	ni->item = field;
	ni->next = NULL;
}

void add_table(struct table *table)
{
	print_table(table);
	void *np = realloc(tables, (tables_len+1)*sizeof(struct table));
	if(!np)
	{
		fprintf(stderr, "failed to allocate memory\n");
		exit(1);
	}
	tables = (struct table*)np;
	memcpy(tables+tables_len, table, sizeof(struct table));
	// a new pointer
	table = tables+tables_len;
	tables_len++;
	// check for primary key
	{
		struct field *field_id = NULL;
		struct field *field_pk = NULL;
		for(struct field_list *fl = table->fields; fl; fl=fl->next)
		{
			if(fl->item.annotation.pk)
				field_pk = &fl->item;
			if(!strcmp(fl->item.name, "id"))
				field_pk = &fl->item;
		}
		// PK takes precedence
		if(!field_pk)
			field_pk = field_id;
		if(!field_pk)
		{
			fprintf(stderr, "No primary key defined for table '%s'\n", table->name);
			exit(1);
		}
		table->pk = field_pk;
	}
}

void add_index(struct index *ndx)
{
	print_index(ndx);
	void *np = realloc(indexes, (indexes_len+1)*sizeof(struct index));
	if(!np)
	{
		fprintf(stderr, "failed to allocate memory\n");
		exit(1);
	}
	indexes = (struct index*)np;
	memcpy(indexes+indexes_len, ndx, sizeof(struct index));
	indexes_len++;
}

static void gen_template(char *fname, int(*caller)(FILE*, void*), void*udata)
{
	int fds[2];
	if(pipe(fds))
	{
		fprintf(stderr, "failed to pipe\n");
		exit(1);
	}
	int pipe_rd = fds[0];
	int pipe_wr = fds[1];

	pid_t child_pid = fork();
	if(child_pid < 0)
	{
		fprintf(stderr, "failed to fork\n");
		exit(1);
	}
	if(child_pid == 0)
	{
		close(pipe_wr);
		int out_fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if(out_fd < 0)
		{
			fprintf(stderr, "failed to open/create '%s'\n", fname);
			exit(1);
		}
		if(dup2(pipe_rd, 0) < 0 || dup2(out_fd, 1) < 0)
		{
			fprintf(stderr, "failed to dup2\n");
			exit(1);
		}
		execlp("clang-format", "clang-format", NULL);
		fprintf(stderr, "failed to exec\n");
		exit(1);
	}
	// else, on parent
	close(pipe_rd);
	FILE* pipe_wr_file = fdopen(pipe_wr, "w");
	if(caller(pipe_wr_file, udata))
	{
		fprintf(stderr, "failed to generate template\n");
		exit(1);	// open fds will be closed
	}
	fclose(pipe_wr_file);	// close with fclose to flush output
	int child_status;
	if(waitpid(child_pid, &child_status, 0) < 0)
	{
		fprintf(stderr, "failed to wait for child\n");
		exit(1);
	}
	// check child status
	if(WEXITSTATUS(child_status))
	{
		fprintf(stderr, "child exited with non-zero\n");
		exit(1);
	}
	// otherwise, all good
}

static int tpl_common(FILE *f, void* _unused)
{
	struct common_ctx ctx = {
		tables, tables_len,
		indexes, indexes_len
	};
	return common_gen(f, &ctx);
}

static int tpl_init(FILE *f, void *_unused)
{
	struct init_ctx ctx = {
		tables, tables_len,
		indexes, indexes_len
	};
	return init_gen(f, &ctx);
}

static int tpl_table(FILE *f, void *ptr_table)
{
	struct table_ctx ctx = {
		(struct table*)ptr_table,
		tables, tables_len,
		indexes, indexes_len
	};
	return table_gen(f, &ctx);
}

void generate(void)
{
	// check that all indexes point to a table
	for(int i=0;i<indexes_len;i++)
	{
		int found_table = 0;
		struct index *ndx = indexes+i;
		for(int j=0;j<tables_len;j++)
		{
			struct table *t = tables+j;
			if(strcmp(t->name, ndx->target))
				continue;
			found_table = 1;
			int found_field = 0;
			// check field exists
			for(struct field_list *fl = t->fields; fl; fl=fl->next)
			{
				if(strcmp(fl->item.name, ndx->field))
					continue;
				found_field = 1;
				break;
			}
			if(!found_field)
			{
				fprintf(stderr, "Index '%s' field is not on table '%s'\n", ndx->name, ndx->target);
				exit(1);
			}
			break;
		}
		if(!found_table)
		{
			fprintf(stderr, "Index '%s' has invalid target, not table named '%s'\n", ndx->name, ndx->target);
			exit(1);
		}
	}
	char *fname;

	if(asprintf(&fname, "%s/common.h", args_info.output_dir_arg) < 0)
	{
		fprintf(stderr, "failed to allocate memory\n");
		exit(1);
	}
	gen_template(fname, tpl_common, NULL);
	free(fname);

	if(asprintf(&fname, "%s/init.c", args_info.output_dir_arg) < 0)
	{
		fprintf(stderr, "failed to allocate memory\n");
		exit(1);
	}
	gen_template(fname, tpl_init, NULL);
	free(fname);
	for(int i=0;i<tables_len;i++)
	{
		if(asprintf(&fname, "out/%s.c", tables[i].name) < 0)
		{
			fprintf(stderr, "failed to allocate memory\n");
			exit(1);
		}
		gen_template(fname, tpl_table, tables+i);
		free(fname);
	}
	// all good
}
