CC=gcc
C_FLAGS=-Wall -Wextra -Wshadow -g -O0 -std=c17
# -Werror
L_FLAGS=-fsanitize=address,leak,undefined,pointer-compare,pointer-subtract

TARGET=sop-kg
FILES=${TARGET}.o

.PHONY: clean all

${TARGET} : ${FILES}
	${CC} ${L_FLAGS} -o ${TARGET} ${FILES}

${TARGET}.o: ${TARGET}.c
	${CC} ${C_FLAGS} -o ${TARGET}.o -c ${TARGET}.c

all: ${TARGET}

clean:
	rm -f ${FILES} ${TARGET}
