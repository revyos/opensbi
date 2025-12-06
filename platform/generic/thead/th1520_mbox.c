// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <libfdt.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_heap.h>
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

struct th1520_mbox {
	uint32_t *cur_cpu_ch_base;
	enum th1520_mbox_icu_cpu_id cur_icu_cpu_id;
	uint32_t *comm_local_base[TH1520_MBOX_CHANS];
	uint32_t *comm_remote_base[TH1520_MBOX_CHANS];
};

/*base api*/

static inline void iowrite32(uint32_t val, uint32_t* addr)
{
	(*(volatile uint32_t*) addr) = val;
}

static inline uint32_t ioread32(uint32_t* addr)
{
	return (uint32_t)(* ((volatile uint32_t*)addr));
}

static uint32_t wj_mbox_read(struct th1520_mbox *priv, uint32_t offs)
{
	return ioread32((uint32_t *)((uint64_t)priv->cur_cpu_ch_base + offs));
}

static void wj_mbox_write(struct th1520_mbox *priv, uint32_t val,
			  uint32_t offs)
{
	iowrite32(val, (uint32_t *)((uint64_t)priv->cur_cpu_ch_base + offs));
}

static uint32_t wj_mbox_chan_read(struct th1520_mbox *priv,
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

static void wj_mbox_chan_write(struct th1520_mbox *priv,
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

static void wj_mbox_chan_rmw(struct th1520_mbox *priv, uint32_t channel_id,
			     uint32_t off, uint32_t set, uint32_t clr,
			     bool is_remote)
{
	uint32_t val;
	val = wj_mbox_chan_read(priv, channel_id, off, is_remote);
	val &= ~clr;
	val |= set;
	wj_mbox_chan_write(priv, channel_id, val, off, is_remote);
}

static void wj_mbox_chan_wr_ack(struct th1520_mbox *priv,
				uint32_t channel_id, void *data, bool is_remote)
{
	uint32_t off  = TH1520_MBOX_INFO7;
	uint32_t *arg = data;
	wj_mbox_chan_write(priv, channel_id, *arg, off, is_remote);
}

static void wj_mbox_chan_wr_data(struct th1520_mbox *priv,
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

static uint32_t wj_mbox_rmw(struct th1520_mbox *priv, uint32_t offs,
			    uint32_t set, uint32_t clr)
{
	uint32_t val;
	val = wj_mbox_read(priv, offs);
	val &= ~clr;
	val |= set;
	wj_mbox_write(priv, val, offs);
	return val;
}

int th1520_mbox_write(struct th1520_mbox *ia,
				    uint8_t remote_channel, uint8_t *buffer,
				    int len)
{
	uint32_t count = 0;
	uint32_t ack   = 0;

	/* set value */
	wj_mbox_chan_wr_data(ia, remote_channel, buffer, true);
	wj_mbox_chan_rmw(ia, remote_channel, TH1520_MBOX_GEN,
			 TH1520_MBOX_GEN_TX_DATA, 0, true);
	do {
		ack = wj_mbox_chan_read(ia, remote_channel,
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
		wj_mbox_chan_rmw(ia, remote_channel, TH1520_MBOX_INFO7,
				 0, 0, false);
	}

	return 0;
}

int th1520_mbox_read(struct th1520_mbox *ia,
				   uint8_t *src_channel, uint8_t *buffer,
				   int *len, int timeout_us)
{
	return 0;
}

struct th1520_mbox *th1520_mbox_init(uint32_t icu_cpu_id, uint64_t *reg)
{
	int bitmap = 0;
	struct th1520_mbox *adapter;
	uint32_t data = 0x0;

	adapter = sbi_zalloc(sizeof(*adapter));
	if (!adapter)
		return NULL;

	adapter->cur_icu_cpu_id = icu_cpu_id;
	sbi_printf("Mbox icu_cpu_id: %u\n", adapter->cur_icu_cpu_id);

	adapter->cur_cpu_ch_base = (uint32_t *)reg[0];

	for (int i = 0; i < TH1520_MBOX_CHANS; i++) {
		if (i >= adapter->cur_icu_cpu_id) {
			adapter->comm_local_base[i] =
				(uint32_t *)((uint64_t)adapter->cur_cpu_ch_base +
					     (i - adapter->cur_icu_cpu_id) *
						     TH1520_MBOX_CHAN_RES_SIZE);
		} else {
			adapter->comm_local_base[i] =
				(uint32_t *)((uint64_t)adapter->cur_cpu_ch_base -
					     (adapter->cur_icu_cpu_id - i) *
						     TH1520_MBOX_CHAN_RES_SIZE);
		}
	}

	adapter->comm_remote_base[adapter->cur_icu_cpu_id] =
		adapter->cur_cpu_ch_base;
	adapter->comm_remote_base[0] = (uint32_t *)reg[1];
	adapter->comm_remote_base[1] = (uint32_t *)reg[2];
	adapter->comm_remote_base[2] = (uint32_t *)reg[3];

	for (int i = 0; i < TH1520_MBOX_CHANS; i++) {
		if (i == adapter->cur_icu_cpu_id)
			continue;
		/*clear local and remote generate and info0~info7*/
		wj_mbox_chan_rmw(adapter, i, TH1520_MBOX_GEN, 0x0, 0xff,
				 true);
		wj_mbox_chan_rmw(adapter, i, TH1520_MBOX_GEN, 0x0, 0xff,
				 false);
		wj_mbox_chan_wr_ack(adapter, i, &data, true);
		wj_mbox_chan_wr_data(adapter, i, &data, false);
		/*enable the chan mask*/
		wj_mbox_rmw(adapter, TH1520_MBOX_MASK, BIT(bitmap), 0);
		bitmap++;
	}

	return adapter;
}
