CC      =/home/ctp/Workspace/Sources/CAMERA/AnyCloud39AV100_SDK_V1.11_uclibc_7.3/AnyCloud39AV100_SDK_V1.11/tools/arm-anycloud-linux-uclibcgnueabi-v7.3.0/bin/arm-anycloud_v7.3.0-linux-uclibcgnueabi-gcc
CFLAGS  = -W -Wall -O2 -Ilib
SRC     = mqtt.c lib/mongoose.c
BIN     = mqtt

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -f $(BIN)

.PHONY: clean