#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/ti/tirex-content/tirtos_tivac_2_16_00_08/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/bios_6_45_01_29/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/ndk_2_25_00_09/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/uia_2_00_05_50/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/ns_1_11_00_10/packages;C:/ti/ccsv6/ccs_base;C:/Users/bob/workspace_v6_2/DTC1200_TivaTM4C123AE6PM/.config
override XDCROOT = C:/ti/xdctools_3_32_01_22_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/ti/tirex-content/tirtos_tivac_2_16_00_08/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/tidrivers_tivac_2_16_00_08/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/bios_6_45_01_29/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/ndk_2_25_00_09/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/uia_2_00_05_50/packages;C:/ti/tirex-content/tirtos_tivac_2_16_00_08/products/ns_1_11_00_10/packages;C:/ti/ccsv6/ccs_base;C:/Users/bob/workspace_v6_2/DTC1200_TivaTM4C123AE6PM/.config;C:/ti/xdctools_3_32_01_22_core/packages;..
HOSTOS = Windows
endif
