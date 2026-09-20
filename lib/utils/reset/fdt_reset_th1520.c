/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (C) 2026 Han Gao <gaohan@iscas.ac.cn> */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/fdt/fdt_driver.h>
#include <sbi_utils/fdt/fdt_helper.h>

#define TH1520_APB3_CLOCK_ENABLE	0xffef010208UL
#define TH1520_WDT0_BASE		0xffefc30000UL
#define TH1520_WDT0_CLOCK		BIT(3)
#define TH1520_RESET_REQUEST_EN	0xfffff44140UL
#define TH1520_WDT0_SYSTEM_RESET	BIT(8)
#define TH1520_E902_RESET		0xfffff44024UL
#define TH1520_MBOX_CLOCKS	(BIT(6) | BIT(7))
#define TH1520_MBOX_LOCAL_OFFSET	0x1000
#define TH1520_MBOX_REMOTE_OFFSET	0x4000
#define TH1520_MBOX_AON_CHANNEL	1
#define MBOX_GENERATE		0x10
#define MBOX_INFO(n)		(0x14 + 4 * (n))
#define MBOX_DATA_READY		BIT(6)
#define MBOX_ACK_MAGIC		0xdeadbeaf
/* v2 watchdog service: seven-word request, one-word reply. */
#define AON_REQUEST_HEADER	0x00050702
#define AON_RESPONSE_HEADER	0x00c50102
#define AON_RPC_WORDS		7
#define AON_RESTART		5
#define AON_POWER_OFF		7
#define DW_WDT_CONTROL		0x00
#define DW_WDT_TIMEOUT_RANGE	0x04
#define DW_WDT_COUNTER_RESTART	0x0c
#define DW_WDT_ENABLE		BIT(0)
#define DW_WDT_RESTART_KEY	0x76

static void *aon_local;
static void *aon_remote;

static void th1520_wdt_reset(void)
{
	void *clock = (void *)TH1520_APB3_CLOCK_ENABLE;
	void *wdt = (void *)TH1520_WDT0_BASE;
	void *request = (void *)TH1520_RESET_REQUEST_EN;

	/* The OS may gate WDT0 while it is unused. */
	writel(readl(clock) | TH1520_WDT0_CLOCK, clock);
	readl(clock);
	/* Watchdog expiry must also reach the system-reset controller. */
	writel(readl(request) | TH1520_WDT0_SYSTEM_RESET, request);

	writel(0, wdt + DW_WDT_TIMEOUT_RANGE);
	writel(DW_WDT_ENABLE, wdt + DW_WDT_CONTROL);
	writel(DW_WDT_RESTART_KEY, wdt + DW_WDT_COUNTER_RESTART);
}

static void th1520_aon_ack(void)
{
	writel(0, aon_local + MBOX_INFO(0));
	writel(readl(aon_local + MBOX_GENERATE) & ~MBOX_DATA_READY,
	       aon_local + MBOX_GENERATE);
	/* E902 polls INFO7 instead of receiving an acknowledgement interrupt. */
	writel(MBOX_ACK_MAGIC, aon_remote + MBOX_INFO(7));
}

static int th1520_aon_request(u8 function)
{
	void *clock = (void *)TH1520_APB3_CLOCK_ENABLE;
	u32 header, status;
	unsigned int i;

	/* SRST has stopped the other harts; poll the mailbox directly. */
	writel(readl(clock) | TH1520_MBOX_CLOCKS, clock);
	if (readl(aon_local + MBOX_GENERATE) & MBOX_DATA_READY) {
		th1520_aon_ack();
		sbi_timer_udelay(1000);
	}
	writel(readl(aon_remote + MBOX_GENERATE) & ~MBOX_DATA_READY,
	       aon_remote + MBOX_GENERATE);
	writel(0, aon_local + MBOX_INFO(0));
	writel(0, aon_local + MBOX_INFO(7));
	writel(0, aon_remote + MBOX_INFO(7));
	for (i = 1; i < AON_RPC_WORDS; i++)
		writel(0, aon_remote + MBOX_INFO(i));
	header = ((u32)function << 24) | AON_REQUEST_HEADER;
	writel(header, aon_remote + MBOX_INFO(0));
	writel(readl(aon_remote + MBOX_GENERATE) | MBOX_DATA_READY,
	       aon_remote + MBOX_GENERATE);
	for (i = 0; i < 1000; i++) {
		if (readl(aon_local + MBOX_GENERATE) & MBOX_DATA_READY)
			break;
		sbi_timer_udelay(1000);
	}
	if (i == 1000)
		return SBI_ETIMEDOUT;

	/* The doorbell publishes the complete response, not just INFO0. */
	header = readl(aon_local + MBOX_INFO(0));
	status = readl(aon_local + MBOX_INFO(1)) & 0xff;
	th1520_aon_ack();
	if (header != (((u32)function << 24) | AON_RESPONSE_HEADER) || status)
		return SBI_EIO;
	return 0;
}

