#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <sys/mman.h>

//#include "mm.hh"

#define NUM_ELEMS ((1 << 30) / sizeof(int)) // 1GB array

void print_node_memusage() {
  for (size_t i=0; i < numa_num_configured_nodes(); i++) {
    FILE *fp;
    char buf[1024];

    snprintf(buf, sizeof(buf),
             "cat /sys/devices/system/node/node%lu/meminfo | grep MemUsed", i);

    if ((fp = popen(buf, "r")) == NULL) {
      perror("popen");
      exit(-1);
    }

    while(fgets(buf, sizeof(buf), fp) != NULL) {
      printf("%s", buf);
    }

    if(pclose(fp))  {
      perror("pclose");
      exit(-1);
    }
  }
}

int main() {
  uint64_t num_nodes = numa_num_configured_nodes()+1;
  uint64_t all_nodes_mask = (1 << numa_num_configured_nodes()) - 1;


  set_mempolicy(MPOL_INTERLEAVE, &all_nodes_mask, num_nodes);

  // print per-node memory usage before
  print_node_memusage();

  fprintf(stderr, "all_nodes_mask is %lx totalsize %lx\n", all_nodes_mask, NUM_ELEMS * sizeof(int));
  // allocate large array and write to it
  int *a = (int *)malloc(NUM_ELEMS * sizeof(int));
  a[0] = 123;
  for (size_t i=1; i < NUM_ELEMS; i++) {
    a[i] = (a[i-1] * a[i-1]) % 1000000;
  }

  // print per-node memory usage after
  print_node_memusage();

  free(a);
  return 0;
}
