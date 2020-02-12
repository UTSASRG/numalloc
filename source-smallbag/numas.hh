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
    static struct logic_cpu get_logic_cpu(int node_num) {
        struct logic_cpu result;

        int total_cpus_num = get_nprocs();
        result.size = total_cpus_num / NUMA_NODES;

        bitmask *bm = numa_bitmask_alloc(total_cpus_num);

        numa_node_to_cpus(node_num, bm);//converts a node number to a bitmask of CPUs

        int mask_index = 0;
        unsigned long mask = bm->maskp[mask_index];
        for (int i = 0, cpu_num = 0; i < result.size; cpu_num++) {
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
