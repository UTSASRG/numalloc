SRCS = real.cpp							\
			 xthread.cpp          \
			 perthreadheap.cpp          \
			 interheap.cpp \
			 persizeclass.cpp \
			 libnumalloc.cpp			

INCS = numas.hh           \
        dlist.hh               \
		numaheap.hh						\
		pernodeheap.hh				\
		pernodesizeclass.hh      \
		perthreadheap.hh				\
		persizeclass.hh				\
		interheap.hh 					\
		log.hh								\
		mm.hh									\
		real.hh								\
		xdefines.hh						\
		xthread.hh

DEPS = $(SRCS) $(INCS)

CXX = clang++ 


#DNDEBUG
#CFLAGS = -g -std=c++14 -Wno-unused-result -O2 -DNUMA_NODES=2 -DUSE_HUGE_PAGE
CFLAGS = -g -std=c++14 -Wno-unused-result -O2 -DNUMA_NODES=2 -DINTERHEAP_SUPPORT -DUSE_HUGE_PAGE

INCLUDE_DIRS = -I.
LIBS     := dl pthread

TARGETS = libnumalloc.so

all: $(TARGETS)

libnumalloc.so: $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libnumalloc.so -ldl -lpthread -lrt -lnuma

clean:
	rm -f $(TARGETS)