static int th1520_reset_check(u32 type, u32 reason)
{
	if (type > SBI_SRST_RESET_TYPE_WARM_REBOOT ||
	    (type == SBI_SRST_RESET_TYPE_SHUTDOWN && !aon_local))
		return 0;
	return aon_local ? 2 : 1;
}

static void th1520_reset(u32 type, u32 reason)
{
	bool shutdown = type == SBI_SRST_RESET_TYPE_SHUTDOWN;

	if (aon_local &&
	    !th1520_aon_request(shutdown ? AON_POWER_OFF : AON_RESTART))
		sbi_timer_mdelay(1000);
	if (!shutdown)
		th1520_wdt_reset();
}

static struct sbi_system_reset_device th1520_reset_device = {
	.name = "th1520-reset",
	.system_reset_check = th1520_reset_check,
	.system_reset = th1520_reset,
};

static int th1520_mbox_address(const void *fdt, int nodeoff,
			       const char *name, unsigned long offset,
			       void **address)
{
	uint64_t base, size;
	unsigned long end = offset + MBOX_INFO(7) + sizeof(u32);
	int ret;

	ret = fdt_get_node_addr_size_by_name(fdt, nodeoff, name, &base, &size);
	if (ret)
		return ret;
	if (!base || (base & 3) || size < end || base > ~0UL - end)
		return SBI_EINVAL;

	*address = (void *)(unsigned long)(base + offset);
	return 0;
}

static int th1520_reset_init(const void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	struct fdt_phandle_args mbox;
	void *local, *remote;
	int index, ret;

	if (fdt_node_check_compatible(fdt, 0, "thead,th1520"))
		return SBI_ENODEV;
	if (!nodeoff) {
		sbi_system_reset_add_device(&th1520_reset_device);
		return 0;
	}
	index = fdt_stringlist_search(fdt, nodeoff, "mbox-names", "aon");
	if (index < 0)
		return SBI_EINVAL;
	ret = fdt_parse_phandle_with_args(fdt, nodeoff, "mboxes",
					 "#mbox-cells", index, &mbox);
	if (ret)
		return ret;
	if (mbox.args_count != 1 || mbox.args[0] != TH1520_MBOX_AON_CHANNEL)
		return SBI_EINVAL;
	if (!fdt_node_is_enabled(fdt, mbox.node_offset) ||
	    fdt_node_check_compatible(fdt, mbox.node_offset,
				      "thead,th1520-mbox"))
		return SBI_ENODEV;

	/* Channel 1 uses the E902 windows within the mailbox apertures. */
	ret = th1520_mbox_address(fdt, mbox.node_offset, "local",
				  TH1520_MBOX_LOCAL_OFFSET, &local);
	if (ret)
		return ret;
	ret = th1520_mbox_address(fdt, mbox.node_offset, "remote-icu0",
				  TH1520_MBOX_REMOTE_OFFSET, &remote);
	if (ret)
		return ret;
	/* A loader without AON firmware still has the WDT provider. */
	if ((readl((void *)TH1520_E902_RESET) & 3) != 3)
		return 0;

	aon_local = local;
	aon_remote = remote;
	return 0;
}

static const struct fdt_match th1520_reset_match[] = {
	{ .compatible = "thead,th1520" },
	{ .compatible = "thead,th1520-aon" },
	{ },
};

const struct fdt_driver fdt_reset_th1520 = {
	.match_table = th1520_reset_match,
	.init = th1520_reset_init,
};
