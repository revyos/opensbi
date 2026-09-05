# SPDX-License-Identifier: BSD-2-Clause

# A210 has RV64 C908/C920 harts and RV64-only reset/cache setup.
ifeq ($(PLATFORM_RISCV_XLEN), 64)
carray-platform_override_modules-$(CONFIG_PLATFORM_ZHIHE_A210) += zhihe_a210
platform-objs-$(CONFIG_PLATFORM_ZHIHE_A210) += zhihe/a210.o
platform-objs-$(CONFIG_PLATFORM_ZHIHE_A210) += zhihe/a210_warm.o
platform-objs-$(CONFIG_PLATFORM_ZHIHE_A210) += zhihe/a210_hsm.o
endif
