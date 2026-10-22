// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define A210_RESET_ENABLE	0x30842070UL
#define A210_RESET_REQUEST	0x30846078UL
#define A210_RESET_SYSTEM	BIT(0)
#define MBOX_GENERATE		0x10
#define MBOX_DATA_READY		BIT(6)
#define SMT_STATUS		0x04
#define SMT_FLAGS		0x10
#define SMT_LENGTH		0x14
#define SMT_HEADER		0x18
#define SMT_PAYLOAD		0x1c
#define SMT_FREE		BIT(0)
#define SMT_SIZE		256
#define SCMI_SYSTEM_POWER	0x12
#define SCMI_VERSION		0
#define SCMI_STATE_SET		3
#define SCMI_SHUTDOWN		0
#define SCMI_COLD_RESET		1
#define SCMI_POLL_US		100
#define SCMI_TIMEOUT_US		250000

static void *scmi_shmem;
static void *scmi_doorbell;
static u32 scmi_token;
static bool scmi_ready;

static int a210_scmi_wait(void)
{
	unsigned int elapsed;

	for (elapsed = 0; elapsed < SCMI_TIMEOUT_US; elapsed += SCMI_POLL_US) {
		if (readl(scmi_shmem + SMT_STATUS) & SMT_FREE)
			return 0;
		sbi_timer_udelay(SCMI_POLL_US);
	}
	return SBI_ETIMEDOUT;
}

static int a210_scmi_request(u32 command, const u32 *args, unsigned int count)
{
	u32 header = ((scmi_token++ & 0x3ff) << 18) |
		     (SCMI_SYSTEM_POWER << 10) | command;
	unsigned int i;
	int ret;

	ret = a210_scmi_wait();
	if (ret)
		return ret;
	writel(0, scmi_shmem + SMT_STATUS);
	writel(0, scmi_shmem + SMT_FLAGS);
	writel(sizeof(u32) * (count + 1), scmi_shmem + SMT_LENGTH);
	writel(header, scmi_shmem + SMT_HEADER);
	for (i = 0; i < count; i++)
		writel(args[i], scmi_shmem + SMT_PAYLOAD + sizeof(u32) * i);
	__asm__ volatile ("fence iorw, iorw" ::: "memory");
	writel(MBOX_DATA_READY, scmi_doorbell + MBOX_GENERATE);
	ret = a210_scmi_wait();
	if (ret)
		return ret;
	__asm__ volatile ("fence iorw, iorw" ::: "memory");
	if (readl(scmi_shmem + SMT_STATUS) != SMT_FREE ||
	    readl(scmi_shmem + SMT_HEADER) != header ||
	    readl(scmi_shmem + SMT_LENGTH) < 2 * sizeof(u32) ||
	    readl(scmi_shmem + SMT_LENGTH) > SMT_SIZE - SMT_HEADER ||
	    (readl(scmi_shmem + SMT_LENGTH) & 3) ||
	    readl(scmi_shmem + SMT_PAYLOAD))
		return SBI_EIO;
	return 0;
}

static int a210_reset_check(u32 type, u32 reason)
{
	if (type > SBI_SRST_RESET_TYPE_WARM_REBOOT)
		return 0;
	if (type == SBI_SRST_RESET_TYPE_SHUTDOWN && !scmi_ready)
		return 0;
	return scmi_ready ? 2 : 1;
}

static int a210_wait_for_harts(void)
{
	struct sbi_hartmask harts;
	u32 index, local = sbi_hartid_to_hartindex(current_hartid());
	unsigned int elapsed;
	int ret;

	ret = sbi_domain_get_assigned_hartmask(sbi_domain_thishart_ptr(), &harts);
	if (ret)
		return ret;
	sbi_hartmask_clear_hartindex(local, &harts);
	/* The generic reset path sends HALT IPIs without waiting for them. */
	for (elapsed = 0; elapsed < SCMI_TIMEOUT_US; elapsed += SCMI_POLL_US) {
		bool stopped = true;

		sbi_hartmask_for_each_hartindex(index, &harts) {
			if (__sbi_hsm_hart_get_state(index) == SBI_HSM_STATE_STOPPED)
				sbi_hartmask_clear_hartindex(index, &harts);
			else
				stopped = false;
		}
		if (stopped)
			return 0;
		sbi_timer_udelay(SCMI_POLL_US);
	}
	return SBI_ETIMEDOUT;
}

