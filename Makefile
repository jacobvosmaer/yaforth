CFLAGS += -Wall -Werror -pedantic -std=gnu89
all: forth
forth.o: builtin.h
builtin.h: builtin.f builtin
	./builtin < builtin.f > $@
test: all
	t/all
clean:
	rm -rf forth forth.o forth.dSYM/ builtin builtin.h builtin.dSYM/
