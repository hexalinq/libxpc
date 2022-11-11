CC:= gcc
LDLIBS += -lusb-1.0 -fsanitize=address
CFLAGS += -Wall -Wextra -fsanitize=address -g3 -D_GNU_SOURCE

xpc: obj/main.o libxpc.a
	$(CC) -o$@ $^ $(LDLIBS)

libxpc.a: obj/bitfile.o obj/driver.o
	rm -f $@
	ar rcs $@ $^

obj/%.o: %.c
	@mkdir -p obj
	$(CC) -c $(CFLAGS) -o$@ $<

install: xpc
	cp -av xpc /usr/bin/

.PHONY:	install
