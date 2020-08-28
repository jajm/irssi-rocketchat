CPPFLAGS := -I/usr/include/irssi -I/usr/include/irssi/src -I/usr/include/irssi/src/core -I/usr/include/irssi/src/fe-common/core
CFLAGS := -g -fPIC -Wall $(shell pkg-config --cflags glib-2.0 libwebsockets jansson) -DUOFF_T_LONG
LDFLAGS := $(shell pkg-config --libs glib-2.0 libwebsockets jansson)

all: librocketchat_core.so

rocketchat_core.o: rocketchat_core.c
	gcc ${CPPFLAGS} ${CFLAGS} -c -o $@ $<

librocketchat_core.so: rocketchat_core.o
	gcc -shared ${LDFLAGS} -o $@ $<

clean:
	rm -f librocketchat_core.so rocketchat_core.o

install: librocketchat_core.so
	cp librocketchat_core.so ~/.irssi/modules/
