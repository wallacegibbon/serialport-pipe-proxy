C_SOURCE_FILES += $(wildcard ./lib/*.c)
C_SOURCE_FILES += ./src/main.c

C_INCLUDES += ./lib ./src

TARGET = sp-pipe

LD_FLAGS += -lserialport -lpthread -lcmd_argument_parser

ifeq ($(shell uname), Linux)
LD_FLAGS += -static
endif

include ./miscellaneous-makefiles/simple-gcc-single.mk

