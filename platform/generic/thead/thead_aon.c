// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <libfdt.h>
#include <thead/thead_aon.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_error.h>
#include "th1520_mbox.h"

struct thead_aon {
	struct th1520_mbox *mbox;
};

struct thead_aon g_thead_aon = { 0x0 };

#define AON_CHANNEL 1 //for e902

static int th1520_require_state_lpm_ctrl(struct th1520_aon_rpc_lpm_msg_hdr *msg,
					 enum th1520_aon_lpm_func func,
					 void *ack_msg, bool ack)
{

	if (!g_thead_aon.mbox) {
		sbi_printf("aon mbox not init");
		return -1;
	}
	struct th1520_aon_rpc_msg_hdr *hdr = &msg->hdr;

	RPC_SET_VER(hdr, TH1520_AON_RPC_VERSION);

	/*svc id use 6bit for version 2*/
	RPC_SET_SVC_ID(hdr, TH1520_AON_RPC_SVC_LPM);
	RPC_SET_SVC_FLAG_MSG_TYPE(hdr, RPC_SVC_MSG_TYPE_DATA);

	if (ack) {
		RPC_SET_SVC_FLAG_ACK_TYPE(hdr, RPC_SVC_MSG_NEED_ACK);
	} else {
		RPC_SET_SVC_FLAG_ACK_TYPE(hdr, RPC_SVC_MSG_NO_NEED_ACK);
	}

	hdr->func = (uint8_t)func;
	hdr->size = TH1520_AON_RPC_MSG_NUM;

	return th1520_mbox_write(g_thead_aon.mbox, AON_CHANNEL, (uint8_t *)msg,
				 sizeof(struct th1520_aon_rpc_lpm_msg_hdr));
}

int thead_aon_cpuhp(unsigned int cpu, bool status)
{
	int ret;

	struct th1520_aon_rpc_lpm_msg_hdr msg	 = { 0 };
	struct th1520_aon_rpc_ack_common ack_msg = { 0 };

	RPC_SET_BE16(&msg.rpc.cpu_info.cpu_id, 0, (u16)cpu);
	RPC_SET_BE16(&msg.rpc.cpu_info.cpu_id, 2, (u16)status);
	ret = th1520_require_state_lpm_ctrl(&msg, TH1520_AON_LPM_FUNC_CPUHP,
					    &ack_msg, false);
	if (ret) {
		sbi_printf("[%s]failed to notify aon subsys %08x\n", __func__,
			   ret);
		return ret;
	}

	return 0;
}

int thead_aon_system_suspend()
{
	int ret;
	struct th1520_aon_rpc_lpm_msg_hdr msg	 = { 0 };
	struct th1520_aon_rpc_ack_common ack_msg = { 0 };

	ret = th1520_require_state_lpm_ctrl(
		&msg, TH1520_AON_LPM_FUNC_REQUIRE_STR, &ack_msg, false);
	if (ret) {
		sbi_printf(
			"[%s]failed to notify aon subsys %08x\n", __func__,
			ret);
		return ret;
	}
	return 0;
}

int thead_aon_init()
{
	int node;
	uint64_t regs[] = { 0xffefc53000, 0xffefc3f000,
			    0xffefc47000, 0xffefc4f000 };
	const void *fdt = fdt_get_address();

	if (!fdt) {
		sbi_printf("fdt addr get faild");
		return -1;
	} else {
		sbi_printf("fdt addr get %ld\n", (uint64_t)fdt);
	}

	node = fdt_node_offset_by_compatible(fdt, -1, "xuantie,th1520-aon");
	if (node < 0)
		node = fdt_node_offset_by_compatible(fdt, -1,
						     "thead,th1520-aon");
	if (node < 0) {
		sbi_printf("aon node not found in FDT: %d\n", node);
		return SBI_ENODEV;
	}

	g_thead_aon.mbox = th1520_mbox_init(3, regs);
	if (!g_thead_aon.mbox) {
		sbi_printf("failed to initialize mbox\n");
		return SBI_ENODEV;
	}

	return 0;
}
