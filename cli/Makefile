IDIR =./
CC=gcc -Wall
CFLAGS=-I$(IDIR)

ODIR=
LDIR =../lib

LIBS=

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = readavrstick.o 
OBJ = $(patsubst %,$(ODIR)%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

readavrstick: $(OBJ)
		$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
		rm -f $(ODIR)/*.o 
