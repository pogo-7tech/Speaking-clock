# ============================================================
#  Makefile  -  Network-Synchronised Speaking Clock
# ------------------------------------------------------------
#  Build:       make
#  Clean:       make clean
#  Run QEMU:    make run
#  Debug:       make debug      (QEMU with gdbserver on :3333)
#
#  Requires:
#     arm-none-eabi-gcc        (13.x or newer)
#     qemu-system-arm          (8.x or newer)
#     python3                  (for the host TTS bridge)
#
#  External sources expected at the paths below:
#     $(FREERTOS_DIR)          FreeRTOS kernel (v10.5+)
#     $(LWIP_DIR)              lwIP 2.2.x
#  Both are submodules / unpacked tarballs; see README.md.
# ============================================================

# ---- Toolchain ----------------------------------------------
CROSS    ?= arm-none-eabi-
CC       := $(CROSS)gcc
AS       := $(CROSS)gcc -x assembler-with-cpp
LD       := $(CROSS)gcc
OBJCOPY  := $(CROSS)objcopy
SIZE     := $(CROSS)size

# ---- Target -------------------------------------------------
TARGET   := speaking_clock
BUILD    := build

# ---- External source trees ----------------------------------
FREERTOS_DIR ?= ./freertos_kernel
LWIP_DIR     ?= ./lwip

# ---- CPU / arch ---------------------------------------------
CPU      := -mcpu=cortex-m3
FPU      :=
ARCH     := $(CPU) $(FPU) -mthumb

# ---- Include paths ------------------------------------------
INCS := \
    -Iinclude -Iconfig -Isrc -Inetwork \
    -I$(FREERTOS_DIR)/include \
    -I$(FREERTOS_DIR)/portable/GCC/ARM_CM3 \
    -I$(LWIP_DIR)/src/include \
    -I$(LWIP_DIR)/src/include/ipv4 \
    -I$(LWIP_DIR)/port/include

# ---- Compile flags ------------------------------------------
CFLAGS  := $(ARCH) -Os -g3 -Wall -Wextra -Wno-unused-parameter \
           -ffunction-sections -fdata-sections \
           -fno-common -fsingle-precision-constant \
           -DSTM32F405xx -DUSE_FULL_LL_DRIVER \
           $(INCS)

ASFLAGS := $(ARCH) -g3

LDSCRIPT := linker/stm32.ld
LDFLAGS  := $(ARCH) -T$(LDSCRIPT) \
            -Wl,--gc-sections -Wl,-Map,$(BUILD)/$(TARGET).map \
            --specs=nano.specs --specs=nosys.specs \
            -Wl,--start-group -lc -lm -lgcc -Wl,--end-group

# ---- Sources ------------------------------------------------
APP_SRC := \
    src/main.c         \
    src/fault.c        \
    src/key_task.c     \
    src/ntp_task.c     \
    src/speech_task.c  \
    src/bsp.c          \
    network/lwip_init.c    \
    network/ntp_client.c   \
    network/netif_qemu.c   \
    network/bsp_net.c      \
    network/sys_arch.c

# FreeRTOS kernel (expect source drop-in)
RTOS_SRC := \
    $(FREERTOS_DIR)/tasks.c  \
    $(FREERTOS_DIR)/queue.c  \
    $(FREERTOS_DIR)/list.c   \
    $(FREERTOS_DIR)/timers.c \
    $(FREERTOS_DIR)/portable/MemMang/heap_4.c \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM3/port.c

