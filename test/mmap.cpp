#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#define NORMAL_PAGE_SIZE 0x1000
#define HUGE_PAGE_SIZE 0x200000
#define TIMES HUGE_PAGE_SIZE/NORMAL_PAGE_SIZE

int main()
{
  int total=1000;
   void * ptr = mmap(NULL, total*HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1,  false);
   if(ptr == MAP_FAILED) 
   fprintf(stderr, "ptr is %p error %s\n", ptr, strerror(errno));
#if 1
  for (int i=0;i<total;i++){
    *(int*)(ptr+i*HUGE_PAGE_SIZE)=1;
  }

#else
  for (int i=0;i<total*TIMES;i++){
    *(int*)(ptr+i*NORMAL_PAGE_SIZE)=1;
  }
#endif
  //sleep(10);
}
