// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (C) 2021-2024 Alibaba Group Holding Limited.
 * Copyright (C) 2025 Icenowy Zheng <uwu@icenowy.me>
 *
 * Authors:
 *   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
 */

#ifndef __MBOX_H__
#define __MBOX_H__

#include <sbi/sbi_types.h>
#include <sbi/sbi_list.h>

struct th1520_mbox;

int th1520_mbox_write(struct th1520_mbox *ia, uint8_t remote_channel,
		      uint8_t *buffer, int len);

int th1520_mbox_read(struct th1520_mbox *ia, uint8_t *src_channel,
		     uint8_t *buffer, int *len, int timeout_us);

struct th1520_mbox *th1520_mbox_init(uint32_t icu_cpu_id, uint64_t *reg);

#endif
