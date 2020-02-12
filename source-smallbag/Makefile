SRCS = real.cpp							\
			 xthread.cpp          \
perthreadsizeclass.cpp          \
			 libnumalloc.cpp			

INCS = numas.hh           \
       jumpfreelist.hh     \
        dlist.hh               \
		numaheap.hh						\
		pernodeheap.hh				\
		pernodesizeclass.hh      \
		perthreadheap.hh				\
		perthreadsizeclass.hh				\
		mainheap.hh 					\
		log.hh								\
		mm.hh									\
		real.hh								\
		xdefines.hh						\
		xthread.hh

DEPS = $(SRCS) $(INCS)

CXX = clang++ 


#DNDEBUG
#CFLAGS = -g -std=c++14 -Wno-unused-result -O2  -DNUMA_NODES=2 -DTRY_TCMALLOC
#-DSPEC_MAINTHREAD_SUPPORT -DUSE_HUGE_PAGE
CFLAGS = -g -std=c++14 -Wno-unused-result -O2  -DNUMA_NODES=2 -DSPEC_MAINTHREAD_SUPPORT -DUSE_HUGE_PAGE
#CFLAGS = -g -std=c++14 -Wno-unused-result -O3  -DNUMA_NODES=2 -DUSE_HUGE_PAGE
#CFLAGS = -g -std=c++14 -Wno-unused-result -O2  -DNUMA_NODES=2 -DSPEC_MAINTHREAD_SUPPORT -DUSE_HUGE_PAGE
#CFLAGS = -g -std=c++14 -Wno-unused-result -O2 -DNUMA_NODES=2 -DUSE_HUGE_PAGE
#CFLAGS = -g -std=c++14 -Wno-unused-result -DNDEBUG -O2 -DNUMA_NODES=2
#CFLAGS = -g -Wno-unused-result -DNDEBUG -O2 -DNUMA_NODES=2
#CFLAGS = -g -Wno-unused-result -DNDEBUG -O3 --std=c++11 -DNEW_BIG_HEAP -DNUMA_NODES=2

INCLUDE_DIRS = -I.
LIBS     := dl pthread

TARGETS = libnumalloc.so

all: $(TARGETS)

libnumalloc.so: $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libnumalloc.so -ldl -lpthread -lrt -lnuma

clean:
	rm -f $(TARGETS)
