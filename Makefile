VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj

_SRCCLIENT = client.c cmdLineParser.c
SRCCLIENT = $(addprefix $(SDIR)/, $(_SRCCLIENT))
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))

_OBJAPI = api.o
OBJAPI = $(addprefix $(ODIR)/, $(_OBJAPI))

_OBJSERVER = server.o configParser.o
OBJSERVER = $(addprefix $(ODIR)/, $(_OBJSERVER))

BDIR = ./bin

CC = gcc -g -std=c99 -pedantic -pthread
CFLAGS = -Wall 
LDFLAG = -lapi -lpthread

.PHONY: all clean

all: $(BDIR)/client $(BDIR)/server


$(BDIR)/client: $(OBJCLIENT) $(BDIR)/libapi.so | $(BDIR)
	$(CC) -o $@ $(CFLAGS) $(OBJCLIENT) -Wl,-rpath=$(BDIR) -L$(BDIR) $(LDFLAG) 

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR)/libapi.so: $(OBJAPI) | $(BDIR)
	$(CC) -shared -o $@ $(CFLAGS) $^

$(OBJAPI): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -fPIC -o $@ $(CFLAGS) $<

$(BDIR)/server: $(OBJSERVER) | $(BDIR)
	$(CC) -o $@ $(CFLAGS) $^ 

$(OBJSERVER): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR):
	mkdir -p $(BDIR)

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -rf $(ODIR)/ $(BDIR)/ mysock