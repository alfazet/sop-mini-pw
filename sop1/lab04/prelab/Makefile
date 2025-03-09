override CFLAGS=-Wall -Wextra -fanalyzer -g -O0 -fsanitize=address,undefined

ifdef CI
override CFLAGS=-Wall -Wextra -Werror
endif

.PHONY: clean all

all: sop-abc

sop-mss: sop-abc.c
	gcc $(CFLAGS) -o sop-abc sop-abc.c

clean:
	rm -f sop-abc
