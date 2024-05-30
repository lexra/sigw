CXX=arm-buildroot-linux-uclibcgnueabihf-g++
CC=arm-buildroot-linux-uclibcgnueabihf-gcc

IDIR=.
CFLAGS=-I$(IDIR) -Wno-implicit-function-declaration

#LIBS=-L/work/klm5s3-v2.5.5/vtcs_toolchain/vienna/usr/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/lib -lm -lpthread -lsqlite3
LIBS=-lm -lpthread -lsqlite3

all: sigw sigq serial_keypad #sqlite3_test

sqlite3_test: sqlite3_test.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

sigw: sigw.o event.o tcpsvc.o uart.o timer.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

sigq: sigq.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

serial_keypad: serial_keypad.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

$%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf a.out *.o *~ core* sigw sigq serial_keypad sqlite3_test
