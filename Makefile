TARGET = tsc
CFLAGS = -Wall -pedantic -std=c11 -g
LDFLAGS =

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

debug: CARGS += -DDEBUG -g
debug: $(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET)
