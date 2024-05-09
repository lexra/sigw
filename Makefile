IDIR = .
CFLAGS = -I$(IDIR)

LIBS = -lm -lpthread

all: sigw sigq

sigw: sigw.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

sigq: sigq.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS)

$%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -rf a.out *.o *~ core* sigw sigq
