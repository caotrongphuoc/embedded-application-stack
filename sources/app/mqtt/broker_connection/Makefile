CC      = gcc
CFLAGS  = -W -Wall -O2 -I../lib
SRC     = mqtt.c ../lib/mongoose.c
BIN     = mqtt

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)

.PHONY: run clean
