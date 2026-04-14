/*
 * ============================================================
 *   OS PROJECT 2 — Advanced Scheduling Algorithms
 *   Algorithms: Lottery Scheduler | EDF | MLFQ
 * ============================================================
 *
 */
#include "kernel/types.h"
#include "user/user.h"
 
/* INT_MAX — not available in xv6's headers */
#define INT_MAX  0x7fffffff
 
/* ----------------------------------------------------------------
 * Simple LCG pseudo-random number generator
 * (replaces rand()/srand() which xv6 does not provide)
 * ---------------------------------------------------------------- */
static unsigned int _lcg_seed = 12345;
 
static void lcg_srand(unsigned int s) { _lcg_seed = s; }
 
static int lcg_rand(void) {
    _lcg_seed = _lcg_seed * 1664525u + 1013904223u;
    return (int)((_lcg_seed >> 1) & 0x7fffffff);
}
 
/* Simple strlen — available in xv6 user lib but kept explicit */
/* (xv6 user.h already declares strlen, so we just use it)      */
 
/* Utility: print a repeated character */
static void print_repeat(char c, int n) {
    for (int i = 0; i < n; i++) {
        char buf[2]; buf[0] = c; buf[1] = '\0';
        printf("%s", buf);
    }
}
