#CXX=arm-buildroot-linux-uclibcgnueabihf-g++

IDIR=.
CFLAGS=-I$(IDIR) -Wno-implicit-function-declaration

LIBS=-lm -lpthread

all: sigw sigq serial_keypad

sigw: sigw.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

sigq: sigq.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

serial_keypad: serial_keypad.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

$%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf a.out *.o *~ core* sigw sigq serial_keypad
