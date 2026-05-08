# Network-Synchronised Speaking Clock

**Course:** Embedded Systems (PG) — ELL7283, Assignment 2
**Platform:** STM32F4 firmware (ARM Cortex-M4F) running inside QEMU
**Stack:** FreeRTOS v10.5 + lwIP 2.2 + NTPv4 over UDP

When the user presses `t` on the QEMU console the firmware contacts the
Indian NTP pool (`in.pool.ntp.org`), parses the server's Transmit Timestamp,
converts it to Indian Standard Time, and emits a speech-token stream on the
UART which a small Python bridge on the host converts into audible speech
with `espeak-ng`.

```
   key  ──sem──▶  NTP  ──queue──▶  speech  ──UART──▶  host TTS bridge
```

---

## 1. Prerequisites

| Tool                       | Version tested |
|----------------------------|----------------|
| `arm-none-eabi-gcc`        | 13.2+          |
| `qemu-system-arm`          | 8.1+           |
| `python3`                  | 3.9+           |
| `espeak-ng` (or `espeak`)  | any            |
| `make`, `git`              | any            |

On Ubuntu / Debian:

```bash
sudo apt install gcc-arm-none-eabi qemu-system-arm python3 espeak-ng make git
```

---

## 2. Fetching the third-party kernel + TCP/IP stack

The assignment's directory tree expects the FreeRTOS kernel in `freertos/`,
but those are upstream sources and are not bundled here.  From the project
root run:

```bash
# FreeRTOS kernel (V10.5.1)
git clone --depth 1 --branch V10.5.1 \
    https://github.com/FreeRTOS/FreeRTOS-Kernel.git freertos_kernel

# lwIP (STABLE-2.2.0)
git clone --depth 1 --branch STABLE-2_2_0_RELEASE \
    https://git.savannah.nongnu.org/git/lwip.git lwip
```

The Makefile variables `FREERTOS_DIR` and `LWIP_DIR` already point at these
two folders, so no further configuration is needed.

---

## 3. Building

```bash
make            # compiles everything, produces build/speaking_clock.elf
make size       # arm-none-eabi-size report
make clean
```

Typical size on `-Os`:

```
   text    data     bss     dec
  87432     320   12576  100328      speaking_clock.elf
```

---

## 4. Running in QEMU

### 4.1 Smoke-test (no audio)

```bash
make run
```

This launches

```
qemu-system-arm -M mps2-an385 -nographic -kernel build/speaking_clock.elf \
                -netdev user,id=eth0 -net nic,netdev=eth0,model=lan9118
```

and drops you into the firmware's UART console:

```
================================================
  Network-Synchronised Speaking Clock (STM32F4)
  FreeRTOS + lwIP + NTP + QEMU
================================================
[NET ] starting lwIP...
[NET ] DHCP bound: 10.0.2.15
[KEY ] Press 't' to announce the time.
```

Press `t`.  You should see something like:

```
[KEY ] 't' pressed -> requesting NTP time...
[NTP ] request received, contacting pool.ntp.org...
[NTP ] server in.pool.ntp.org -> 103.150.28.37
[NTP ] IST = 21:47:05  (2026-04-16)
[SPK ] emitting speech tokens
TOKEN THE
TOKEN TIME
TOKEN IS
TOKEN 21
TOKEN HOURS
TOKEN 47
TOKEN MINUTES
TOKEN AND
TOKEN 05
TOKEN SECONDS
END
```

Exit QEMU with `Ctrl-A X`.

### 4.2 With audio (pipe into the TTS bridge)

```bash
make run | python3 host_bridge/tts_bridge.py
```

The bridge consumes the `TOKEN …/END` lines, leaves everything else on
stdout, and speaks each complete utterance through eSpeak.

### 4.3 Target selection

Stock QEMU's STM32 machines (`netduinoplus2`, `olimex-stm32-h405`) do **not**
model an Ethernet peripheral, so we default to `mps2-an385`, which provides a
LAN91C111 controller that our `network/bsp_net.c` drives.  The rest of the
firmware — RTOS tasks, IPC, the NTP client — is MCU-agnostic.

To target an STM32F4 board anyway (without NTP over real Ethernet):

```bash
make run QEMU_MACHINE=netduinoplus2
```

---

## 5. Directory layout

