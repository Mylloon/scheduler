CC    = gcc
RM    = rm -rf
TAR   = tar -cf
CP    = cp -r
MKDIR = mkdir -p

SRC_DIR = src
INC_DIR = includes
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJETS  = $(patsubst %.c,%.o,$(notdir $(SOURCES)))

CFLAGS  = -std=gnu11 -pedantic
LDFLAGS =

EXE     = ordonnanceur
EXE_EXT = .elf

ARCHIVE = kennel
PDF_DIR = report

%.o: src/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

release: CFLAGS += -O3
release: compilation

debug: CFLAGS  += -Wall -Wextra -Wshadow -Wcast-align -Wstrict-prototypes
debug: CFLAGS  += -fanalyzer -fsanitize=undefined -g -Og
debug: LDFLAGS += -fsanitize=undefined
debug: compilation

compilation: $(OBJETS)
	$(CC) -o $(EXE)$(EXE_EXT) $(OBJETS) $(LDFLAGS)

all:
	release

pdf-make:
	cd report && \
	$(MAKE)

pdf-clean:
	@cd report && \
	$(MAKE) clean

clean: pdf-clean
	$(RM) $(OBJETS) "$(EXE)$(EXE_EXT)" "$(ARCHIVE).tar"

archive: pdf-make
	$(MKDIR) "$(ARCHIVE)"
	$(CP) "$(SRC_DIR)" "$(INC_DIR)" Makefile README \
	      "$(wildcard $(PDF_DIR)/*.pdf)" "$(ARCHIVE)"
	$(TAR) "$(ARCHIVE).tar" "$(ARCHIVE)"
	$(RM) "$(ARCHIVE)"
