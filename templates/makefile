.SUFFIXES:

all: templates.o

TEMPLATES := $(wildcard *.tpl)
C_FILES := $(TEMPLATES:%.tpl=tpl-%.c)
O_FILES := $(C_FILES:%.c=%.o)
O_FILES += $(TEMPLATES:%.tpl=tpl-%-data.o)

.PRECIOUS: $(C_FILES)

tpl-%.c: %.tpl
	ctemplate $<

# shouldn't run twice
tpl-%-data.o: %.tpl
	ctemplate $<

%.o: %.c
	gcc -c $< -I.. -ggdb

templates.o: $(O_FILES)
	ld -r $^ -o $@

clean:
	rm -f *.c *.h *.o
