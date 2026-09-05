/* SPDX-License-Identifier: BSD-2-Clause */
#include <sbi/riscv_asm.h>
#include <sbi/riscv_io.h>
#include <sbi/riscv_locks.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <zhihe/a210.h>

extern void a210_start_warm(void);

unsigned long a210_hsm_start_addr[A210_HART_COUNT];
static DEFINE_SPIN_LOCK(a210_start_lock);
static bool c920_attached;

static const unsigned long a210_vector_base[] = {
	A210_C908_RVBA_BASE, A210_C920_RVBA_BASE,
};
static const unsigned long a210_reset_control[] = {
	A210_C908_RESET_CONTROL, A210_C920_RESET_CONTROL,
};

/* Called with the start lock held, before any C920 core is released. */
static int a210_attach_c920(void *control, u32 value)
{
	unsigned int retry;

	if (c920_attached)
		return 0;
	if (value & A210_CORE_RESET_MASK)
		return SBI_EFAIL;

	/* Keep all core resets asserted while the cluster joins the fabric. */
	writel(value | A210_RELEASE_CLUSTER_ONLY, control);
	if (!(readl(control) & A210_RELEASE_CLUSTER_ONLY))
		return SBI_EIO;
	writel(A210_ACE_ATTACH_REQUEST,
	       (void *)(A210_NCORE_ACE1 + A210_XAIUTCR));
	for (retry = 0; retry < A210_ACE_ATTACH_RETRIES; retry++) {
		if (readl((void *)(A210_NCORE_ACE1 + A210_XAIUTAR)) &
		    A210_ACE_ATTACH_ACK) {
			c920_attached = true;
			return 0;
		}
	}
	/* No cores were released. Do not reset a fabric with unknown state. */
	return SBI_ETIMEDOUT;
}

static int a210_hart_start(u32 hartid, unsigned long saddr)
{
	unsigned long trampoline = (unsigned long)a210_start_warm;
	unsigned int cluster, core;
	void *vector, *control;
	u32 value, release, high;
	int ret = 0;

	/* Hart 0 is already running; this platform supports only single-die DEV. */
	if (!hartid || hartid >= A210_HART_COUNT || !saddr || (saddr & 1))
		return SBI_EINVAL;
	cluster = hartid / A210_CORES_PER_CLUSTER;
	core = hartid % A210_CORES_PER_CLUSTER;
	vector = (void *)(a210_vector_base[cluster] + core * A210_RVBA_STRIDE);
	control = (void *)a210_reset_control[cluster];
	release = A210_CORE_RESET_RELEASE(core);
	high = trampoline >> A210_RVBA_HIGH_SHIFT;

	spin_lock(&a210_start_lock);
	value = readl(control);
	/* Never reset or rewrite the vector of an unexpectedly running core. */
	if (value & release) {
		ret = SBI_EFAIL;
		goto out;
	}

	/* Honor HSM's M-mode entry address after per-core cache setup. */
	a210_hsm_start_addr[hartid] = saddr;
	writel((u32)trampoline, vector);
	writel(high, vector + A210_RVBA_HIGH_OFFSET);
	if (readl(vector) != (u32)trampoline ||
	    readl(vector + A210_RVBA_HIGH_OFFSET) != high) {
		ret = SBI_EIO;
		goto out;
	}

	if (cluster == A210_C920_CLUSTER) {
		ret = a210_attach_c920(control, value);
		if (ret)
			goto out;
	} else if (!(value & A210_RELEASE_CLUSTER_ONLY)) {
		ret = SBI_EFAIL;
		goto out;
	}

	/* Publish the target address, scratch state and reset vector first. */
	asm volatile("fence iorw, iorw" ::: "memory");
	value = readl(control);
	writel(value | A210_RELEASE_CLUSTER_ONLY | release, control);
	if (!(readl(control) & release))
		ret = SBI_EIO;
out:
	spin_unlock(&a210_start_lock);
	return ret;
}

/*
 * A start-only device: generic HSM invokes this for a hart's first start.
 * With no hart_stop callback, later offline/online cycles park in generic
 * warmboot and wake by IPI. Do not claim power-off or suspend support.
 */
static const struct sbi_hsm_device a210_hsm = {
	.name = "zhihe-a210",
	.hart_start = a210_hart_start,
};

int a210_hsm_init(void)
{
	u32 c908, c920;

	if (sbi_hsm_get_device())
		return SBI_EALREADY;
	c908 = readl((void *)A210_C908_RESET_CONTROL);
	c920 = readl((void *)A210_C920_RESET_CONTROL);
	/* This path requires SPL to leave all seven secondary harts in reset. */
	if ((c908 & A210_RELEASE_CLUSTER_AND_CORES) !=
	    (A210_RELEASE_CLUSTER_ONLY | A210_CORE_RESET_RELEASE(0)) ||
	    (c920 & A210_CORE_RESET_MASK)) {
		sbi_printf("A210 HSM: unexpected initial reset state c908=0x%x c920=0x%x\n",
			   c908, c920);
		return SBI_EFAIL;
	}
	sbi_hsm_set_device(&a210_hsm);
	return 0;
}
