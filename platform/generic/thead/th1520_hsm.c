/* SPDX-License-Identifier: BSD-2-Clause */
#include <sbi/riscv_asm.h>
#include <sbi/riscv_barrier.h>
#include <sbi/riscv_io.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <thead/th1520.h>

extern void th1520_start_warm(void);

unsigned long th1520_hsm_start_addr[TH1520_HART_COUNT];
static DEFINE_SPIN_LOCK(th1520_start_lock);

static int th1520_hart_start(u32 hartid, unsigned long saddr)
{
	unsigned long entry = (unsigned long)th1520_start_warm;
	void *reset = (void *)TH1520_C910_RESET;
	void *vector;
	u32 value, release;
	int ret = 0;

	if (!hartid || hartid >= TH1520_HART_COUNT || !saddr || (saddr & 1))
		return SBI_EINVAL;

	vector = (void *)TH1520_CORE_START(hartid);
	release = TH1520_C910_CORE_RST_N(hartid);
	spin_lock(&th1520_start_lock);
	value = readl(reset);
	/* Never reset a running hart or disturb the shared cluster reset. */
	if (!(value & TH1520_C910_CLUSTER_RST_N) || (value & release)) {
		ret = SBI_EFAIL;
		goto out;
	}

	th1520_hsm_start_addr[hartid] = saddr;
	writel((u32)entry, vector);
	writel(entry >> 32, vector + 4);
	if (readl(vector) != (u32)entry || readl(vector + 4) != entry >> 32) {
		ret = SBI_EIO;
		goto out;
	}

	/* The reset entry enables snooping before reading shared state. */
	mb();
	writel(value | release, reset);
	if (!(readl(reset) & release))
		ret = SBI_EIO;
out:
	spin_unlock(&th1520_start_lock);
	return ret;
}

/* Subsequent offline/online cycles use generic HSM parking and IPIs. */
static const struct sbi_hsm_device th1520_hsm = {
	.name = "thead-th1520",
	.hart_start = th1520_hart_start,
};

int th1520_hsm_init(void)
{
	u32 reset = readl((void *)TH1520_C910_RESET) & TH1520_C910_RESET_MASK;

	/* Older SPLs release all harts: retain generic IPI-based HSM. */
	if (reset == TH1520_C910_RESET_MASK)
		return 0;
	if (reset != (TH1520_C910_CLUSTER_RST_N | TH1520_C910_CORE_RST_N(0)) ||
	    current_hartid() != 0) {
		sbi_printf("TH1520 HSM: unexpected initial reset state 0x%x\n", reset);
		return SBI_EFAIL;
	}
	if (sbi_hsm_get_device())
		return SBI_EALREADY;

	sbi_hsm_set_device(&th1520_hsm);
	return 0;
}
