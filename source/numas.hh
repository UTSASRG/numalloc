//
// Created by XIN ZHAO on 11/27/19.
//

#ifndef NUMACHY_NUMAS_HH
#define NUMACHY_NUMAS_HH

#include <numa.h>
#include <vector>
#include <ios>
#include <sys/sysinfo.h>


struct logic_cpu {
    int size;
    int cpus[1024];
};

class Numa {

public:
    // There are multiple issues of this program. 
    // First, the "#ifndef NUMACHY_NUMAS_HH" are not following the same rule of the file. 
    // Second, it will return a structure, which will involve in one additional (unnecessary copy). Two mechanisms will
    // be certainly better, first, we will pass the pointer in from the caller function.  
    // Third, this is a initialization function, which calls a lot of CPU_ALLOC, numa_bitmask_alloc function that could potentially call allocation. Since we are designning an allocator, which will potentially cause the confusion in the initialization phase. Also, there is a potential memory leak, since there is no deallocation of such functions. 
    // Fourth, bm will be allocated again and again for each node. That is just unnecessary.
    // Fifth, there is no comments at all. 
    static struct logic_cpu get_logic_cpu(int node_num) {
        struct logic_cpu result;

        int total_cpus_num = get_nprocs();


        result.size = total_cpus_num / NUMA_NODES;
        fprintf(stderr, "node %d total cpus %d size %d\n", node_num, total_cpus_num, result.size);

        bitmask *bm = numa_bitmask_alloc(total_cpus_num);

        fprintf(stderr, "bm is %p\n", bm);
        numa_node_to_cpus(node_num, bm);//converts a node number to a bitmask of CPUs

        int mask_index = 0;
        unsigned long mask = bm->maskp[mask_index];

        for (int i = 0, cpu_num = 0; i < result.size; cpu_num++) {
            fprintf(stderr, "mask %d, i %d\n", mask, i);
            if (mask == 0) {
                mask_index++;
                mask = bm->maskp[mask_index];
            }
            if (mask % 2 == 0) {
                mask = mask >> 1;
                continue;
            }
            mask = mask >> 1;
            result.cpus[i] = cpu_num;
            i++;
        }

        return result;

    }
};


#endif //NUMACHY_NUMAS_HH
