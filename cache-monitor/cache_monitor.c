/*
 * Record the cache miss rate of Intel Sandybridge cpu
 * To confirm the event is correctly set!
 */
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */

/*4 Performance Counters Selector for %ecx in insn wrmsr*/
#define PERFEVTSEL0    0x186
#define PERFEVTSEL1    0x187
#define PERFEVTSEL2    0x188
#define PERFEVTSEL3    0x189

/*4 MSR Performance Counter for the above selector*/
#define PMC0    0xc1
#define PMC1    0xc2
#define PMC2    0xc2
#define PMC3    0xc3

/*Intel Software Developer Manual Page 2549*/ /*L1I L1D cache events has not been confirmed!*/
/*L1 Instruction Cache Performance Tuning Events*/
#define L1I_ALLHIT_EVENT    0x80
#define L1I_ALLHIT_MASK     0x01
#define L1I_ALLMISS_EVENT   0x80    /*confirmed*/
#define L1I_ALLMISS_MASK    0x02    /*confirmed*/

/*L1 Data Cache Performance Tuning Events*/ 
/*Intel does not have the ALLREQ Miss mask; have to add LD_miss and ST_miss*/
#define L1D_ALLREQ_EVENT    0x43
#define L1D_ALLREQ_MASK     0x01
#define L1D_LDMISS_EVENT    0x40
#define L1D_LDMISS_MASK     0x01
#define L1D_STMISS_EVENT    0x28
#define L1D_STMISS_MASK     0x01
        
/*L2 private cache for each core*/ /*confirmed*/
#define L2_ALLREQ_EVENT     0x24
#define L2_ALLREQ_MASK      L2_ALLCODEREQ_MASK  /*0xFF*/
#define L2_ALLMISS_EVENT    0x24
#define L2_ALLMISS_MASK     L2_ALLCODEMISS_MASK /*0xAA*/
        
#define L2_ALLCODEREQ_MASK  0x30
#define L2_ALLCODEMISS_MASK 0x20
        
/*L3 shared cache*/ /*confirmed*/
/*Use the last level cache event and mask*/
#define L3_ALLREQ_EVENT     0x2E
#define L3_ALLREQ_MASK      0x4F
#define L3_ALLMISS_EVENT    0x2E
#define L3_ALLMISS_MASK     0x41 
        
#define USR_BIT             (0x01UL << 16)
#define OS_BIT              (0x01UL << 17)
        

#define SET_MSR_USR_BIT(eax)    eax |= USR_BIT
#define CLEAR_MSR_USR_BIT(exa)  eax &= (~USR_BIT)
#define SET_MSR_OS_BIT(eax)     eax |= OS_BIT
#define CLEAR_MSR_OS_BIT(eax)   eax &= (~OS_BIT)

#define SET_EVENT_MASK(eax, event, umask)    eax |= (event | (umask << 8))  

/*MSR EN flag: when set start the counter!*/
//#define MSR_ENFLAG      (0x1<<22)
#define MSR_ENFLAG      (0x1<<22)


/* 32bit insn v3*/
static inline void rtxen_write_msr(uint32_t eax, uint32_t ecx)
{
    /*clear counter first*/
   __asm__ __volatile__ ("movl %0, %%ecx\n\t"
        "xorl %%edx, %%edx\n\t"
        "xorl %%eax, %%eax\n\t"
        "wrmsr\n\t"
        : /* no outputs */
        : "m" (ecx)
        : "eax", "ecx", "edx" /* all clobbered */);

   eax |= MSR_ENFLAG;

   __asm__("movl %0, %%ecx\n\t" /* ecx contains the number of the MSR to set */
        "xorl %%edx, %%edx\n\t"/* edx contains the high bits to set the MSR to */
        "movl %1, %%eax\n\t" /* eax contains the log bits to set the MSR to */
        "wrmsr\n\t"
        : /* no outputs */
        : "m" (ecx), "m" (eax)
        : "eax", "ecx", "edx" /* clobbered */);
}

static inline void  rtxen_read_msr(uint32_t* ecx, uint32_t *eax, uint32_t* edx)
{    __asm__ __volatile__(\
        "rdmsr"\
        :"=d" (*edx), "=a" (*eax)\
        :"c"(*ecx)
        );
}

static inline void delay(void )
{
    char tmp[1000]; 
    int i;
    for( i = 0; i < 1000; i++ )
    {
        tmp[i] = i * 2;
    }
}

enum cache_level
{
    UOPS,
    L1I,
    L1D,
    L2,
    L3
};

int init_module(void)
{
    enum cache_level op;
    uint32_t eax, edx, ecx;
    uint64_t l3_all;
    op = UOPS;
    switch(op)
    {
    case UOPS:
        eax = 0x0001010E;
        eax |= MSR_ENFLAG;
        ecx = 0x187;
        printk(KERN_INFO "UOPS Demo: write_msr: eax=%#010x, ecx=%#010x\n", eax, ecx);
        rtxen_write_msr(eax, ecx);
        ecx = 0xc2;
        eax = 1;
        edx = 2;
        rtxen_read_msr(&ecx, &eax, &edx);
        printk(KERN_INFO "UOPS Demo: read_msr: edx=%#010x, eax=%#010x\n", edx, eax);
        break;
    case L3: 
        eax = 0;
        SET_MSR_USR_BIT(eax);
        SET_MSR_OS_BIT(eax);
        SET_EVENT_MASK(eax, L3_ALLREQ_EVENT, L3_ALLREQ_MASK);
        eax |= MSR_ENFLAG;
        ecx = PERFEVTSEL2;
        printk(KERN_INFO "before wrmsr: eax=%#010x, ecx=%#010x\n", eax, ecx);
        rtxen_write_msr(eax, ecx);
        printk(KERN_INFO "after wrmsr: eax=%#010x, ecx=%#010x\n", eax, ecx);
        printk(KERN_INFO "L3 all request set MSR PMC2\n");
        printk(KERN_INFO "delay by access an array\n");
        delay();
        ecx = PMC2;
        eax = 1;
        edx = 2;
        printk(KERN_INFO "rdmsr: ecx=%#010x\n", ecx);
        rtxen_read_msr(&ecx, &eax, &edx); /*need to pass into address!*/
        l3_all = ( ((uint64_t) edx << 32) | eax );
        printk(KERN_INFO "rdmsr: L3 all request is %llu (%#010lx)\n", l3_all, (unsigned long)l3_all);
        break;
    default:
        printk(KERN_INFO "operation not implemented yet\n");   
    }
    /* 
     * A non 0 return means init_module failed; module can't be loaded. 
     */
    return 0;
}

void cleanup_module(void)
{
    printk(KERN_INFO "Goodbye world 1.\n");
}
