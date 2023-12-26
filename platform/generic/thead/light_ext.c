#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_trap.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <thead/light/asm.h>

#define SBI_EXT_VENDOR_SMC      (SBI_EXT_VENDOR_START + 0)
#define SBI_EXT_VENDOR_PMU      (SBI_EXT_VENDOR_START + 1)
#define SBI_EXT_VENDOR_PMP      (SBI_EXT_VENDOR_START + 2)

#define CSR_MCOUNTERWEN  0x7c9
#define PMP_BASE_ADDR			0xffdc020000UL
#define PMP_SIZE_PER_CORE		0x4000UL
#define TCM0_START_ADDR			0xffe0180000UL
#define TCM0_END_ADDR			0xffe01c0000UL
#define TCM1_START_ADDR			0xffe01c0000UL
#define TCM1_END_ADDR			0xffe0200000UL
#define RESERVED_START_ADDR		0xffe0200000UL
#define RESERVED_END_ADDR		0xffe1000000UL
#define PMP_ENTRY_BASE_ADDR		0x100UL
#define PMP_ENTRY_START_ADDR(n)	(PMP_BASE_ADDR + PMP_ENTRY_BASE_ADDR + (n * 8))
#define PMP_ENTRY_END_ADDR(n)	(PMP_ENTRY_START_ADDR(n) + 4)
#define PMP_ENTRY_CFG_ADDR(n)	(PMP_BASE_ADDR + ((n / 4) * 4))


static void sbi_thead_pmu_init(void)
{
	unsigned long interrupts;
	interrupts = csr_read(CSR_MIDELEG) | (1 << 17);
	csr_write(CSR_MIDELEG, interrupts);
	/* CSR_MCOUNTEREN has already been set in mstatus_init() */
	csr_write(CSR_MCOUNTERWEN, 0xffffffff);
	csr_write(CSR_MHPMEVENT3, 1);
	csr_write(CSR_MHPMEVENT4, 2);
	csr_write(CSR_MHPMEVENT5, 3);
	csr_write(CSR_MHPMEVENT6, 4);
	csr_write(CSR_MHPMEVENT7, 5);
	csr_write(CSR_MHPMEVENT8, 6);
	csr_write(CSR_MHPMEVENT9, 7);
	csr_write(CSR_MHPMEVENT10, 8);
	csr_write(CSR_MHPMEVENT11, 9);
	csr_write(CSR_MHPMEVENT12, 10);
	csr_write(CSR_MHPMEVENT13, 11);
	csr_write(CSR_MHPMEVENT14, 12);
	csr_write(CSR_MHPMEVENT15, 13);
	csr_write(CSR_MHPMEVENT16, 14);
	csr_write(CSR_MHPMEVENT17, 15);
	csr_write(CSR_MHPMEVENT18, 16);
	csr_write(CSR_MHPMEVENT19, 17);
	csr_write(CSR_MHPMEVENT20, 18);
	csr_write(CSR_MHPMEVENT21, 19);
	csr_write(CSR_MHPMEVENT22, 20);
	csr_write(CSR_MHPMEVENT23, 21);
	csr_write(CSR_MHPMEVENT24, 22);
	csr_write(CSR_MHPMEVENT25, 23);
	csr_write(CSR_MHPMEVENT26, 24);
	csr_write(CSR_MHPMEVENT27, 25);
	csr_write(CSR_MHPMEVENT28, 26);
}

static void sbi_thead_pmu_map(unsigned long idx, unsigned long event_id)
{
	switch (idx) {
	case 3:
		csr_write(CSR_MHPMEVENT3, event_id);
		break;
	case 4:
		csr_write(CSR_MHPMEVENT4, event_id);
		break;
	case 5:
		csr_write(CSR_MHPMEVENT5, event_id);
		break;
	case 6:
		csr_write(CSR_MHPMEVENT6, event_id);
		break;
	case 7:
		csr_write(CSR_MHPMEVENT7, event_id);
		break;
	case 8:
		csr_write(CSR_MHPMEVENT8, event_id);
		break;
	case 9:
		csr_write(CSR_MHPMEVENT9, event_id);
		break;
	case 10:
		csr_write(CSR_MHPMEVENT10, event_id);
		break;
	case 11:
		csr_write(CSR_MHPMEVENT11, event_id);
		break;
	case 12:
		csr_write(CSR_MHPMEVENT12, event_id);
		break;
	case 13:
		csr_write(CSR_MHPMEVENT13, event_id);
		break;
	case 14:
		csr_write(CSR_MHPMEVENT14, event_id);
		break;
	case 15:
		csr_write(CSR_MHPMEVENT15, event_id);
		break;
	case 16:
		csr_write(CSR_MHPMEVENT16, event_id);
		break;
	case 17:
		csr_write(CSR_MHPMEVENT17, event_id);
		break;
	case 18:
		csr_write(CSR_MHPMEVENT18, event_id);
		break;
	case 19:
		csr_write(CSR_MHPMEVENT19, event_id);
		break;
	case 20:
		csr_write(CSR_MHPMEVENT20, event_id);
		break;
	case 21:
		csr_write(CSR_MHPMEVENT21, event_id);
		break;
	case 22:
		csr_write(CSR_MHPMEVENT22, event_id);
		break;
	case 23:
		csr_write(CSR_MHPMEVENT23, event_id);
		break;
	case 24:
		csr_write(CSR_MHPMEVENT24, event_id);
		break;
	case 25:
		csr_write(CSR_MHPMEVENT25, event_id);
		break;
	case 26:
		csr_write(CSR_MHPMEVENT26, event_id);
		break;
	case 27:
		csr_write(CSR_MHPMEVENT27, event_id);
		break;
	case 28:
		csr_write(CSR_MHPMEVENT28, event_id);
		break;
	case 29:
		csr_write(CSR_MHPMEVENT29, event_id);
		break;
	case 30:
		csr_write(CSR_MHPMEVENT30, event_id);
		break;
	case 31:
		csr_write(CSR_MHPMEVENT31, event_id);
		break;
	}
}

