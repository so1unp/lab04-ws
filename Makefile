CC=gcc
BIN=./bin
CFLAGS=-g -Wall -Wextra -Wshadow -Wconversion -Wunreachable-code -I./src/entidades
LIBS=-lncurses -lpthread

NAVE_SRC=./src/entidades/nave.c ./src/main/testMovimiento.c

.PHONY: all
all: $(BIN)/nave

$(BIN)/nave: $(NAVE_SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f $(BIN)/nave

.PHONY: test
test:
	@./test.sh ||:

zip:
	git archive --format zip --output ${USER}-lab04.zip HEAD

html:
	pandoc -o README.html -f gfm README.md