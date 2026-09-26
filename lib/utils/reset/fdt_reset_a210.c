/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define A210_PERI0_CLOCK_ENABLE	0x00300200UL
#define A210_PERI0_MBOX_CLOCKS	(BIT(5) | BIT(6))
#define A210_PERI1_CLOCK_ENABLE	0x02010200UL
#define A210_PERI1_MASTER_CLOCKS	(BIT(16) | BIT(17))
#define A210_RESET_ENABLE	0x30842070UL
#define A210_RESET_REQUEST	0x30846078UL
#define A210_RESET_SYSTEM	BIT(0)
#define A210_AON_READY		0xfeabdeff
#define A210_AON_RESET_RELEASE	BIT(1)
#define MBOX_GENERATE		0x10
#define MBOX_INFO(n)		(0x14 + 4 * (n))
#define MBOX_DATA_READY		BIT(6)
#define MBOX_ACK_READY		BIT(7)
#define MBOX_ACK_MAGIC		0xdeadbeaf
#define AON_RPC_VERSION		2
#define AON_RPC_WORDS		7
#define AON_WDG_SERVICE		5
#define AON_NO_REPLY		BIT(6)
#define AON_REPLY		BIT(7)
#define AON_RESTART		5
#define AON_POWER_OFF		7
#define AON_TIMEOUT_MS		1000

static void *aon_local;
static void *aon_remote;
static bool aon_ready;

static void a210_aon_ack(void)
{
	writel(0, aon_local + MBOX_INFO(0));
	writel(0, aon_local + MBOX_GENERATE);
	writel(MBOX_ACK_MAGIC, aon_remote + MBOX_INFO(7));
	/* Unlike TH1520, the A210 mailbox also accepts an ACK doorbell. */
	writel(MBOX_ACK_READY, aon_remote + MBOX_GENERATE);
}

static int a210_aon_request(u8 function)
{
	bool reply = function == AON_POWER_OFF;
	u32 header, status;
	unsigned int i;
	void *clock;

	/* SRST stops the other harts before taking over the AP mailbox. */
	clock = (void *)A210_PERI1_CLOCK_ENABLE;
	writel(readl(clock) | A210_PERI1_MASTER_CLOCKS, clock);
	clock = (void *)A210_PERI0_CLOCK_ENABLE;
	writel(readl(clock) | A210_PERI0_MBOX_CLOCKS, clock);
	readl(clock);

	/* Release a response left pending by the stopped operating system. */
	if (readl(aon_local + MBOX_INFO(0))) {
		a210_aon_ack();
		sbi_timer_mdelay(1);
	}
	for (i = 0; i < AON_TIMEOUT_MS; i++) {
		if (!readl(aon_remote + MBOX_INFO(0)))
			break;
		if (readl(aon_local + MBOX_INFO(0)))
			a210_aon_ack();
		sbi_timer_mdelay(1);
	}
	if (i == AON_TIMEOUT_MS)
		return SBI_ETIMEDOUT;

	writel(0, aon_local + MBOX_GENERATE);
	writel(0, aon_local + MBOX_INFO(0));
	writel(0, aon_local + MBOX_INFO(7));
	writel(0, aon_remote + MBOX_INFO(7));
	for (i = 1; i < AON_RPC_WORDS; i++)
		writel(0, aon_remote + MBOX_INFO(i));
	header = AON_RPC_VERSION | (AON_RPC_WORDS << 8) |
		 ((AON_WDG_SERVICE | (reply ? 0 : AON_NO_REPLY)) << 16) |
		 ((u32)function << 24);
	writel(header, aon_remote + MBOX_INFO(0));
	writel(MBOX_DATA_READY, aon_remote + MBOX_GENERATE);

	for (i = 0; i < AON_TIMEOUT_MS; i++) {
		if (readl(aon_local + MBOX_INFO(7)) == MBOX_ACK_MAGIC)
			break;
		sbi_timer_mdelay(1);
	}
	if (i == AON_TIMEOUT_MS)
		return SBI_ETIMEDOUT;
	writel(0, aon_local + MBOX_INFO(7));
	if (!reply)
		return 0;

	for (i = 0; i < AON_TIMEOUT_MS; i++) {
		if (readl(aon_local + MBOX_GENERATE) & MBOX_DATA_READY)
			break;
		sbi_timer_mdelay(1);
	}
	if (i == AON_TIMEOUT_MS)
		return SBI_ETIMEDOUT;
	header = readl(aon_local + MBOX_INFO(0));
	status = readl(aon_local + MBOX_INFO(1)) & 0xff;
	a210_aon_ack();
	if ((header & 0xff) != AON_RPC_VERSION ||
	    ((header >> 16) & 0xbf) != (AON_WDG_SERVICE | AON_REPLY) ||
	    (header >> 24) != function || status)
		return SBI_EIO;
	return 0;
}

