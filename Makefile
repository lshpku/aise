
LDFLAGS+=$(shell llvm-config --ldflags)
CXXFLAGS+=$(shell llvm-config --cxxflags)
CPPFLAGS+=$(shell llvm-config --cppflags)

LLVMLIBS=$(shell llvm-config --libs bitreader bitwriter core support)

LOADABLE_MODULE_OPTIONS=-shared -Wl,-O1

all: irread

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $<

irgen: irgen.o
	$(CXX) -o $@ $(CXXFLAGS) $< $(LLVMLIBS) $(LDFLAGS)

irread: irread.o
	$(CXX) -o $@ $(CXXFLAGS) $< $(LLVMLIBS) $(LDFLAGS)

fnarg.so: FnArgCnt.o
	$(CXX) -o $@ $(LOADABLE_MODULE_OPTIONS) $(CXXFLAGS) $(LDFLAGS) $^
