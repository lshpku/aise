
LDFLAGS+=$(shell llvm-config --ldflags)
CXXFLAGS+=$(shell llvm-config --cxxflags)
CPPFLAGS+=$(shell llvm-config --cppflags)

LLVMLIBS=$(shell llvm-config --libs bitreader bitwriter core support)

OBJECTS=main.o node.o ioutils.o miso.o

all: main

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $<

main: $(OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $^ $(LLVMLIBS) $(LDFLAGS)

clean:
	rm -f *.o main