#include <stdio.h>
#include <stdlib.h>
#include <mm.hh>
#include <unistd.h>

#define NORMAL_PAGE_SIZE 0x1000
#define HUGE_PAGE_SIZE 0x200000
#define TIMES HUGE_PAGE_SIZE/NORMAL_PAGE_SIZE

int main()
{
  int total_huge_page_num=1000;
   size_t start = 0x100000000000;
  void * ptr = MM::mmapAllocatePrivate(total_huge_page_num*HUGE_PAGE_SIZE, (void *)start, false);
  for (int i=0;i<total_huge_page_num;i++){
    *(int*)(ptr+i*HUGE_PAGE_SIZE)=1;
  }

  //for (int i=0;i<total_huge_page_num*TIMES;i++){
  //  *(int*)(ptr+i*NORMAL_PAGE_SIZE)=1;
  //}
  //sleep(10);
}
