CFLAGS += -Wall -Werror -pedantic -std=gnu89
all: forth
test: all
	cd tests && ./run
