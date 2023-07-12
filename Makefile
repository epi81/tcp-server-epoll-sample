CFLAGS += -W -Wall -Wextra -Werror -Os -s -fstack-protector-all

all: main

main: tcp-server-epoll.c

clean:
	$(RM) main
