/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 <ben.lh@alibaba-inc.com>
 */

#include <platform_override.h>
#include <thead/c9xx_encoding.h>
#include <thead/light/asm.h>
#include <thead/light/aonsys_reg_define.h>
#include <thead/light/apsys_reg_define.h>
#include <thead/light/aprst_reg_define.h>
#include <thead/light/apclk_reg_define.h>
#include <thead/thead_aon.h>
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_init.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_scratch.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi/sbi_system.h>

#define INDICATOR_0_MAGIC_NUM           0x5a5a5a5a
#define INDICATOR_1_MAGIC_NUM           0x12345678
#define INDICATOR_2_MAGIC_NUM           0x32fde438
#define INDICATOR_3_MAGIC_NUM           0x8ab4c52c

/* system lowpoer mode */
#define LP_HW_VAD			(1 << 16)
#define LP_STANDBY			(2 << 16)

/* redefine CSR register */
#define CSR_MXSTATUS		THEAD_C9XX_CSR_MXSTATUS
#define CSR_MHCR		THEAD_C9XX_CSR_MHCR
#define CSR_MCCR2		THEAD_C9XX_CSR_MCCR2
#define CSR_MHINT		THEAD_C9XX_CSR_MHINT
#define CSR_MHINT2_E		THEAD_C9XX_CSR_MHINT2
#define CSR_MHINT4		THEAD_C9XX_CSR_MHINT4
#define CSR_MSMPR		THEAD_C9XX_CSR_MSMPR
#define CSR_SMPEN		CSR_MSMPR

/* CSR_MHCR */
#define MHCR_IE			_UL(0x00000001)
#define MHCR_DE			_UL(0x00000002)

/* CSR_MHINT */
#define MHINT_DPLD		_UL(0x00000004)
#define MHINT_IPLD		_UL(0x00000100)
#define MHINT_L2PLD		_UL(0x00008000)

/* CSR_MSMPR */
#define MSMPR_MSPEN		_UL(0x00000001)

#define CONFIG_SYS_CACHELINE_SIZE   64

#define ONLINE			true
#define OFFLINE			false
#define C910_CORE0		0
#define C910_CORE1		1
#define C910_CORE2		2
#define C910_CORE3		3

static unsigned long csr_mstatus;
static unsigned long csr_mie;
static unsigned long csr_mhcr;
static unsigned long csr_mhint;
static unsigned long csr_msmpen;
int hotplug_flag = 0;

extern void __thead_pre_start_warm(void);
extern void *_data_start, *_data_end, *_bss_start, *_bss_end, *_fw_end;

static void light_set_lpmode(int mode)
{
	writel(mode, (volatile void *)REG_AON_CHIP_LP_MODE);
}

static void light_mastercore_entryboot_set()
{
	u32 t;

	writel(INDICATOR_0_MAGIC_NUM, (volatile void *)REG_AON_STR_INDICATOR_0);
	writel(INDICATOR_1_MAGIC_NUM, (volatile void *)REG_AON_STR_INDICATOR_1);
	writel(INDICATOR_2_MAGIC_NUM, (volatile void *)REG_AON_STR_INDICATOR_2);
	writel(INDICATOR_3_MAGIC_NUM, (volatile void *)REG_AON_STR_INDICATOR_3);

	/* brom->spl entry addr */
	writel(0xFF, (volatile void *)REG_AON_RESERVED_REG_0);
	writel(0xE0000000, (volatile void *)REG_AON_RESERVED_REG_1);

	/* spl->opensbi entry addr */
	t = (ulong)&__thead_pre_start_warm;
	writel(t, (volatile void *)REG_AON_RESERVED_REG_2);
	t = (u64)(ulong)&__thead_pre_start_warm >> 32;
	writel(t, (volatile void *)REG_AON_RESERVED_REG_3);
}

static void light_auxcore_entryboot_set()
{
	u32 t;

	/* spl->opensbi entry addr */
	t = (ulong)&__thead_pre_start_warm;
	writel(t, (volatile void *)REG_C910_CORE0_RVBA_L);
	writel(t, (volatile void *)REG_C910_CORE1_RVBA_L);
	writel(t, (volatile void *)REG_C910_CORE2_RVBA_L);
	writel(t, (volatile void *)REG_C910_CORE3_RVBA_L);
	t = (u64)(ulong)&__thead_pre_start_warm >> 32;
	writel(t, (volatile void *)REG_C910_CORE0_RVBA_H);
	writel(t, (volatile void *)REG_C910_CORE1_RVBA_H);
	writel(t, (volatile void *)REG_C910_CORE2_RVBA_H);
	writel(t, (volatile void *)REG_C910_CORE3_RVBA_H);
}

void cpu_performance_disable(void)
{
	csr_write(CSR_SMPEN, 0x0);
	csr_write(CSR_MHINT2_E, 0x0);
	csr_write(CSR_MHINT4, 0x0);
	csr_write(CSR_MCCR2, 0x02490009);
	csr_write(CSR_MHCR, 0x11ff);
	csr_write(CSR_MXSTATUS, 0xc0638000);
	csr_write(CSR_MHINT, 0x24000);
}

