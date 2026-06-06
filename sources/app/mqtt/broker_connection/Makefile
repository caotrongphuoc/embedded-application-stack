CC      = gcc
CFLAGS  = -W -Wall -O2 -I../lib
LDLIBS  =

SRC     = main.c ../lib/mongoose.c
BIN     = broker_connection

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDLIBS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: run clean
