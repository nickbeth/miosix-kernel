##
## Makefile for Miosix networking stack
## Network stack files are exported in NET_SRC
##

NET_DIR := $(KPATH)/network

# Variable needed by lwIP makefiles
LWIPDIR := $(NET_DIR)/lwip/src
include $(LWIPDIR)/Filelists.mk

# lwIP source files + Miosix porting layer
NET_SRC := $(COREFILES)                         \
           $(CORE4FILES)                        \
           $(NETIFFILES)                        \
           $(APIFILES)                          \
           $(NET_DIR)/port/sys_arch.cpp         \
           $(NET_DIR)/network.cpp

# lwIP driver source files
NET_SRC += $(NET_DIR)/drivers/stm32_ethif.cpp
