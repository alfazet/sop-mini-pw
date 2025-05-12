CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=-fsanitize=address,undefined -fanalyzer

.PHONY: clean all

all: unix_client tcp_client tcp_server

%: %.o
	${CC} ${LDFLAGS} ${LDLIBS} -o $@

%.o: %.c
	${CC} ${CFLAGS} -o $@ -c $<

clean:
	rm -f ${FILES} ${TARGET}
