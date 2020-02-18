#include <stdio.h>
#include <stdlib.h>


int main() {
  int * ptr, * ptr2;
  int * firstPtr; 

  int i;

  int size = 14; 
  for(i = 0; i < 2; i++) {
    ptr = (int *)malloc(14);
    if(i == 0) {
      firstPtr == ptr;
    }
    else {
      if(ptr == firstPtr) {
       fprintf(stderr, "WARNING: Please adjust MALLOC_SITE_OFFSET of xdefines.h to one of \\
           0x48, 0x50, 0x58, 0x60. Until this warning dissappears\n");
       }
    }
    for(int j = 0; j < 2; j++) {
      ptr2 = (int *)malloc(16);
    }
    fprintf(stderr, "size %d ptr %p ptr2 %p\n", size, ptr, ptr2);
    free(ptr);
  }

  ptr = new int;
  fprintf(stderr, "printf new ptr %p\n", ptr);
}