static void a210_reset(u32 type, u32 reason)
{
	bool shutdown = type == SBI_SRST_RESET_TYPE_SHUTDOWN;
	u32 args[] = {0, shutdown ? SCMI_SHUTDOWN : SCMI_COLD_RESET};
	int ret;

	if (scmi_ready) {
		ret = a210_wait_for_harts();
		if (!ret)
			ret = a210_scmi_request(SCMI_STATE_SET, args, array_size(args));
		sbi_printf("a210-reset: SCMI transition returned: %d\n", ret);
	}
	if (!shutdown) {
		void *enable = (void *)A210_RESET_ENABLE;

		/* Keep recovery possible when the management core has stopped. */
		sbi_printf("a210-reset: using system reset fallback\n");
		writel(readl(enable) | A210_RESET_SYSTEM, enable);
		writel(A210_RESET_SYSTEM, (void *)A210_RESET_REQUEST);
	}
}

static struct sbi_system_reset_device a210_reset_device = {
	.name = "a210-scmi-reset",
	.system_reset_check = a210_reset_check,
	.system_reset = a210_reset,
};

static int a210_reset_init(const void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	struct fdt_phandle_args mbox;
	const fdt32_t *shmem;
	u64 base, size;
	int index, len, node, ret;

	if (fdt_node_check_compatible(fdt, 0, "zhihe,a210"))
		return SBI_ENODEV;
	index = fdt_stringlist_search(fdt, nodeoff, "mbox-names", "tx");
	if (index < 0)
		return SBI_EINVAL;
	ret = fdt_parse_phandle_with_args(fdt, nodeoff, "mboxes", "#mbox-cells", index, &mbox);
	if (ret)
		return ret;
	if (mbox.args_count != 1 || mbox.args[0] ||
	    !fdt_node_is_enabled(fdt, mbox.node_offset) ||
	    fdt_node_check_compatible(fdt, mbox.node_offset, "zhihe,a210-ap-mailbox"))
		return SBI_EINVAL;
	ret = fdt_get_node_addr_size_by_name(fdt, mbox.node_offset, "tx", &base, &size);
	if (ret || !base || (base & 3) || size < MBOX_GENERATE + sizeof(u32) ||
	    base > ~0UL - size)
		return SBI_EINVAL;
	scmi_doorbell = (void *)(unsigned long)base;
	/* The first shmem is the A2P channel; a P2A channel may follow. */
	shmem = fdt_getprop(fdt, nodeoff, "shmem", &len);
	if (!shmem || len < sizeof(*shmem))
		return SBI_EINVAL;
	node = fdt_node_offset_by_phandle(fdt, fdt32_to_cpu(*shmem));
	if (node < 0 || fdt_node_check_compatible(fdt, node, "arm,scmi-shmem"))
		return SBI_EINVAL;
	ret = fdt_get_node_addr_size(fdt, node, 0, &base, &size);
	if (ret || !base || (base & 3) || size < SMT_SIZE || base > ~0UL - size)
		return SBI_EINVAL;
	scmi_shmem = (void *)(unsigned long)base;
	scmi_ready = !a210_scmi_request(SCMI_VERSION, NULL, 0) &&
		readl(scmi_shmem + SMT_LENGTH) == 3 * sizeof(u32) &&
		(readl(scmi_shmem + SMT_PAYLOAD + sizeof(u32)) >> 16) >= 1;
	sbi_system_reset_add_device(&a210_reset_device);
	return 0;
}

static const struct fdt_match a210_reset_match[] = {
	{ .compatible = "arm,scmi" },
	{ },
};

const struct fdt_driver fdt_reset_a210 = {
	.match_table = a210_reset_match,
	.init = a210_reset_init,
};
