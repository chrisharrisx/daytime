SOURCES = client.c server.c tunnel.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLES = $(SOURCES:.c=)
CFLAGS = -std=gnu99 -Wall

all: client server tunnel

clean:
	@rm -f $(OBJECTS) $(EXECUTABLES)

distclean: clean
