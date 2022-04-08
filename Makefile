VPATH = ./src:./include
SDIR = ./src
ODIR = ./obj
_OBJCLIENT = client.o cmdLineParser.o
OBJCLIENT = $(addprefix $(ODIR)/, $(_OBJCLIENT))
BDIR = ./bin

CC = gcc
CFLAGS = -Wall

.PHONY: all clean

all: $(BDIR)/client

$(BDIR)/client: $(OBJCLIENT) | $(BDIR)
	$(CC) -o $@ $^ $(CFLAGS)

$(BDIR):
	mkdir -p $(BDIR)

$(OBJCLIENT): $(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR):
	mkdir -p $(ODIR)


clean:
	rm -rf $(ODIR)/ $(BDIR)/