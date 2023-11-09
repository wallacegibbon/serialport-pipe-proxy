C_SOURCE_FILES += $(wildcard ./lib/*.c)
C_SOURCE_FILES += ./src/main.c

C_INCLUDES += ./lib ./src

TARGET = sp-pipe

LD_FLAGS += -lserialport -lpthread -static

include ./miscellaneous-makefiles/simple-gcc-single.mk

