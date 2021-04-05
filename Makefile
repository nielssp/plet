TARGET = plet
CFLAGS = -Wall -pedantic -std=c11 -Wstrict-prototypes -Wmissing-prototypes -Wshadow
LDFLAGS = 

ifneq ($(UNICODE), 0)
	LDFLAGS += $(shell pkg-config --libs icu-uc icu-i18n)
	CFLAGS += -DWITH_UNICODE $(shell pkg-config --cflags icu-uc icu-i18n)
endif

ifneq ($(MARKDOWN), 0)
	LDFLAGS += $(shell pkg-config --libs md4c-html)
	CFLAGS += -DWITH_MARKDOWN $(shell pkg-config --cflags md4c-html)
endif

ifneq ($(GUMBO), 0)
	LDFLAGS += $(shell pkg-config --libs gumbo)
	CFLAGS += -DWITH_GUMBO $(shell pkg-config --cflags gumbo)
endif

ifneq ($(IMAGEMAGICK), 0)
	LDFLAGS += $(shell pkg-config --libs MagickWand)
	CFLAGS += -DWITH_IMAGEMAGICK $(shell pkg-config --cflags MagickWand)
endif

ifeq ($(MUSL), 1)
	CFLAGS += -DMUSL
endif

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
	valgrind --leak-check=full --error-exitcode=1 ./test_all

test_all: $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJECTS) $(TARGET) $(TEST_OBJECTS) test_all
