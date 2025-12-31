// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#include <sbi_utils/mbox/fdt_mbox.h>

#ifndef __IPC_H_
#define __IPC_H_

#define TH1520_AON_RPC_MSG_NUM 7
#define TH1520_AON_RPC_VERSION 2
#define RPC_SVC_MSG_TYPE_DATA 0
#define RPC_SVC_MSG_TYPE_ACK 1
#define RPC_SVC_MSG_NEED_ACK 0
#define RPC_SVC_MSG_NO_NEED_ACK 1

#define RPC_GET_VER(MESG) ((MESG)->ver)
#define RPC_SET_VER(MESG, VER) ((MESG)->ver = (VER))
#define RPC_GET_SVC_ID(MESG) ((MESG)->svc & 0x3F)
#define RPC_SET_SVC_ID(MESG, ID) ((MESG)->svc |= 0x3F & (ID))
#define RPC_GET_SVC_FLAG_MSG_TYPE(MESG) (((MESG)->svc & 0x80) >> 7)
#define RPC_SET_SVC_FLAG_MSG_TYPE(MESG, TYPE) ((MESG)->svc |= (TYPE) << 7)
#define RPC_GET_SVC_FLAG_ACK_TYPE(MESG) (((MESG)->svc & 0x40) >> 6)
#define RPC_SET_SVC_FLAG_ACK_TYPE(MESG, ACK) ((MESG)->svc |= (ACK) << 6)

#define RPC_SET_BE64(MESG, OFFSET, SET_DATA)                              \
	do {                                                              \
		uint8_t *data	 = (uint8_t *)(MESG);                     \
		data[OFFSET + 7] = (SET_DATA)&0xFF;                       \
		data[OFFSET + 6] = ((SET_DATA)&0xFF00) >> 8;              \
		data[OFFSET + 5] = ((SET_DATA)&0xFF0000) >> 16;           \
		data[OFFSET + 4] = ((SET_DATA)&0xFF000000) >> 24;         \
		data[OFFSET + 3] = ((SET_DATA)&0xFF00000000) >> 32;       \
		data[OFFSET + 2] = ((SET_DATA)&0xFF0000000000) >> 40;     \
		data[OFFSET + 1] = ((SET_DATA)&0xFF000000000000) >> 48;   \
		data[OFFSET + 0] = ((SET_DATA)&0xFF00000000000000) >> 56; \
	} while (0)

#define RPC_SET_BE32(MESG, OFFSET, SET_DATA)                      \
	do {                                                      \
		uint8_t *data	 = (uint8_t *)(MESG);             \
		data[OFFSET + 3] = (SET_DATA)&0xFF;               \
		data[OFFSET + 2] = ((SET_DATA)&0xFF00) >> 8;      \
		data[OFFSET + 1] = ((SET_DATA)&0xFF0000) >> 16;   \
		data[OFFSET + 0] = ((SET_DATA)&0xFF000000) >> 24; \
	} while (0)
#define RPC_SET_BE16(MESG, OFFSET, SET_DATA)                 \
	do {                                                 \
		uint8_t *data	 = (uint8_t *)(MESG);        \
		data[OFFSET + 1] = (SET_DATA)&0xFF;          \
		data[OFFSET + 0] = ((SET_DATA)&0xFF00) >> 8; \
	} while (0)
#define RPC_SET_U8(MESG, OFFSET, SET_DATA)         \
	do {                                       \
		uint8_t *data = (uint8_t *)(MESG); \
		data[OFFSET]  = (SET_DATA)&0xFF;   \
	} while (0)
#define RPC_GET_BE64(MESG, OFFSET, PTR)                                    \
	do {                                                               \
		uint8_t *data = (uint8_t *)(MESG);                         \
		*(uint32_t *)(PTR) =                                       \
			(data[OFFSET + 7] | data[OFFSET + 6] << 8 |        \
			 data[OFFSET + 5] << 16 | data[OFFSET + 4] << 24 | \
			 data[OFFSET + 3] << 32 | data[OFFSET + 2] << 40 | \
			 data[OFFSET + 1] << 48 | data[OFFSET + 0] << 56); \
	} while (0)
#define RPC_GET_BE32(MESG, OFFSET, PTR)                                    \
	do {                                                               \
		uint8_t *data = (uint8_t *)(MESG);                         \
		*(uint32_t *)(PTR) =                                       \
			(data[OFFSET + 3] | data[OFFSET + 2] << 8 |        \
			 data[OFFSET + 1] << 16 | data[OFFSET + 0] << 24); \
	} while (0)
#define RPC_GET_BE16(MESG, OFFSET, PTR)                             \
	do {                                                        \
		uint8_t *data = (uint8_t *)(MESG);                  \
		*(uint16_t *)(PTR) =                                \
			(data[OFFSET + 1] | data[OFFSET + 0] << 8); \
	} while (0)
#define RPC_GET_U8(MESG, OFFSET, PTR)                  \
	do {                                           \
		uint8_t *data	  = (uint8_t *)(MESG); \
		*(uint8_t *)(PTR) = (data[OFFSET]);    \
	} while (0)

struct th1520_aon_rpc_msg_hdr {
	uint8_t ver; ///< version of msg hdr
	uint8_t size; ///< msg size ,uinit in bytes,the size includes rpc msg header self.
	uint8_t svc;  ///< rpc main service id
	uint8_t func; ///< rpc sub func id of specific service, sent by caller
} __packed __aligned(1);

struct rpc_msg_cpu_info {
	u16 cpu_id;
	u16 status;
	u32 reserved[5];
} __packed __aligned(1);

struct th1520_aon_rpc_lpm_msg_hdr {
	struct th1520_aon_rpc_msg_hdr hdr;
	union rpc_func_t {
		struct rpc_msg_cpu_info cpu_info;
	} __packed __aligned(1) rpc;
} __packed __aligned(1);

struct th1520_aon_rpc_ack_common {
	struct th1520_aon_rpc_msg_hdr hdr;
	u8 err_code;
} __packed __aligned(1);

enum th1520_aon_rpc_svc {
	TH1520_AON_RPC_SVC_UNKNOWN = 0,
	TH1520_AON_RPC_SVC_PM	   = 1,
	TH1520_AON_RPC_SVC_MISC	   = 2,
	TH1520_AON_RPC_SVC_AVFS	   = 3,
	TH1520_AON_RPC_SVC_SYS	   = 4,
	TH1520_AON_RPC_SVC_WDG	   = 5,
	TH1520_AON_RPC_SVC_LPM	   = 6,
	TH1520_AON_RPC_SVC_MAX	   = 0x3F,
};

enum th1520_aon_lpm_func {
	TH1520_AON_LPM_FUNC_UNKNOWN	= 0,
	TH1520_AON_LPM_FUNC_REQUIRE_STR = 1,
	TH1520_AON_LPM_FUNC_RESUME_STR	= 2,
	TH1520_AON_LPM_FUNC_REQUIRE_STD = 3,
	TH1520_AON_LPM_FUNC_CPUHP	= 4,
	TH1520_AON_LPM_FUNC_REGDUMP_CFG = 5,
};

int thead_aon_init();
int thead_aon_cpuhp(unsigned int cpu, bool status);
int thead_aon_system_suspend();

#endif