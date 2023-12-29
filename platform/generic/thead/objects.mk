#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (C) 2023 Inochi Amaoto <inochiama@outlook.com>
# Copyright (C) 2023 Alibaba Group Holding Limited.
#

carray-platform_override_modules-$(CONFIG_PLATFORM_THEAD_GENERIC) += thead_generic
platform-objs-$(CONFIG_PLATFORM_THEAD_GENERIC) += thead/thead-generic.o
platform-objs-$(CONFIG_PLATFORM_THEAD_GENERIC) += thead/thead-patch.o
platform-objs-$(CONFIG_PLATFORM_THEAD_GENERIC) += thead/light_c910.o
