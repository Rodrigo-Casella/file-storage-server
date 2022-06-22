VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj
BDIR = ./bin

_SRCCLIENT = client.c cmdLineParser.c
SRCCLIENT = $(addprefix $(SDIR)/, $(_SRCCLIENT))
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))

_OBJIOUTILS = io_utils.o
OBJIOUTILS = $(addprefix $(ODIR)/, $(_OBJIOUTILS))

_OBJAPI = api.o
OBJAPI = $(addprefix $(ODIR)/, $(_OBJAPI))

_OBJSERVERPTHREAD = server.o worker.o boundedqueue.o filesystem.o
OBJSERVERPTHREAD = $(addprefix $(ODIR)/, $(_OBJSERVERPTHREAD))
_OBJSERVER = configParser.o icl_hash.o fdList.o
OBJSERVER = $(addprefix $(ODIR)/, $(_OBJSERVER))

CC = gcc -g -std=c99 -pedantic
PTHREAD = -pthread
CFLAGS = -Wall
APILIB = -lapi
LDFLAG = -lpthread

.PHONY: all clean

all: $(BDIR)/client $(BDIR)/server


$(BDIR)/client: $(OBJIOUTILS) $(OBJCLIENT) $(BDIR)/libapi.so | $(BDIR)
	$(CC) -o $@ $(CFLAGS) $(OBJCLIENT) $(OBJIOUTILS) -Wl,-rpath=$(BDIR) -L$(BDIR) $(APILIB)

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR)/libapi.so: $(OBJAPI) | $(BDIR)
	$(CC) -shared -o $@ $(CFLAGS) $^

$(OBJAPI): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -fPIC -o $@ $(CFLAGS) $<

$(OBJIOUTILS): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR)/server: $(OBJSERVER) $(OBJSERVERPTHREAD) | $(BDIR)
	$(CC) $(PTHREAD) -o $@ $(CFLAGS) $^ $(LDFLAG)

$(OBJSERVERPTHREAD): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) $(PTHREAD) -c -o $@ $(CFLAGS) $< $(LDFLAG)

$(OBJSERVER): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR):
	mkdir -p $(BDIR)

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -f *~ $(ODIR)/*.o $(BDIR)/*