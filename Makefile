DEBUG=y

ifeq ($(DEBUG), y)
	CFLAGS += -O -g -DDEBUG
else
	CFLAGS += -O2
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
