C_SOURCE_FILES += $(wildcard ./lib/*.c)
C_SOURCE_FILES += ./src/main.c

C_INCLUDES += ./lib ./src

TARGET = sp-pipe

ifeq ($(shell uname), Linux)
LD_FLAGS += -static
else
C_INCLUDES += /usr/local/include
LD_FLAGS += -L/usr/local/lib
endif

LD_FLAGS += -lserialport -lpthread -lcmd_argument_parser

include ./miscellaneous-makefiles/simple-gcc-single.mk

