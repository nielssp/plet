TARGET = tsc
CFLAGS = -Wall -pedantic -std=c11
LDFLAGS =

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)

TESTS := $(filter-out src/main.c, $(SOURCES)) $(wildcard tests/*.c)
TEST_OBJECTS := $(TESTS:.c=.o)

.PHONY: all
all: $(TARGET)

.PHONY: debug
debug: CFLAGS += -DDEBUG -g
debug: all

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: test
test: CFLAGS += -DDEBUG -g
test: test_all
	valgrind --leak-check=full ./test_all

test_all: $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_OBJECTS) test_all
