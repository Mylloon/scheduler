CC = gcc
RM = rm -r
TAR      = tar -cf
CP       = cp -r
MKDIR    = mkdir -p

SRC_DIR = src
INC_DIR = includes
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJETS  = $(patsubst %.c,%.o,$(notdir $(SOURCES)))

CFLAGS  = -std=gnu11 -pedantic
LDFLAGS =

EXE     = projet
EXE_EXT = out

ARCHIVE = kennel
PDF_DIR = report

%.o: src/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

compilation: $(OBJETS)
	$(CC) -o $(EXE).$(EXE_EXT) $(OBJETS) $(LDFLAGS)

main: CFLAGS += -O3
main: compilation

dev: CFLAGS += -Wall -Wextra -Wshadow -Wcast-align -Wstrict-prototypes
dev: CFLAGS += -fanalyzer -fsanitize=undefined -g -Og
dev: LDFLAGS += -fsanitize=undefined
dev: compilation

all:
	main

pdf-make:
	cd report && \
	$(MAKE)

pdf-clean:
	cd report && \
	$(MAKE) clean

clean: pdf-clean
	-$(RM) $(OBJETS) "$(EXE).$(EXE_EXT)" "$(ARCHIVE).tar"

archive: pdf-make
	$(MKDIR) "$(ARCHIVE)"
	$(CP) "$(SRC_DIR)" "$(INC_DIR)" Makefile README \
	      "$(wildcard $(PDF_DIR)/*.pdf)" "$(ARCHIVE)"
	$(TAR) "$(ARCHIVE).tar" "$(ARCHIVE)"
	$(RM) "$(ARCHIVE)"
