# FreeRTOS Kernel Sources

This folder is **reserved for the FreeRTOS kernel source files** listed in the
assignment's required directory structure (§7):

```
freertos/
    tasks.c
    queue.c
    list.c
```

These files are **upstream FreeRTOS code** and are not redistributed inside
this repository to keep the submission focused on my own work.  They must be
obtained from the official FreeRTOS repository and placed here before the
project will link.

---

## How to populate this folder

### Option 1 — Clone the official kernel (recommended)

```bash
# From the root of this project:
git clone --depth 1 --branch V10.5.1 \
    https://github.com/FreeRTOS/FreeRTOS-Kernel.git freertos_kernel
```

This creates a sibling folder `freertos_kernel/` that the top-level Makefile
points at via the `FREERTOS_DIR` variable.  The kernel C files
(`tasks.c`, `queue.c`, `list.c`, `timers.c`, and the heap + port files)
are compiled directly from there.

### Option 2 — Copy the three required files here

If your submission workflow requires the files to physically sit under
`freertos/`, copy them after cloning:

```bash
cp freertos_kernel/tasks.c freertos/
cp freertos_kernel/queue.c freertos/
cp freertos_kernel/list.c  freertos/
```

…and adjust the `RTOS_SRC` variable in the Makefile accordingly.

---

## Port / heap files used

| File                                        | Purpose                   |
|---------------------------------------------|---------------------------|
| `portable/GCC/ARM_CM4F/port.c`              | Cortex-M4F context switch |
| `portable/GCC/ARM_CM4F/portmacro.h`         | port macros               |
| `portable/MemMang/heap_4.c`                 | heap allocator (48 KiB)   |

The kernel is configured by `../config/FreeRTOSConfig.h`.
