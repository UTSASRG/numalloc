SRCS = real.cpp							\
			 xthread.cpp          \
			 libbind.cpp			

INCS = xthread.hh

DEPS = $(SRCS) $(INCS)

CXX = clang++ 


CFLAGS = -std=c++14 -Wno-unused-result -O2 -DNUMA_NODES=8

INCLUDE_DIRS = -I.
LIBS     := dl pthread

TARGETS = libbind.so

all: $(TARGETS)

libbind.so: $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libnumalloc.so -ldl -lpthread -lrt -lnuma

clean:
	rm -f $(TARGETS)
