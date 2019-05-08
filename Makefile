# ICAB Makefile
CC=gcc
LD=gcc
CFLAGS=-Wall -Wextra -O3 -pedantic -Wstrict-prototypes -ffunction-sections -fdata-sections
LDFLAGS=-s -Wl,--gc-sections -Wl,--relax
INCLUDES=-I include -I zlib
INDENT_FLAGS=-br -ce -i4 -bl -bli0 -bls -c4 -cdw -ci4 -cs -nbfda -l100 -lp -prs -nlp -nut -nbfde -npsl -nss

all: host

prepare:
	@mkdir -p release

host: prepare
	@echo "  CC    src/unpack.c"
	@$(CC) $(INCLUDES) $(CFLAGS) -c src/unpack.c -o release/unpack.o
	@echo "  CC    src/pack.c"
	@$(CC) $(INCLUDES) $(CFLAGS) -c src/pack.c -o release/pack.o
	@echo "  CC    src/clone.c"
	@$(CC) $(INCLUDES) $(CFLAGS) -c src/clone.c -o release/clone.o
	@echo "  LD    release/unpack"
	@$(LD) $(LDFLAGS) release/unpack.o zlib/*.o -o release/unpack
	@echo "  LD    release/pack"
	@$(LD) $(LDFLAGS) release/pack.o zlib/*.o -o release/pack
	@echo "  LD    release/clone"
	@$(LD) $(LDFLAGS) release/clone.o zlib/*.o -o release/clone

clean:
	@echo "  CLEAN ."
	@rm -f release/*.o

install:
	@cp -v release/pack /usr/bin/icab-pack
	@cp -v release/unpack /usr/bin/icab-unpack
	@cp -v release/clone /usr/bin/icab-clone
	@cp -v schema /usr/bin/icab-schema

uninstall:
	@rm -fv /usr/bin/icab-pack
	@rm -fv /usr/bin/icab-unpack
	@rm -fv /usr/bin/icab-clone
	@rm -fv /usr/bin/icab-schema

indent:
	@echo 'Formatting all the source code...'
	@find ./ -type f -exec touch {} +
	@indent $(INDENT_FLAGS) include/*.h
	@indent $(INDENT_FLAGS) src/*.c
	@rm -fv include/*~
	@rm -fv src/*~

analysis:
	@scan-build make
	@cppcheck --force include/*.h
	@cppcheck --force src/*.c
