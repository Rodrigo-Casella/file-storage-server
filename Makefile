SHELL := /bin/bash
VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj
BDIR = ./bin
LIBDIR = ./lib

_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))

_OBJIOUTILS = io_utils.o
OBJIOUTILS = $(addprefix $(ODIR)/, $(_OBJIOUTILS))

_OBJAPI = api.o
OBJAPI = $(addprefix $(ODIR)/, $(_OBJAPI))

_LIB = libapi.so
LIB = $(addprefix $(LIBDIR)/, $(_LIB))

_OBJSERVERPTHREAD = server.o worker.o boundedqueue.o filesystem.o logger.o
OBJSERVERPTHREAD = $(addprefix $(ODIR)/, $(_OBJSERVERPTHREAD))

_OBJSERVER = configParser.o icl_hash.o fdList.o compare_func.o
OBJSERVER = $(addprefix $(ODIR)/, $(_OBJSERVER))

CC = gcc -g -std=c99 -pedantic
PTHREAD = -pthread
CFLAGS = -Wall
APILIB = -lapi
LDFLAG = -lpthread

.PHONY: all clean cleanall test1 test2

all: client server


client: $(OBJIOUTILS) $(OBJCLIENT) $(LIB) | $(BDIR)
	$(CC) -o $(BDIR)/$@ $(CFLAGS) $(OBJCLIENT) $(OBJIOUTILS) -Wl,-rpath=$(LIBDIR) -L$(LIBDIR) $(APILIB)

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(LIB): $(OBJIOUTILS) $(OBJAPI) | $(LIBDIR)
	$(CC) -shared -o $@ $(CFLAGS) $(OBJAPI)

$(OBJAPI): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -fPIC -o $@ $(CFLAGS) $<

$(OBJIOUTILS): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

server: $(OBJSERVER) $(OBJSERVERPTHREAD) | $(BDIR)
	$(CC) $(PTHREAD) -o $(BDIR)/$@ $(CFLAGS) $^ $(LDFLAG)

$(OBJSERVERPTHREAD): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) $(PTHREAD) -c -o $@ $(CFLAGS) $< $(LDFLAG)

$(OBJSERVER): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR):
	mkdir -p $(BDIR)

$(ODIR):
	mkdir -p $(ODIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

test1:	server client
	./tests/test1.sh
	./statistiche.sh logs.txt

test2:	server client
	./tests/test2.sh

test3:	server client
	./tests/test3.sh
	./statistiche.sh logs.txt

clean:
	rm -rf $(ODIR) $(BDIR) $(LIBDIR)

cleanall:
	rm -rf $(ODIR) $(BDIR) $(LIBDIR) logs.txt tests/test1tmp1 tests/test1tmp2 tests/evicted1 tests/evicted2 tests/evicted3 tests/evicted4 tests/evicted5 tests/test3tmp