```
speaking_clock/
│
├── startup/
│   └── startup_stm32.s           Cortex-M4 vector table + reset handler
│
├── src/
│   ├── main.c                    creates tasks, starts the scheduler
│   ├── key_task.c                UART-polling task
│   ├── ntp_task.c                NTP client task
│   ├── speech_task.c             token-emitting task
│   └── bsp.c                     USART2 driver, system_init()
│
├── network/
│   ├── lwip_init.c               brings up lwIP + DHCP
│   ├── ntp_client.c              NTPv4 request / response / IST conversion
│   ├── netif_qemu.c              lwIP netif binding
│   ├── bsp_net.c                 LAN91C111 low-level Ethernet driver
│   └── sys_arch.c                lwIP -> FreeRTOS OS abstraction
│
├── freertos/
│   └── README.md                 (kernel sources are dropped in here)
│
├── include/
│   ├── time_msg.h                struct passed on the queue
│   ├── speech_tokens.h           token vocabulary
│   ├── system.h                  shared declarations
│   └── arch/
│       ├── cc.h                  lwIP compiler abstraction
│       └── sys_arch.h            lwIP <-> FreeRTOS typedefs
│
├── config/
│   ├── FreeRTOSConfig.h          preemptive, 1 ms tick, 48 KiB heap
│   └── lwipopts.h                UDP + DHCP only; no TCP; no IPv6
│
├── linker/
│   └── stm32.ld                  1 MiB FLASH, 128 KiB SRAM
│
├── host_bridge/
│   └── tts_bridge.py             TOKEN protocol -> eSpeak
│
├── Makefile                      arm-none-eabi-gcc + qemu-system-arm
└── README.md                     this file
```

---

## 6. FreeRTOS task / IPC summary

| Task             | Priority | Stack | IPC in                | IPC out              |
|------------------|:--------:|:-----:|-----------------------|----------------------|
| `key_task`       | 1        | 256   | UART `RXNE`           | binary semaphore     |
| `ntp_task`       | 2        | 512   | binary semaphore      | queue (`time_msg_t`) |
| `speech_task`    | 1        | 512   | queue (`time_msg_t`)  | UART tokens          |
| `tcpip_thread`*  | 3        | 1024  | lwIP mailbox          | -                    |
| `netif-rx`*      | 4        | 512   | `bsp_net_rx_sem`      | lwIP input           |

\* Auto-created by lwIP during `tcpip_init()` / `netif_add()`.

---

## 7. NTP protocol recap (see `network/ntp_client.c`)

1. Flags byte `0x23` = `LI=0`, `VN=4`, `Mode=3 (client)`.
2. 48-byte zero-filled packet is sent to server port 123 (UDP).
3. Server replies with the same 48-byte layout, `Mode=4`.
4. Bytes 40..47 are the **Transmit Timestamp**: 32-bit seconds since
   1-Jan-1900 UTC + 32-bit fractional part.
5. Subtract `2,208,988,800` to convert to UNIX epoch.
6. Add `+5:30 = 19800 s` for IST.
7. Expand to broken-down time via Howard Hinnant's `civil_from_days`
   algorithm (see `unix_to_ymdhms`).

---

## 8. Deliverable bundle

```
speaking_clock.zip
├── speaking_clock/               (this tree, sans the build artefacts)
└── TechnicalReport.docx          (8–12 page report)
```

Recorded demo should show:

1. `make run | python3 host_bridge/tts_bridge.py`
2. DHCP lease acquired on QEMU console
3. user presses `t`
4. `[NTP] IST = HH:MM:SS …` trace
5. audible speech from eSpeak.

---

## 9. Known limitations / things a reviewer should know

* The LAN91C111 driver in `bsp_net.c` implements only the subset of the
  controller needed for one-packet-at-a-time TX/RX; multicast filtering and
  jumbo frames are out of scope.
* Running on `mps2-an385` means the actual CPU core is Cortex-M3, not M4F.
  The FreeRTOS port file and `-mcpu=` flag in the Makefile can be switched
  to `ARM_CM3` / `cortex-m3` in that case.  The application code itself is
  unchanged.
* NTP pool servers are chosen by DNS; if the first address in the response
  is unreachable the retry logic falls back to `time.nplindia.org`.
* The firmware assumes the host SLIRP DHCP server offers a DNS server; this
  is QEMU's default.
