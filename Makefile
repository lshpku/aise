
LDFLAGS+=$(shell llvm-config --ldflags)
CXXFLAGS+=$(shell llvm-config --cxxflags)
CPPFLAGS+=$(shell llvm-config --cppflags)

LLVMLIBS=$(shell llvm-config --libs bitreader core support)

OBJECTS=main.o node.o utils.o miso.o

all: main

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $<

%.bc: %.c
	clang -emit-llvm -O3 -c -o $@ $<

%.ll: %.c
	clang -emit-llvm -O3 -S -o $@ $<

main: $(OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $^ $(LLVMLIBS) $(LDFLAGS)

test:
	./main enum -max-input 2 -o result.miso.txt div_power_2.bc 

count:
	wc -l *.h *.cpp

clean:
	rm -f *.o main