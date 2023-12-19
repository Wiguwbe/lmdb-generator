
.SUFFIXES:

TEMPLATES := $(wildcard templates/*.tpl)
H_TEMPLATES := $(TEMPLATES:templates/%.tpl=templates/tpl-%.h)

ddl: ddl.o lex.yy.o cli.o templates/templates.o
	gcc -o $@ $^ -ggdb

ddl.o: ddl.c $(H_TEMPLATES) structs.h cli.h
	gcc -c $< -ggdb

cli.o: cli.c cli.h
	gcc -c $<

ddl.c: ddl.y
	bison -d -o $@ $<

lex.yy.o: lex.yy.c
	gcc -c $< -ggdb

lex.yy.c: ddl.l
	flex $<

cli.c: cli.ggo
	gengetopt -i $<

cli.h: cli.ggo
	gengetopt -i $<

templates/templates.o: $(TEMPLATES)
	make -C templates

templates/tpl-%.h: $(TEMPLATES)
	make -C templates

clean:
	rm -f *.o ddl.c ddl.h cli.c cli.h lex.yy.c
	make -C templates clean
