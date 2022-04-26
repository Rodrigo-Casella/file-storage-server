VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj
_SRCCLIENT = client.c cmdLineParser.c
SRCCLIENT = $(addprefix $(SDIR)/, $(_SRCCLIENT))
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))
_OBJAPI = api.o
OBJAPI = $(addprefix $(ODIR)/, $(_OBJAPI))
BDIR = ./bin

CC = gcc -std=c99 -pedantic
CFLAGS = -Wall
LDFLAG = -lapi

.PHONY: all debug clean

all: $(BDIR)/client


$(BDIR)/client: $(OBJCLIENT) $(BDIR)/libapi.so | $(BDIR)
	$(CC) -o $@ $(CFLAGS) $(OBJCLIENT) -Wl,-rpath=$(BDIR) -L$(BDIR) $(LDFLAG) 

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(BDIR)/libapi.so: $(OBJAPI) | $(BDIR)
	$(CC) -shared -o $@ $(CFLAGS) $^

$(OBJAPI): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -fPIC -o $@ $(CFLAGS) $<

$(BDIR):
	mkdir -p $(BDIR)

$(ODIR):
	mkdir -p $(ODIR)

debug:	$(BDIR)/clientdb

$(BDIR)/clientdb:	$(SRCCLIENT) $(BDIR)/libapi.so | $(BDIR)
	$(CC) -g $^ -o $@ $(CFLAGS) -Wl,-rpath=$(BDIR) -L$(BDIR) $(LDFLAG)

clean:
	rm -rf $(ODIR)/ $(BDIR)/