#############################################################################################
# Makefile
#############################################################################################
# G++ is part of GCC (GNU compiler collection) and is a compiler best suited for C++
CC=g++

# Compiler Flags: https://linux.die.net/man/1/g++
#############################################################################################
# -g: produces debugging information (for gdb)
# -Wall: enables all the warnings
# -Wextra: further warnings
# -Werror: treat warnings as errors
# -O: Optimizer turned on
# -std: use the C++ 14 standard
# -c: says not to run the linker
# -pthread: Add support for multithreading using the POSIX threads library. This option sets 
#           flags for both the preprocessor and linker. It does not affect the thread safety 
#           of object code produced by the compiler or that of libraries supplied with it. 
#           These are HP-UX specific flags.
#############################################################################################
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++17 -pthread
LIBS=-lldap -llber

all: clean build
build: ./server ./client

clean:
	clear
	rm -rf ./src/*.o ./obj/* ./bin/* ./src/server ./src/client ./server ./client

./obj/client.o: ./src/client.cpp
	${CC} ${CFLAGS} -o ./obj/client.o -c ./src/client.cpp 

./obj/server.o: ./src/server.cpp
	${CC} ${CFLAGS} -o ./obj/server.o -c ./src/server.cpp

./server: ./obj/server.o
	${CC} ${CFLAGS} -o ./server ./obj/server.o ${LIBS}

./client: ./obj/client.o
	${CC} ${CFLAGS} -o ./client ./obj/client.o
