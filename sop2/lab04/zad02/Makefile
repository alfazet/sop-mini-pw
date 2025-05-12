CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=-fsanitize=address,undefined -fanalyzer

.PHONY: clean all

all: zad02

%: %.o
	${CC} ${LDFLAGS} ${LDLIBS} -o $@

%.o: %.c
	${CC} ${CFLAGS} -o $@ -c $<

clean:
	rm -f ${FILES} ${TARGET}
