CC      = gcc
CFLAGS  = -W -Wall -O2 -Ilib -DMG_TLS=MG_TLS_MBED
LDFLAGS = -lmbedtls -lmbedx509 -lmbedcrypto
SRC     = mqtt.c lib/mongoose.c
BIN     = mqtt

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)

.PHONY: clean