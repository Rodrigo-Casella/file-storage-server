VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj
_SRCCLIENT = client.c cmdLineParser.c
SRCCLIENT = $(addprefix $(SDIR)/, $(_SRCCLIENT))
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))
BDIR = ./bin

CC = gcc
CFLAGS = -Wall

.PHONY: all debug clean

all: $(BDIR)/client

$(BDIR)/client: $(OBJCLIENT) | $(BDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(ODIR):
	mkdir -p $(ODIR)

debug:	$(BDIR)/clientdb

$(BDIR)/clientdb:	$(SRCCLIENT) | $(BDIR)
	$(CC) -g $^ -o $@  $(CFLAGS)

$(BDIR):
	mkdir -p $(BDIR)

clean:
	rm -rf $(ODIR)/ $(BDIR)/