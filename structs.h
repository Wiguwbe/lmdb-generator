
#ifndef STRUCTS_H_
#define STRUCTS_H_

struct fk {
	char *table;
	char *field;
};

#define TYPE_INT 1
#define TYPE_ID 2
#define TYPE_CHAR 3
struct type {
	int tt;
	int size;	// for string/char()
};

struct annotation {
	int pk;
	struct fk fk;
};

struct field {
	char *name;
	struct type type;
	struct annotation annotation;
};

struct field_list {
	struct field item;
	struct field_list *next;
};

struct table {
	char *name;
	// list of fields
	struct field_list *fields;
	// pointer to PK
	struct field *pk;
};

struct index {
	char *target;
	char *name;
	char *field;
};

union lval {
	char *str;
	int tt;
	struct type type;
	struct annotation annotation;
	struct field field;
	struct field_list *field_list;
	struct table table;
	struct index_field *indexes;
	struct index index;
};

static inline void print_table(struct table *t)
{
	printf("table '%s' {\n", t->name);
	// fields
	for(struct field_list *fl = t->fields; fl; fl=fl->next)
	{
		struct field *f = &fl->item;
		printf("\t'%s' ", f->name);
		switch(f->type.tt)
		{
		case TYPE_CHAR:
			printf("CHAR(%d)", f->type.size);
			break;
		case TYPE_INT:
			printf("INT");
			break;
		case TYPE_ID:
			printf("ID");
			break;
		}
		// annotations
		if(f->annotation.pk)
			printf(" PK");
		if(f->annotation.fk.table)
			printf(" FK(%s.%s)", f->annotation.fk.table, f->annotation.fk.field);

		if(fl->next)
			printf(",");
		printf("\n");
	}
	printf("}\n\n");
}

static inline void print_index(struct index *i)
{
	printf("index(%s) '%s' {\n\t%s\n}\n\n", i->target, i->name, i->field);
}

#endif
