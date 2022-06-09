VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj

_SRCCLIENT = client.c cmdLineParser.c
SRCCLIENT = $(addprefix $(SDIR)/, $(_SRCCLIENT))
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))

_OBJAPI = api.o
OBJAPI = $(addprefix $(ODIR)/, $(_OBJAPI))

_OBJSERVER = server.o configParser.o worker.o boundedqueue.o filesystem.o icl_hash.o fdList.o
OBJSERVER = $(addprefix $(ODIR)/, $(_OBJSERVER))

BDIR = ./bin

CC = gcc -g -std=c99 -pedantic
PTHREAD = -pthread
CFLAGS = -Wall
APILIB = -lapi
LDFLAG = -lpthread

.PHONY: all clean

all: $(BDIR)/client $(BDIR)/server


$(BDIR)/client: $(OBJCLIENT) $(BDIR)/libapi.so | $(BDIR)
	$(CC) -o $@ $(CFLAGS) $(OBJCLIENT) -Wl,-rpath=$(BDIR) -L$(BDIR) $(APILIB) 

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR)/libapi.so: $(OBJAPI) | $(BDIR)
	$(CC) -shared -o $@ $(CFLAGS) $^

$(OBJAPI): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -fPIC -o $@ $(CFLAGS) $<

$(BDIR)/server: $(OBJSERVER) | $(BDIR)
	$(CC) $(PTHREAD) -o $@ $(CFLAGS) $^ $(LDFLAG)

$(OBJSERVER): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) $(PTHREAD) -c -o $@ $(CFLAGS) $< $(LDFLAG)

$(BDIR):
	mkdir -p $(BDIR)

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -rf $(ODIR)/ $(BDIR)/ mysock