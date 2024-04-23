CC    = gcc
RM    = rm -rf
TAR   = tar -cf
CP    = cp -r
MKDIR = mkdir -p

SRC_DIR = src
INC_DIR = includes

SOURCES         = $(wildcard $(SRC_DIR)/*.c)
SOURCES_NOSCHED = $(filter-out $(wildcard $(SRC_DIR)/sched-*.c), $(SOURCES))

ALL_OBJECTS = $(patsubst %.c,%.o,$(notdir $(SOURCES)))
OBJECTS     = $(patsubst %.c,%.o,$(notdir $(SOURCES_NOSCHED)))

CFLAGS  = -std=gnu11 -pedantic
LDFLAGS =
SCHED   = sched-ws.o

EXE     = ordonnanceur
EXE_EXT = .elf

ARCHIVE_NAME = kennel
PDF_DIR      = report
PDF_NEWNAME  = Rapport de projet

%.o: src/%.c
	$(CC) -c $< -o $@ $(CFLAGS)

release: CFLAGS  += -O2
release: compilation

debug: CFLAGS  += -Wall -Wextra -Wshadow -Wcast-align -Wstrict-prototypes
debug: CFLAGS  += -fanalyzer -fsanitize=undefined -g -Og
debug: LDFLAGS += -fsanitize=undefined
debug: compilation

compilation: $(ALL_OBJECTS)
	$(CC) -o $(EXE)$(EXE_EXT) $(OBJECTS) $(SCHED) $(LDFLAGS)

all:
	release

# Specific schedulers
threads: SCHED = sched-threads.o
threads: release

stack: SCHED = sched-stack.o
stack: release

random: SCHED = sched-random.o
random: release

ws: SCHED = sched-ws.o
ws: release

pdf-make:
	cd report && \
	$(MAKE)

pdf-clean:
	-@cd report && \
	$(MAKE) clean

clean: pdf-clean
	$(RM) *.o "$(EXE)$(EXE_EXT)" "$(ARCHIVE_NAME).tar"

archive: pdf-make
	$(MKDIR) "$(ARCHIVE_NAME)"
	$(CP) "$(SRC_DIR)" "$(INC_DIR)" Makefile README "$(ARCHIVE_NAME)"
	$(CP) "$(wildcard $(PDF_DIR)/*.pdf)" "$(ARCHIVE_NAME)/$(PDF_NEWNAME).pdf"
	$(TAR) "$(ARCHIVE_NAME).tar" "$(ARCHIVE_NAME)"
	$(RM) "$(ARCHIVE_NAME)"
