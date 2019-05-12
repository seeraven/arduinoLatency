CC      = g++
CFLAGS  = -O3 -march=native
LIBS    = -lrt

UNAME_M := $(shell uname -m)

ifeq ($(UNAME_M),armv7l)
    UNAME_N := $(shell uname -n)

    ifeq ($(UNAME_N),raspberrypi)
        CFLAGS += -DON_RASPBERRY_PI
    else
        CFLAGS += -DON_ODROID_XU4
    endif
    LIBS += -lwiringPi -lpthread -lcrypt
endif

_OBJ      = main.o

SRCDIR    = .
ODIR      = obj
ODIR_KDRV = obj_kdrv
MKDIR_P   = mkdir -p

OBJ       = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_KDRV  = $(patsubst %,$(ODIR_KDRV)/%,$(_OBJ))

all: directories latencyTest latencyTestKMod

.PHONY: directories clean

directories: $(ODIR) $(ODIR_KDRV)


$(ODIR):
	$(MKDIR_P) $(ODIR)

$(ODIR_KDRV):
	$(MKDIR_P) $(ODIR_KDRV)


$(ODIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR_KDRV)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS) -DUSE_KERNEL_DRIVER


latencyTest: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

latencyTestKMod: $(OBJ_KDRV)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)


clean:
	rm -rf $(ODIR) $(ODIR_KDRV) *~ core latencyTest latencyTestKMod *.gpd

