SRCS = real.cpp							\
			 xthread.cpp          \
perthreadsizeclass.cpp          \
			 libnumalloc.cpp			

INCS = dlist.h               \
		numaheap.hh						\
		pernodeheap.hh				\
		pernodebigobjects.hh				\
		pernodesizeclass.hh      \
		perthreadheap.hh				\
		perthreadsizeclass.hh				\
		log.hh								\
		mm.hh									\
		real.hh								\
		xdefines.hh						\
		xthread.hh

DEPS = $(SRCS) $(INCS)

CXX = g++ 

#DNDEBUG
CFLAGS = -g -Wno-unused-result -DNDEBUG -O2 -DNUMA_NODES=2
#CFLAGS = -g -Wno-unused-result -DNDEBUG -O2 -DNUMA_NODES=2
#CFLAGS = -g -Wno-unused-result -DNDEBUG -O3 --std=c++11 -DNEW_BIG_HEAP -DNUMA_NODES=2

INCLUDE_DIRS = -I. -I/usr/include/x86_64-linux-gnu/c++/4.8/ -I./rng
LIBS     := dl pthread

TARGETS = libnumalloc.so

all: $(TARGETS)

libnumalloc.so: $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libnumalloc.so -ldl -lpthread -lrt -lnuma

clean:
	rm -f $(TARGETS)