# lwIP core (a sensible subset; adjust if you build a different drop)
LWIP_SRC := \
    $(LWIP_DIR)/src/core/init.c       \
    $(LWIP_DIR)/src/core/def.c        \
    $(LWIP_DIR)/src/core/dns.c        \
    $(LWIP_DIR)/src/core/inet_chksum.c \
    $(LWIP_DIR)/src/core/ip.c         \
    $(LWIP_DIR)/src/core/mem.c        \
    $(LWIP_DIR)/src/core/memp.c       \
    $(LWIP_DIR)/src/core/netif.c      \
    $(LWIP_DIR)/src/core/pbuf.c       \
    $(LWIP_DIR)/src/core/raw.c        \
    $(LWIP_DIR)/src/core/stats.c      \
    $(LWIP_DIR)/src/core/sys.c        \
    $(LWIP_DIR)/src/core/tcp.c        \
    $(LWIP_DIR)/src/core/timeouts.c   \
    $(LWIP_DIR)/src/core/udp.c        \
    $(LWIP_DIR)/src/core/ipv4/autoip.c \
    $(LWIP_DIR)/src/core/ipv4/dhcp.c  \
    $(LWIP_DIR)/src/core/ipv4/etharp.c $(LWIP_DIR)/src/core/ipv4/acd.c \
    $(LWIP_DIR)/src/core/ipv4/icmp.c  \
    $(LWIP_DIR)/src/core/ipv4/igmp.c  \
    $(LWIP_DIR)/src/core/ipv4/ip4_addr.c \
    $(LWIP_DIR)/src/core/ipv4/ip4.c   \
    $(LWIP_DIR)/src/core/ipv4/ip4_frag.c \
    $(LWIP_DIR)/src/api/api_lib.c     \
    $(LWIP_DIR)/src/api/api_msg.c     \
    $(LWIP_DIR)/src/api/err.c         \
    $(LWIP_DIR)/src/api/netbuf.c      \
    $(LWIP_DIR)/src/api/netdb.c       \
    $(LWIP_DIR)/src/api/netifapi.c    \
    $(LWIP_DIR)/src/api/sockets.c     \
    $(LWIP_DIR)/src/api/tcpip.c       \
    $(LWIP_DIR)/src/netif/ethernet.c

ASM_SRC := startup/startup_stm32.s

C_SRC   := $(APP_SRC) $(RTOS_SRC) $(LWIP_SRC)

OBJS := $(C_SRC:%.c=$(BUILD)/%.o) $(ASM_SRC:%.s=$(BUILD)/%.o)

# ============================================================
#  Rules
# ============================================================

.PHONY: all clean run debug size

all: $(BUILD)/$(TARGET).elf size

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(OBJS) $(LDFLAGS) -o $@
	$(OBJCOPY) -O binary $@ $(BUILD)/$(TARGET).bin
	$(OBJCOPY) -O ihex   $@ $(BUILD)/$(TARGET).hex

size: $(BUILD)/$(TARGET).elf
	@$(SIZE) $<

# ------------------------------------------------------------
#  QEMU launch
# ------------------------------------------------------------
#  Default machine: mps2-an385 (Cortex-M3 + LAN91C111 Ethernet).
#  Stock QEMU STM32 machines (netduinoplus2, olimex-stm32) do
#  NOT model Ethernet, so we use mps2-an385 whose LAN91C111
#  controller we drive from network/bsp_net.c.
#
#  To target an STM32F4 board instead, override QEMU_MACHINE
#  on the command line, e.g.
#      make run QEMU_MACHINE=netduinoplus2
#  (NTP will then fail unless you provide your own Ethernet
#   MAC or enable SIMULATE_NTP at build time.)
# ------------------------------------------------------------
QEMU          := qemu-system-arm
QEMU_MACHINE  ?= mps2-an385
QEMU_OPTS     := -M $(QEMU_MACHINE) -nographic -semihosting \
                 -kernel $(BUILD)/$(TARGET).elf \
                 -netdev user,id=eth0 \
                 -net nic,netdev=eth0,model=lan9118

run: $(BUILD)/$(TARGET).elf
	@echo ">> launching QEMU (Ctrl-A X to exit)"
	$(QEMU) $(QEMU_OPTS)

debug: $(BUILD)/$(TARGET).elf
	$(QEMU) $(QEMU_OPTS) -s -S

clean:
	rm -rf $(BUILD)
