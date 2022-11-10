CC=gcc
CFLAGS=-std=gnu99 -Wextra -Wno-deprecated -Werror -pedantic -pthread

SOURCES = proj2.c

TARGET = proj2

all: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm proj2