CC      = gcc
CFLAGS  = -W -Wall -O2 -Ilib
SRC     = http.c lib/mongoose.c
BIN     = http

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)

.PHONY: clean