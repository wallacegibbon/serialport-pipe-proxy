C_SOURCE_FILES += $(wildcard ./lib/*.c)

C_INCLUDES += ./lib ./src

C_FLAGS += -I$(HOME)/.local/include

TARGET = sp-pipe

ifeq ($(shell uname), Linux)
LD_FLAGS += -static
else
C_INCLUDES += /usr/local/include
LD_FLAGS += -L/usr/local/lib
endif

LD_FLAGS += -L$(HOME)/.local/lib
LD_FLAGS += -lserialport -lpthread -lcmd_argument_parser

include cc-with-test.mk

exec: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(OBJECTS) ./src/main.c | build_dir
	@echo "CC $<"
	@$(CC) -o $@ $^ $(C_FLAGS) $(LD_FLAGS)