static void light_mastercore_save(void)
{
	/* a) disable all irq */
	csr_mstatus = csr_read_clear(CSR_MSTATUS, MSTATUS_MIE | MSTATUS_SIE);
	csr_mie = csr_read_clear(CSR_MIE, MIP_MSIP | MIP_MTIP | MIP_MEIP | \
				MIP_SSIP | MIP_STIP | MIP_SEIP );
	hotplug_flag = 1;
	/* b) close prefetch */
	csr_mhint = csr_read_clear(CSR_MHINT, MHINT_L2PLD | MHINT_IPLD | MHINT_DPLD);
	/* c) inv&clr d-call all */
	dcache_ciall();
	sync_is();
	/* d) close dcache */
	csr_mhcr = csr_read_clear(CSR_MHCR, MHCR_DE);
	/* e) close smpen */
	csr_msmpen = csr_read_clear(CSR_MSMPR, MSMPR_MSPEN);
	/* f) fence iorw,iorw*/
	mb();
	/* g) sleepmode reg */
	light_set_lpmode(LP_STANDBY);

	/* set mastercore bootrom jump entry */
	light_mastercore_entryboot_set();

	cpu_performance_disable();

	/* h) wfi */
	wfi();
	__thead_pre_start_warm();
}

static void light_auxcore_save(void)
{
	/* a) disable all irq */
	csr_mstatus = csr_read_clear(CSR_MSTATUS, MSTATUS_MIE | MSTATUS_SIE);
	csr_mie = csr_read_clear(CSR_MIE, MIP_MSIP | MIP_MTIP | MIP_MEIP | \
				MIP_SSIP | MIP_STIP | MIP_SEIP );

	hotplug_flag = 1;
	/* b) close prefetch */
	csr_mhint = csr_read_clear(CSR_MHINT, MHINT_L2PLD | MHINT_IPLD | MHINT_DPLD);
	/* c) inv&clr d-call all */
	dcache_ciall();
	sync_is();
	/* d) close dcache */
	csr_mhcr = csr_read_clear(CSR_MHCR, MHCR_DE);
	/* e) close smpen */
	csr_msmpen = csr_read_clear(CSR_MSMPR, MSMPR_MSPEN);
	/* f) fence iorw,iorw*/
	mb();
	/* g) sleepmode reg */

	/* h) wfi : when test hotplug just comment wfi to continue run */
	wfi();
}

static void light_auxcore_restore(u32 hartid)
{
	/* set auxcore bootrom jump entry after warm reset*/
	light_auxcore_entryboot_set();
}

static int light_hart_start(u32 hartid, ulong saddr)
{
	int ret;

	/* send ipi to triger already plugout core which will be waiting in sbi_hsm_hart_wait
	 * after reset.
	 */
	if (sbi_entry_count(hartid) == C910_CORE0)
		return sbi_ipi_raw_send(sbi_hartid_to_hartindex(hartid));

	light_auxcore_restore(hartid);
	ret = thead_aon_cpuhp(hartid, ONLINE);
	if(ret)
		return ret;
	return 0;
}

static int light_hart_stop(void)
{
	if (sbi_entry_count(current_hartid()) == C910_CORE0)
	{
	    sbi_printf("[%s,%d]core %d is not avaliable in hotplug process"
			    , __func__, __LINE__, current_hartid());
	    wfi();
	}
	else
	{
	    thead_aon_cpuhp(current_hartid(), OFFLINE);
	    light_auxcore_save();
	}

	/* It should never reach here */
	sbi_hart_hang();

	return 0;
}

static int th1520_system_suspend(u32 suspend_type,
				 unsigned long resume_addr)
 {
	int ret;

 	/* Use the generic code for retentive suspend. */
	if (suspend_type != SBI_SUSP_SLEEP_TYPE_SUSPEND)
		return SBI_EINVAL;

	ret = thead_aon_system_suspend();
	if (ret) {
		sbi_printf("failed to notify aon subsys with suspend to ram...%08x\n", ret);
		return ret;
	}
 	light_mastercore_save();

 	return 0;
 }

static int th1520_system_suspend_check(u32 sleep_type)
{
	int ret;
	ret = (sleep_type == SBI_SUSP_SLEEP_TYPE_SUSPEND) ? 0 : SBI_EINVAL;
	if(ret)
		return ret;

	return SBI_OK;
}

 const struct sbi_hsm_device light_ppu = {
 	.name		= "light-ppu",
 	.hart_start	= light_hart_start,
 	.hart_stop	= light_hart_stop,
};

struct sbi_system_suspend_device th1520_susp = {
	.name		= "th1520-susp",
	.system_suspend_check = th1520_system_suspend_check,
	.system_suspend = th1520_system_suspend,
};
