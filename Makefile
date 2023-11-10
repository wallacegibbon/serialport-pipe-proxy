C_SOURCE_FILES += $(wildcard ./lib/*.c)
C_SOURCE_FILES += ./src/main.c

C_INCLUDES += ./lib ./src

TARGET = sp-pipe

#LD_FLAGS += -lserialport -lpthread -static
LD_FLAGS += -lserialport -lpthread

include ./miscellaneous-makefiles/simple-gcc-single.mk

