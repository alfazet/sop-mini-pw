CC=gcc
CFLAGS=-Wall -fsanitize=address,undefined

.PHONY: all
all: prog_opt prog_env

prog_opt: prog_opt.c
	$(CC) -o $@ $^

prog_env: prog_env.c
	$(CC) -o $@ $^
