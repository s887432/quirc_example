#CC ?= gcc
CC = arm-buildroot-linux-gnueabihf-gcc
PREFIX ?= /usr/local
SDL_CFLAGS != pkg-config --cflags sdl -mfloat-abi=hard -mfpu=neon -ftree-vectorize
SDL_LIBS != pkg-config --libs sdl

LIB_VERSION = 1.0

CFLAGS ?= -O3 -Wall -fPIC
QUIRC_CFLAGS = -Ilib $(CFLAGS) $(SDL_CFLAGS)
LIB_OBJ = \
    lib/decode.o \
    lib/identify.o \
    lib/quirc.o \
    lib/version_db.o

all: libquirc.so qrcam qrraw

qrraw: tests/qrraw.o tests/data.o libquirc.a
	$(CC) -o $@ tests/data.o tests/qrraw.o libquirc.a $(LDFLAGS) -lm

qrcam: tests/qrcam.o tests/uartFunc.o libquirc.a
	$(CC) -o $@ tests/qrcam.o tests/uartFunc.o libquirc.a $(LDFLAGS) -lm

libquirc.a: $(LIB_OBJ)
	rm -f $@
	ar cru $@ $(LIB_OBJ)
	ranlib $@

.PHONY: libquirc.so
libquirc.so: libquirc.so.$(LIB_VERSION)

libquirc.so.$(LIB_VERSION): $(LIB_OBJ)
	$(CC) -shared -o $@ $(LIB_OBJ) $(LDFLAGS) -lm

.c.o:
	$(CC) $(QUIRC_CFLAGS) -o $@ -c $<

.SUFFIXES: .cxx
.cxx.o:
	$(CXX) $(QUIRC_CXXFLAGS) -o $@ -c $<

clean:
	rm -f */*.o
	rm -f */*.lo
	rm -f libquirc.a
	rm -f libquirc.so.$(LIB_VERSION)
	rm -f qrraw
	rm -f qrcam
