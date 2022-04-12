NAME := lwip

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION := 0.0.1
$(NAME)_SUMMARY := lightweight tcp/ip stack

#ifeq (y,$(FEATURE_LWIP_ENABLED))
ifneq ($(no_with_lwip),1)
GLOBAL_DEFINES += WITH_LWIP
with_lwip := 1
endif
#endif

#ifeq (y,$(FEATURE_USE_AOS_LWIP))
ifneq ($(use_private_lwip),1)
include network/lwip/Filelists.mk

GLOBAL_INCLUDES += include port/include

GLOBAL_DEFINES += CONFIG_NET_LWIP

$(NAME)_INCLUDES += port/include

$(NAME)_SOURCES := $(COREFILES)
$(NAME)_SOURCES += $(CORE4FILES)
$(NAME)_SOURCES += $(CORE6FILES)
$(NAME)_SOURCES += $(APIFILES)
$(NAME)_SOURCES += $(NETIFFILES)

ifeq (y,$(FEATURE_AF_PACKET_ENABLED))
#define LWIP_PACKET                     1
endif

ifeq (y,$(FEATURE_SNMP_ENABLED))
$(NAME)_SOURCES += $(SNMPFILES)
endif

ifeq (y,$(FEATURE_HTTPD_ENABLED))
$(NAME)_SOURCES += $(HTTPDFILES)
endif

ifeq (y,$(FEATURE_LWIPERF_ENABLED))
$(NAME)_SOURCES += $(LWIPERFFILES)
endif

ifeq (y,$(FEATURE_SNTP_ENABLED))
$(NAME)_SOURCES += $(SNTPFILES)
endif

ifeq (y,$(FEATURE_MDNS_ENABLED))
$(NAME)_SOURCES += $(MDNSFILES)
endif

ifeq (y,$(FEATURE_NETBIOSNS_ENABLED))
$(NAME)_SOURCES += $(NETBIOSNSFILES)
endif

ifeq (y,$(FEATURE_TFTP_ENABLED))
use_lwip_tftp := 0
ifeq ($(use_lwip_tftp), 1)
GLOBAL_DEFINES += WITH_LWIP_TFPT
$(NAME)_SOURCES += $(TFTPFILES)
endif
endif

ifeq (y,$(FEATURE_TELNETD_ENABLED))
use_private_telnetd ?= 1
ifneq ($(use_private_telnetd), 1)
GLOBAL_DEFINES += WITH_LWIP_TELNETD
$(NAME)_SOURCES += $(TELNETDFILES)
endif
endif

ifeq (y,$(FEATURE_DHCPD_ENABLED))
#default use the private dhcpd
use_private_dhcpd := 1
ifneq ($(use_private_dhcpd), 1)
$(NAME)_SOURCES += $(DHCPDFILES)
endif
endif
$(NAME)_SOURCES += port/sys_arch.c


endif
#endif

