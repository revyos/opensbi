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

struct thead_aon {
	struct mbox_adapter *mbox;
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

	return mbox_adapter_write(g_thead_aon.mbox, AON_CHANNEL, (uint8_t *)msg,
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
	int node, mbox_node, err;
	const fdt32_t *property;
	uint32_t phandle;
	const void *fdt = fdt_get_address();

	if (!fdt) {
		sbi_printf("fdt addr get faild");
		return -1;
	} else {
		sbi_printf("fdt addr get %ld\n", (uint64_t)fdt);
	}

	node = fdt_node_offset_by_compatible(fdt, -1, "xuantie,th1520-aon");
	if (node < 0) {
		sbi_printf("aon node not found in FDT: %d\n", node);
		return SBI_ENODEV;
	}

	property = fdt_getprop(fdt, node, "opensbi-mboxes", &err);
	if (!property) {
		sbi_printf("Failed to get 'opensbi-mboxes' property: %d\n",
			   err);
		return SBI_ENODEV;
	}

	phandle	  = fdt32_to_cpu(property[0]);
	mbox_node = fdt_node_offset_by_phandle(fdt, phandle);
	if (mbox_node < 0) {
		sbi_printf("MBOX node not found: %d\n", mbox_node);
		return SBI_ENODEV;
	}

	err = fdt_mbox_adapter_get(fdt, mbox_node, &g_thead_aon.mbox);
	if (err) {
		sbi_printf("mbox adapter get faild %d\n", err);
		return -1;
	}

	return 0;
}