static int a210_reset_check(u32 type, u32 reason)
{
	if (type > SBI_SRST_RESET_TYPE_WARM_REBOOT)
		return 0;
	if (type == SBI_SRST_RESET_TYPE_SHUTDOWN && !aon_ready)
		return 0;
	return aon_ready ? 2 : 1;
}

static void a210_reset(u32 type, u32 reason)
{
	bool shutdown = type == SBI_SRST_RESET_TYPE_SHUTDOWN;
	int ret;

	if (aon_ready) {
		ret = a210_aon_request(shutdown ? AON_POWER_OFF : AON_RESTART);
		if (ret)
			sbi_printf("a210-reset: AON request failed: %d\n", ret);
		else
			/* The AON restart handler has a one-second delay. */
			sbi_timer_mdelay(2000);
	}
	if (!shutdown) {
		void *enable = (void *)A210_RESET_ENABLE;

		sbi_printf("a210-reset: using system reset fallback\n");
		writel(readl(enable) | A210_RESET_SYSTEM, enable);
		writel(A210_RESET_SYSTEM, (void *)A210_RESET_REQUEST);
	}
}

static struct sbi_system_reset_device a210_reset_device = {
	.name = "a210-reset",
	.system_reset_check = a210_reset_check,
	.system_reset = a210_reset,
};

static int a210_reset_address(const void *fdt, int nodeoff,
			      const char *name, unsigned long minimum,
			      void **address)
{
	uint64_t base, size;
	int ret;

	ret = fdt_get_node_addr_size_by_name(fdt, nodeoff, name, &base, &size);
	if (ret)
		return ret;
	if (!base || (base & 3) || size < minimum || base > ~0UL - minimum)
		return SBI_EINVAL;
	*address = (void *)(unsigned long)base;
	return 0;
}

static int a210_reset_init(const void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	struct fdt_phandle_args mbox;
	void *sync, *reset;
	int index, ret;

	if (fdt_node_check_compatible(fdt, 0, "zhihe,a210"))
		return SBI_ENODEV;
	index = fdt_stringlist_search(fdt, nodeoff, "mbox-names", "aon0");
	if (index < 0)
		return SBI_EINVAL;
	ret = fdt_parse_phandle_with_args(fdt, nodeoff, "mboxes",
					 "#mbox-cells", index, &mbox);
	if (ret)
		return ret;
	if (mbox.args_count != 2 || mbox.args[0] != 1 || mbox.args[1])
		return SBI_EINVAL;
	if (!fdt_node_is_enabled(fdt, mbox.node_offset) ||
	    fdt_node_check_compatible(fdt, mbox.node_offset,
				     "zhihe,a210-mailbox-v2"))
		return SBI_ENODEV;
	ret = a210_reset_address(fdt, mbox.node_offset, "local0",
				 MBOX_INFO(7) + sizeof(u32), &aon_local);
	if (ret)
		return ret;
	ret = a210_reset_address(fdt, mbox.node_offset, "remote0",
				 MBOX_INFO(7) + sizeof(u32), &aon_remote);
	if (ret)
		return ret;
	ret = a210_reset_address(fdt, nodeoff, "aon-sync", sizeof(u32), &sync);
	if (ret)
		return ret;
	ret = a210_reset_address(fdt, nodeoff, "aon-reset", sizeof(u32), &reset);
	if (ret)
		return ret;
	aon_ready = readl(sync) == A210_AON_READY &&
		(readl(reset) & A210_AON_RESET_RELEASE) == A210_AON_RESET_RELEASE;
	sbi_system_reset_add_device(&a210_reset_device);
	return 0;
}

static const struct fdt_match a210_reset_match[] = {
	{ .compatible = "zhihe,a210-aon" },
	{ },
};

const struct fdt_driver fdt_reset_a210 = {
	.match_table = a210_reset_match,
	.init = a210_reset_init,
};
