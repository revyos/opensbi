#
# SPDX-License-Identifier: BSD-2-Clause
#

carray-platform_override_modules-$(CONFIG_PLATFORM_ALLWINNER_D1) += sun20i_d1
platform-objs-$(CONFIG_PLATFORM_ALLWINNER_D1) += allwinner/sun20i-d1.o

carray-platform_override_modules-$(CONFIG_PLATFORM_ALLWINNER_V861) += sun252i_v861
platform-objs-$(CONFIG_PLATFORM_ALLWINNER_V861) += allwinner/sun252i-v861.o
