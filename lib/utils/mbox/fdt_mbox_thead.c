// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/mbox/fdt_mbox.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_console.h>

enum th1520_mbox_icu_cpu_id {
	TH1520_MBOX_ICU_CPU0, /*910T*/
	TH1520_MBOX_ICU_CPU1, /*902T*/
	TH1520_MBOX_ICU_CPU2, /*906R*/
	TH1520_MBOX_ICU_CPU3, /*910R*/
};

#define TH1520_MBOX_CHANS 4
#define TH1520_MBOX_GEN 0x10
#define TH1520_MBOX_MASK 0xc
#define TH1520_MBOX_INFO_NUM 8
#define TH1520_MBOX_INFO0 0x14
#define TH1520_MBOX_INFO7 0x30
#define TH1520_MBOX_ACK_MAGIC 0xdeadbeaf
#define TH1520_MBOX_GEN_TX_DATA 1 << 6
#define TH1520_MBOX_CHAN_RES_SIZE 0x1000
#define TH1520_ACK_MAGIC_TIMEOUT 200 //us

struct th1520_mbox_priv {
	uint32_t *cur_cpu_ch_base;
	enum th1520_mbox_icu_cpu_id cur_icu_cpu_id;
	uint32_t *comm_local_base[TH1520_MBOX_CHANS];
	uint32_t *comm_remote_base[TH1520_MBOX_CHANS];
};

struct thead_mbox_adapter {
	struct th1520_mbox_priv ctl;
	struct mbox_adapter adapter;
};

// struct th1520_mbox_priv global_priv[1] = {
//     {
//     .cur_cpu_ch_base = (uint32_t *)0xFFEFC53000,
//     .cur_icu_cpu_id = TH1520_MBOX_ICU_CPU3,
//     .comm_local_base =  {(uint32_t *)0xFFEFC50000,(uint32_t *)0xFFEFC51000,(uint32_t *)0xFFEFC52000,(uint32_t *)0xFFEFC53000},
//     .comm_remote_base = {(uint32_t *)0xFFEFC3F000,(uint32_t *)0xFFEFC47000,(uint32_t *)0xFFEFC4F000,(uint32_t *)0xFFEFC53000}
//     },
// };

/*base api*/

static inline void iowrite32(uint32_t val, uint32_t* addr)
{
	(*(volatile uint32_t*) addr) = val;
}

static inline uint32_t ioread32(uint32_t* addr)
{
	return (uint32_t)(* ((volatile uint32_t*)addr));
}

static uint32_t wj_mbox_read(struct th1520_mbox_priv *priv, uint32_t offs)
{
	return ioread32((uint32_t *)((uint64_t)priv->cur_cpu_ch_base + offs));
}

static void wj_mbox_write(struct th1520_mbox_priv *priv, uint32_t val,
			  uint32_t offs)
{
	iowrite32(val, (uint32_t *)((uint64_t)priv->cur_cpu_ch_base + offs));
}

static uint32_t wj_mbox_chan_read(struct th1520_mbox_priv *priv,
				  uint32_t channel_id, uint32_t offs,
				  bool is_remote)
{
	if (is_remote)
		return ioread32((uint32_t *)((uint64_t)(priv->comm_remote_base
								[channel_id]) +
					     offs));
	else
		return ioread32((uint32_t *)((uint64_t)(priv->comm_local_base
								[channel_id]) +
					     offs));
}

static void wj_mbox_chan_write(struct th1520_mbox_priv *priv,
			       uint32_t channel_id, uint32_t val, uint32_t offs,
			       bool is_remote)
{
	if (is_remote)
		iowrite32(val, (uint32_t *)((uint64_t)(priv->comm_remote_base
							       [channel_id]) +
					    offs));
	else
		iowrite32(val, (uint32_t *)((uint64_t)(priv->comm_local_base
							       [channel_id]) +
					    offs));
}

static void wj_mbox_chan_rmw(struct th1520_mbox_priv *priv, uint32_t channel_id,
			     uint32_t off, uint32_t set, uint32_t clr,
			     bool is_remote)
{
	uint32_t val;
	val = wj_mbox_chan_read(priv, channel_id, off, is_remote);
	val &= ~clr;
	val |= set;
	wj_mbox_chan_write(priv, channel_id, val, off, is_remote);
}

static void wj_mbox_chan_wr_ack(struct th1520_mbox_priv *priv,
				uint32_t channel_id, void *data, bool is_remote)
{
	uint32_t off  = TH1520_MBOX_INFO7;
	uint32_t *arg = data;
	wj_mbox_chan_write(priv, channel_id, *arg, off, is_remote);
}

static void wj_mbox_chan_wr_data(struct th1520_mbox_priv *priv,
				 uint32_t channel_id, const void *data,
				 bool is_remote)
{
	uint32_t off	    = TH1520_MBOX_INFO0;
	const uint32_t *arg = data;
	uint32_t i;
	/*write info0~info6,totaly 28 bytes
     * requires data memory is 28bytes valid data
     */
	for (i = 0; i < TH1520_MBOX_INFO_NUM; i++) {
		wj_mbox_chan_write(priv, channel_id, *arg, off, is_remote);
		off += 4;
		arg++;
	}
}

static uint32_t wj_mbox_rmw(struct th1520_mbox_priv *priv, uint32_t offs,
			    uint32_t set, uint32_t clr)
{
	uint32_t val;
	val = wj_mbox_read(priv, offs);
	val &= ~clr;
	val |= set;
	wj_mbox_write(priv, val, offs);
	return val;
}

