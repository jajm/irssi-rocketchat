CPPFLAGS := -I/usr/include/irssi -I/usr/include/irssi/src -I/usr/include/irssi/src/core -I/usr/include/irssi/src/fe-common/core
CFLAGS := -fPIC -Wall $(shell pkg-config --cflags glib-2.0 libwebsockets jansson) -DUOFF_T_LONG
LDFLAGS := $(shell pkg-config --libs glib-2.0 libwebsockets jansson)

SRCS := rocketchat_core.c rocketchat-servers.c
OBJS := $(SRCS:.c=.o)

all: librocketchat_core.so

%.o: %.c
	gcc ${CPPFLAGS} ${CFLAGS} -c -o $@ $<

librocketchat_core.so: ${OBJS}
	gcc -shared ${LDFLAGS} -o $@ $^

clean:
	rm -f librocketchat_core.so ${OBJS}

install: librocketchat_core.so
	cp librocketchat_core.so ~/.irssi/modules/
