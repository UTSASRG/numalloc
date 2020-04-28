all:
	#gcc -g -rdynamic /media/umass/datasystem/tongping/numalloc/source/libnumalloc.so -O0 -o testprivatesite testprivatesite.cpp
	gcc -g -O0 -std=c++14 -DUSE_HUGE_PAGE -DNUMA_NODES=8 -o memtest ../source/mm.hh mem_test.cpp -I../source/
#gcc -O2 -o alloc-tcmalloc alloc.cpp -rdynamic /media/umass/datasystem/tongping/tcmalloc/tcmalloc-2.7/.libs/libtcmalloc.so
