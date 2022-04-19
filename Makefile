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

$(BDIR):
	mkdir -p $(BDIR)

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(ODIR):
	mkdir -p $(ODIR)

debug:	./clientdb

./clientdb:	$(SRCCLIENT)
	$(CC) -g $^ -o $@  $(CFLAGS)

clean:
	rm -rf $(ODIR)/ $(BDIR)/ clientdb