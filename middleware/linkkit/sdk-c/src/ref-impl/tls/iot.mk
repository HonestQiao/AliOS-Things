LIBA_TARGET := libiot_tls.a
CFLAGS      := $(filter-out -ansi,$(CFLAGS))
HDR_REFS    += src/infra

LIB_SRC_PATTERN := *.c */*.c
