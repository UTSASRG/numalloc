#ifndef __TIME_H__
#define __TIME_H__

inline unsigned long long rdtscp() {
    unsigned int lo, hi;
    asm volatile (
            "rdtscp"
            : "=a"(lo), "=d"(hi) /* outputs */
            : "a"(0)             /* inputs */
            : "%ebx", "%ecx");     /* clobbers*/
    unsigned long long retval = ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
    return retval;
}

#endif
