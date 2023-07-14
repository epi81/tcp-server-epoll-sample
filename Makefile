CFLAGS += -W -Wall -Wextra -Werror
RELEASE = -Os -s -fstack-protector-all
DEBUG = -ggdb -fsanitize=address -DDEBUG

all: echo-server

debug: CFLAGS += $(DEBUG)
debug: echo-server

echo-server: tcp-server-epoll.c

clean:
	$(RM) echo-server
