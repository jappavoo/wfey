/* Inspired by
   https://github.com/KeccakTeam/KeccakCodePackage/blob/f07a7f614179b431ebfc8b97f93c563368c969ed/support/Kernel-PMU/enable_arm_pmu.c
   https://github.com/zhiyisun/enable_arm_pmu/blob/dev/ko/enable_arm_pmu.c
*/

#include <linux/module.h> /* Needed by all modules */ 
#include <linux/kernel.h> /* Doing kernel work */
#include <linux/smp.h> /* To access all cpus */


#if !defined(__aarch64)
#error Module should only be compiled on ARMv8
#endif

/* Macros */
#define DRIVER_NAME "enable_pmu"

// PMUSERENR Register
#define PMUSERENR_EN_EL0 BIT(0) /* Enable EL0 Access */
#define PMUSERENR_CR     BIT(2) /* Enable read on Cycle Counter */
#define PMUSERENR_ER     BIT(3) /* Enable read on Event Counter */

// PMCR Register
#define PMCR_MASK        0x3f   /* Bits 6-31 will not be affected */

#define PMCR_E           BIT(0) /* Enable all counters enabled by PMCNTENSET_EL0 */
#define PMCR_P           BIT(1) /* Event Counter reset */
#define PMCR_C           BIT(2) /* Cycle Counter reset */

// PMCNTENSET Register
#define PMCNTENSET_C     BIT(31) /* Enable Cycle Counter */
#define PMCNTENSET_EVENT_N(n) (BIT(n))

/* Functions */

static inline u32 pmcr_read(void)
{
  u64 val = 0;
  asm volatile("MRS %0, pmcr_el0" : "r"(val));
  return (u32)val;
}

static inline void pmcr_write(u32 val)
{
  val &= PMCR_MASK;
  isb(); /* Flushes pipeline */
  asm volatile("MSR pmcr_el0, %0" : : "r"((u64)val));
}

static void
enable_cpu_counters(void *data)
{
  printk(KERN_DEBUG "[" DRIVER_NAME "] Enabling UserMode PMU access on CPU #%d", smp_processor_id());

  /* Disabling cycle counter overflow interrupt -- commonly done? perhaps just to not handle them ourselves */
  asm volatile("MSR pmintenclr_el1, %0" : : "r"(BIT(31)));
  
  /* Enable user-mode access to counters */
  asm volatile("MSR pmuserenr_el0, %0" : : "r"((u64)PMUSERENR_EN_EL0|PMUUSERENR_CR|PMUSERENR_ER));

  /* Reset Cycle and Event Counter */
  pmcr_write(PMCR_P | PMCR_C);

  /* Choose which counters to enable */
  asm_volatile("MSR pmcntenset_el0, %0" : : "r"((u64)PMCNTENSET_C|PMCNTENSET_EVENT_N(0));
  
  /* All counters accessible are enabled by PMCNTENSET */
  pmcr_write(pmcr_read() | PMCR_E);
  
}

static void
disable_cpu_counters(void *data)
{
  printk(KERN_DEBUG "[" DRIVER_NAME "] Disabling UserMode PMU access on CPU #%d", smp_processor_id());
}

static int __init
init_module(void) 
{
  // Enabling usermode for the pmu registers -- no extra info & atomically waits
  // until function completed on other cpus
  on_each_cpu(enable_cpu_counters, NULL, 1);
  printk(KERN_INFO "[" DRIVER_NAME "] Initialized");
  return 0; 
} 

static void __exit
cleanup_module(void) 
{ 
  on_each_cpu(disable_cpu_counters, NULL, 1);
  printk(KERN_INFO "[" DRIVER_NAME "] Unloaded");
}

/* Meta */

MODULE_LICENSE("GPL");
module_init(init_module);
module_exit(cleanup_module);
