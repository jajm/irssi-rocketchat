CPPFLAGS := -I../core/ -I/usr/include/irssi -I/usr/include/irssi/src -I/usr/include/irssi/src/core -I/usr/include/irssi/src/fe-common/core
CFLAGS := -fPIC -Wall $(shell pkg-config --cflags glib-2.0 libwebsockets jansson) -DUOFF_T_LONG
LDFLAGS := $(shell pkg-config --libs glib-2.0 libwebsockets jansson)

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

all: libfe_rocketchat.so

%.o: %.c
	gcc ${CPPFLAGS} ${CFLAGS} -c -o $@ $<

libfe_rocketchat.so: ${OBJS}
	gcc -shared ${LDFLAGS} -o $@ $^

clean:
	rm -f libfe_rocketchat.so ${OBJS}

install: libfe_rocketchat.so
	cp $^ ~/.irssi/modules/
