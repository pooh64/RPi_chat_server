CFLAGS := -c -g -O2 -Wall -std=gnu99 -MD
LDFLAGS :=

BUILD_DIR := build

all: echoloop

-include $(BUILD_DIR)/*.d

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

ECHOLOOP_SRC := echoloop.c sopbuf.c strlist.c
ECHOLOOP_OBJ := $(addprefix $(BUILD_DIR)/,$(ECHOLOOP_SRC:.c=.o))

.PHONY: echoloop
echoloop: $(BUILD_DIR)/echoloop
$(BUILD_DIR)/echoloop: $(ECHOLOOP_OBJ)
	$(CC) $(LDFLAGS) $(ECHOLOOP_OBJ) -o $@
