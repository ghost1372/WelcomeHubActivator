PS5_HOST ?= ps5
PS5_PORT ?= 9021

ifdef PS5_PAYLOAD_SDK
    include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
    $(error PS5_PAYLOAD_SDK is undefined)
endif

PAYLOAD_VERSION ?= v1.2.0
ELF := WelcomeHubActivator-$(PAYLOAD_VERSION).elf
SOURCES := main.c db.c server.c sqlite/sqlite3.c
HEADERS := config.h db.h server.h sqlite/sqlite3.h
ASSETS  := web/index.html
CPPFLAGS := -DWELCOMEHUB_PAYLOAD_VERSION=\"$(PAYLOAD_VERSION)\"
CFLAGS := -Wall -Werror -Os
LDLIBS := -lSceUserService -lSceSystemService

.PHONY: all clean test debug

all: $(ELF)

$(ELF): $(SOURCES) $(HEADERS) $(ASSETS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	rm -f $(ELF)

test: $(ELF)
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $^

debug: $(ELF)
	gdb-multiarch \
	-ex "set architecture i386:x86-64" \
	-ex "target extended-remote $(PS5_HOST):2159" \
	-ex "file $(ELF)" \
	-ex "remote put $(ELF) /data/$(ELF)" \
	-ex "set remote exec-file /data/$(ELF)" \
	-ex "start"