static void sbi_thead_pmu_set(unsigned long type, unsigned long idx, unsigned long event_id)
{
	switch (type) {
	case 2:
		sbi_thead_pmu_map(idx, event_id);
		break;
	default:
		sbi_thead_pmu_init();
		break;
	}
}

static void sbi_thead_reserved_pmp_set(void)
{
	unsigned int num, reg_val;

	for (num = 0; num < 4; num++) {
		/*	pmp entry 28 for reserved memory	*/
		writel(RESERVED_START_ADDR >> 12, (void *)(PMP_ENTRY_START_ADDR(28) + num*PMP_SIZE_PER_CORE));
		writel(RESERVED_END_ADDR >> 12, (void *)(PMP_ENTRY_END_ADDR(28) + num*PMP_SIZE_PER_CORE));

		/*	pmp entry 28 config	*/
		reg_val = readl((void *)(PMP_ENTRY_CFG_ADDR(28) + num*PMP_SIZE_PER_CORE));
		reg_val = (reg_val & 0xffffff00) | 0x040;
		writel(reg_val, (void *)((PMP_ENTRY_CFG_ADDR(28) + num*PMP_SIZE_PER_CORE)));
	}

	sync_is();
}

static void sbi_thead_tcm0_pmp_set(unsigned long auth)
{
	sbi_printf("%s: auth:%lx \n", __func__, auth);
	unsigned int num, reg_val;

	reg_val = readl((void *)PMP_ENTRY_START_ADDR(26));

	if (reg_val != TCM0_START_ADDR >> 12)
		for(num = 0; num < 4; num++) {
			/*	pmp entry 26 for dsp tcm0	*/
			writel(TCM0_START_ADDR >> 12, (void *)(PMP_ENTRY_START_ADDR(26) + num*PMP_SIZE_PER_CORE));
			writel(TCM0_END_ADDR >> 12, (void *)(PMP_ENTRY_END_ADDR(26) + num*PMP_SIZE_PER_CORE));
		}

	for(num = 0; num < 4; num++) {
		/*	pmp entry 26 config	*/
		reg_val = readl((void *)(PMP_ENTRY_CFG_ADDR(26) + num*PMP_SIZE_PER_CORE));
		reg_val = (reg_val & 0xff00ffff) | (auth << 16);
		writel(reg_val, (void *)(PMP_ENTRY_CFG_ADDR(26) + num*PMP_SIZE_PER_CORE));
	}

	sync_is();
}


static void sbi_thead_tcm1_pmp_set(unsigned long auth)
{
	sbi_printf("%s: auth:%lx \n", __func__, auth);
	unsigned int num, reg_val;

	reg_val = readl((void *)PMP_ENTRY_START_ADDR(27));
	if (reg_val != TCM1_START_ADDR >> 12)
		for (num = 0; num < 4; num++) {
			/*	pmp entry 27 for dsp tcm1	*/
			writel(TCM1_START_ADDR >> 12, (void *)(PMP_ENTRY_START_ADDR(27) + num*PMP_SIZE_PER_CORE));
			writel(TCM1_END_ADDR >> 12, (void *)(PMP_ENTRY_END_ADDR(27) + num*PMP_SIZE_PER_CORE));
		}

	for (num = 0; num < 4; num++) {
		/*	pmp entry 27 config	*/
		reg_val = readl((void *)(PMP_ENTRY_CFG_ADDR(27) + num*PMP_SIZE_PER_CORE));
		reg_val = (reg_val & 0x00ffffff) | (auth << 24);
		writel(reg_val, (void *)(PMP_ENTRY_CFG_ADDR(27) + num*PMP_SIZE_PER_CORE));
	}

	sync_is();
}

static void sbi_thead_pmp_set(unsigned long idx, unsigned long auth)
{
	unsigned int reg_val;

	if (idx !=0 && idx != 1)
		return;

	/*	read pmp entry 28	*/
	reg_val = readl((void *)PMP_ENTRY_START_ADDR(28));

	if (reg_val != RESERVED_START_ADDR >> 12)
		sbi_thead_reserved_pmp_set();

	switch (idx) {
	case 0:
		sbi_thead_tcm0_pmp_set(auth);
		break;
	case 1:
		sbi_thead_tcm1_pmp_set(auth);
		break;
	default:
		break;
	}
}


static int sbi_ecall_light_handler(unsigned long extid, unsigned long funcid,
				  const struct sbi_trap_regs *regs,
				  unsigned long *out_val,
				  struct sbi_trap_info *out_trap)
{
    switch(extid) {
    case SBI_EXT_VENDOR_PMU:
		sbi_thead_pmu_set(regs->a0, regs->a1, regs->a2);
		break;
	case SBI_EXT_VENDOR_PMP:
		sbi_thead_pmp_set(funcid, regs->a0);
		break;
    }
    return 0;
}



struct sbi_ecall_extension ecall_light = {
	.extid_start		= SBI_EXT_VENDOR_PMU,
	.extid_end		= SBI_EXT_VENDOR_PMP,
	.handle			= sbi_ecall_light_handler,
};