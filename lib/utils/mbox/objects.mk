#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2021-2024 Alibaba Group Holding Limited.
#
# Authors:
#   XiaoJin Chai <xiaojin.cxj@alibaba-inc.com>
#

libsbiutils-objs-$(CONFIG_FDT_MBOX) += mbox/mbox.o
libsbiutils-objs-$(CONFIG_FDT_MBOX) += mbox/fdt_mbox.o
libsbiutils-objs-$(CONFIG_FDT_MBOX) += mbox/fdt_mbox_adapter_drivers.o

carray-fdt_mbox_drivers-$(CONFIG_FDT_MBOX_THEAD) += fdt_mbox_thead
libsbiutils-objs-$(CONFIG_FDT_MBOX_THEAD) += mbox/fdt_mbox_thead.o
