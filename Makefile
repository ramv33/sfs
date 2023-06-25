ifeq ($(DEBUG), y)
	CFLAGS += -O -g -DDEBUG -Wall -Wextra
else
	CFLAGS += -O2 -Wall -Wextra
endif

OBJS = ls.o ssl.o http.o
LIBS = -lssl -lcrypto

server: $(OBJS) $(LIBS)

http.o: ssl.o ls.o

ssl.o: $(LIBS)

ls.o: 

.PHONY : clean
clean:
	rm -f $(OBJS) server
