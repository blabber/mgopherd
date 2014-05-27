BIN+=		mgopherd
OBJ+=		mgopherd.o
OBJ+=		options.o
OBJ+=		send.o
OBJ+=		tools.o
OBJ+=		gophermap.o

CFLAGS+=	-O2 -pipe  -std=iso9899:1999 -fstack-protector

LDADD+=		-lmagic

all:		$(BIN)

$(BIN):	$(OBJ)
	$(CC) $(LDADD) -o $@ $(OBJ)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(BIN)