static int thead_mbox_adapter_write(struct mbox_adapter *ia,
				    uint8_t remote_channel, uint8_t *buffer,
				    int len)
{
	uint32_t count = 0;
	uint32_t ack   = 0;
	// fix me :len not used
	struct thead_mbox_adapter *adap =
		container_of(ia, struct thead_mbox_adapter, adapter);

	/* set value */
	wj_mbox_chan_wr_data(&adap->ctl, remote_channel, buffer, true);
	wj_mbox_chan_rmw(&adap->ctl, remote_channel, TH1520_MBOX_GEN,
			 TH1520_MBOX_GEN_TX_DATA, 0, true);
	do {
		ack = wj_mbox_chan_read(&adap->ctl, remote_channel,
					TH1520_MBOX_INFO7, false);
		sbi_timer_udelay(1);
		count++;
	} while (ack != TH1520_MBOX_ACK_MAGIC &&
		 count <= TH1520_ACK_MAGIC_TIMEOUT);

	if (ack != TH1520_MBOX_ACK_MAGIC) {
		sbi_printf("mbox send faild, no ack magic receive %d\n", count);
		return SBI_ETIMEDOUT;
	} else {
		//clear ack
		wj_mbox_chan_rmw(&adap->ctl, remote_channel, TH1520_MBOX_INFO7,
				 0, 0, false);
	}

	return 0;
}

static int thead_mbox_adapter_read(struct mbox_adapter *ia,
				   uint8_t *src_channel, uint8_t *buffer,
				   int *len, int timeout_us)
{
	// struct thead_mbox_adapter *adap =
	// 	container_of(ia, struct thead_mbox_adapter, adapter);
	// int rc;
	return 0;
}

static int thead_mbox_init(void *fdt, int nodeoff,
			   const struct fdt_match *match)
{
	int rc = 0, bitmap = 0, len = 0;
	struct thead_mbox_adapter *adapter;
	uint8_t data = 0x0;

	adapter = sbi_zalloc(sizeof(*adapter));
	if (!adapter)
		return SBI_ENOMEM;

	const fdt32_t *icu_cpu_id =
		fdt_getprop(fdt, nodeoff, "icu_cpu_id", &len);
	if (!icu_cpu_id) {
		sbi_printf("Failed to get 'icu_cpu_id' property\n");
		goto faild;
	}

	adapter->ctl.cur_icu_cpu_id = fdt32_to_cpu(icu_cpu_id[0]);
	sbi_printf("Mbox icu_cpu_id: %u\n", adapter->ctl.cur_icu_cpu_id);

	const fdt32_t *reg = fdt_getprop(fdt, nodeoff, "reg", &len);
	if (!reg) {
		sbi_printf("Failed to get 'reg' property\n");
		goto faild;
	}

	adapter->ctl.cur_cpu_ch_base =
		(uint32_t *)fdt64_to_cpu(((const fdt64_t *)reg)[0]);

	for (int i = 0; i < TH1520_MBOX_CHANS; i++) {
		if (i >= adapter->ctl.cur_icu_cpu_id) {
			adapter->ctl.comm_local_base[i] =
				(uint32_t *)((uint64_t)adapter->ctl
						     .cur_cpu_ch_base +
					     (i - adapter->ctl.cur_icu_cpu_id) *
						     TH1520_MBOX_CHAN_RES_SIZE);
		} else {
			adapter->ctl.comm_local_base[i] =
				(uint32_t *)((uint64_t)adapter->ctl
						     .cur_cpu_ch_base -
					     (adapter->ctl.cur_icu_cpu_id - i) *
						     TH1520_MBOX_CHAN_RES_SIZE);
		}
	}

	adapter->ctl.comm_remote_base[adapter->ctl.cur_icu_cpu_id] =
		adapter->ctl.cur_cpu_ch_base;
	adapter->ctl.comm_remote_base[0] =
		(uint32_t *)fdt64_to_cpu(((const fdt64_t *)reg)[2]);
	adapter->ctl.comm_remote_base[1] =
		(uint32_t *)fdt64_to_cpu(((const fdt64_t *)reg)[4]);
	adapter->ctl.comm_remote_base[2] =
		(uint32_t *)fdt64_to_cpu(((const fdt64_t *)reg)[6]);

	for (int i = 0; i < TH1520_MBOX_CHANS; i++) {
		if (i == adapter->ctl.cur_icu_cpu_id)
			continue;
		/*clear local and remote generate and info0~info7*/
		wj_mbox_chan_rmw(&adapter->ctl, i, TH1520_MBOX_GEN, 0x0, 0xff,
				 true);
		wj_mbox_chan_rmw(&adapter->ctl, i, TH1520_MBOX_GEN, 0x0, 0xff,
				 false);
		wj_mbox_chan_wr_ack(&adapter->ctl, i, &data, true);
		wj_mbox_chan_wr_data(&adapter->ctl, i, &data, false);
		/*enable the chan mask*/
		wj_mbox_rmw(&adapter->ctl, TH1520_MBOX_MASK, BIT(bitmap), 0);
		bitmap++;
	}
	adapter->adapter.id    = nodeoff;
	adapter->adapter.write = thead_mbox_adapter_write;
	adapter->adapter.read  = thead_mbox_adapter_read;
	rc		       = mbox_adapter_add(&adapter->adapter);
	if (rc) {
		goto faild;
	}
	return 0;
faild:
	sbi_free(adapter);

	return -1;
}

static const struct fdt_match mbox_thead_match[] = {
	{ .compatible = "xuantie,th1520-mbox-r" },
	{},
};

struct fdt_mbox_adapter fdt_mbox_thead = {
	.match_table = mbox_thead_match,
	.init	     = thead_mbox_init,
};
