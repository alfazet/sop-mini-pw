CC=gcc
CFLAGS=-Wall -Wextra -g
LDFLAGS=-fsanitize=address,undefined -fanalyzer

TARGET=prod_cons
FILES=${TARGET}.o

.PHONY: clean all

${TARGET} : ${FILES}
	${CC} ${LDFLAGS} ${LDLIBS} -o ${TARGET} ${FILES}

${TARGET}.o: ${TARGET}.c
	${CC} ${CFLAGS} -o ${TARGET}.o -c ${TARGET}.c

all: ${TARGET}

clean:
	rm -f ${FILES} ${TARGET}
