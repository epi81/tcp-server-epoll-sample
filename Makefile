CFLAGS += -W -Wall -Wextra -Werror
RELEASE = -Os -s -fstack-protector-all
DEBUG = -ggdb -fsanitize=address -DDEBUG
RSFLAGS += -O -C prefer-dynamic 

%: %.rs
	rustc $(RSFLAGS) $<

all: echo-server client


debug: CFLAGS += $(DEBUG)
debug: echo-server

release: CFLAGS += $(RELEASE)
release: echo-server


echo-server: tcp-server-epoll.c

clean:
	$(RM) echo-server client
