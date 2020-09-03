SRCS = real.cpp							\
			 xthread.cpp          \
			 perthreadheap.cpp          \
			 interheap.cpp \
			 pernodesizeclassbag.cpp \
			 libnumalloc.cpp			

INCS = dlist.hh               \
		numaheap.hh						\
		pernodeheap.hh				\
		perthreadheap.hh				\
		perthreadsizeclasslist.hh				\
		pernodesizeclasslist.hh				\
		pernodesizeclassbag.hh				\
		interheap.hh 					\
		log.hh								\
		mm.hh									\
		real.hh								\
		xdefines.hh						\
		xthread.hh

DEPS = $(SRCS) $(INCS)

CXX = clang++ 


#DNDEBUG
#CFLAGS = -g -O2 -std=c++14 -Wno-unused-result -DNUMA_NODES=8 -DUSE_HUGE_PAGE
CFLAGS = -g -O2 -std=c++14 -Wno-unused-result -DNUMA_NODES=2 -DINTERHEAP_SUPPORT -DUSE_HUGE_PAGE

INCLUDE_DIRS = -I.
LIBS     := dl pthread

TARGETS = libnumalloc.so

all: $(TARGETS)

libnumalloc.so: $(DEPS)
	$(CXX) $(CFLAGS) $(INCLUDE_DIRS) -shared -fPIC $(SRCS) -o libnumalloc.so -ldl -lpthread -lrt -lnuma

clean:
	rm -f $(TARGETS)
