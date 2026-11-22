# 04 — Root-Cause Analysis & Bugfixes (`3.10.49` & `3.10.108`)

During the reverse-engineering, `3.10.49` reconstruction, and `3.10.108` LTS upgrade of the LYF Flame 8 (`LS-4505`), we isolated and fixed **8 distinct boot-blocking and runtime kernel bugs** across the Android compiler toolchain, ARM architecture code, ALSA SoC core, and Qualcomm/MStar vendor drivers.

---

## 1. Android GCC `-fpic` Emitting Orphan `.data.rel*` Sections After `_edata` (Early Boot Hang at `t = 0.000s`)

### Symptom
Kernel compiled cleanly with zero errors using Android `arm-linux-androideabi-gcc` (4.8 and 4.9), `aboot` loaded `zImage-dtb`, and the phone froze silently on the static splash logo before `console_init()`.

### Root Cause
Android cross-compilers define `__ANDROID__=1` and `__PIC__=1` (`-fpic`) by default. Under `-fpic`, GCC places initialized structs containing function/data pointers (`struct clock_ops`, `struct sys_timer`, `struct of_device_id`, `initcall` tables — **812 KB** in total!) into `.data.rel`, `.data.rel.ro`, and `.data.rel.local` sections.
Because `arch/arm/kernel/vmlinux.lds.S` in Linux `3.10` does not explicitly list `.data.rel*` inside `.data`, GNU `ld` placed those orphan `.data.rel*` sections **after `_edata` / `__bss_start`**! At `t = 0.000000s`, `__mmap_switched` (`arch/arm/kernel/head-common.S`) zeroes memory from `__bss_start` (`_edata`) through `_end`, **wiping 812 KB of initialized kernel pointer tables to `0x00000000`**!

### Fix (`Makefile`)
Added `-mno-android -fno-pic` to `KBUILD_CFLAGS` and `KBUILD_AFLAGS`:
```makefile
KBUILD_CFLAGS   := -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs \
                   -fno-strict-aliasing -fno-common \
                   -mno-android -fno-pic \
...
KBUILD_AFLAGS   := -D__ASSEMBLY__ -mno-android -fno-pic
```
Verified via `arm-linux-androideabi-readelf -S vmlinux`: **zero** `.data.rel*` sections remain, and uncompressed `Image` size matched stock `Image` within `1.5 KB` (`18,869,572` vs `18,868,020` bytes).

---

## 2. SELinux `POLICYDB_VERSION_MAX` Mismatch (`LA.BR.1.2.4` Lollipop vs `LA.BR.1.2.7` Marshmallow)

### Symptom
Earlier community MSM8909 trees (`android_kernel_alcatel_msm8909`, based on CAF `LA.BR.1.2.4` Android 5.1) fail to load Android 6.0.1 `/sepolicy` during `/init` stage 1.

### Root Cause & Fix
LS-4505's stock Marshmallow `/sepolicy` is compiled with SELinux policy version **`30`**, whereas `LA.BR.1.2.4` only supports up to `POLICYDB_VERSION_MAX = 28`. Rebasing onto official Qualcomm CodeLinaro `LA.BR.1.2.7_rb1.28` (`POLICYDB_VERSION_MAX = 30`) resolved `/sepolicy` loading and achieved `0` symbol CRC diffs with stock Marshmallow modules.

---

## 3. CAF `LA.BR.1.2.7_rb1.52` Userspace Vendor Blob Divergence

### Finding (Preserved on `testing/ls4505-3.10.108-rb1.52`)
Between `LA.BR.1.2.7_rb1.28` (Sep 2016, matching LS-4505's stock `MMB29M` ROM) and `LA.BR.1.2.7_rb1.52` (Nov 2017), Qualcomm modified 488 files (`+54,807 / -11,246` lines), including `drivers/gpu/msm/kgsl*`, `drivers/video/msm/mdss/*`, `drivers/media/platform/msm/camera_v2/*`, and `msm8909-ion.dtsi` (deleting `SYSTEM_CONTIG` heap 21). Merging `v3.10.108` directly onto `LA.BR.1.2.7_rb1.28` preserved 100% binary compatibility with the phone's `/system/vendor/lib` Adreno KGSL, HWC, Camera, and RIL HALs while bringing in every upstream `3.10.108` core kernel fix.

---

## 4. ARM `virt_addr_valid()` + `pfn_valid()` `BUG_ON` on Carved-Out MSM8909 Memory

### Root Cause
Upstream `v3.10.x` commit `efb1f1d96f` (`ARM: 8060/1: mm: allow virt_addr_valid() to check for pfn_valid()`) added `&& pfn_valid(__pa(kaddr) >> PAGE_SHIFT)` to `virt_addr_valid()` in `arch/arm/include/asm/memory.h`.
On 32-bit Qualcomm MSM8909, `memblock_remove()` punches non-`pfn_valid` holes inside the lowmem linear map for PIL/Modem/Pronto/QSEECOM DMA buffers (`0x87a00000 – 0x8da00000`), which Qualcomm drivers subsequently pass to `sg_init_one()` -> `sg_set_buf()`, hitting `BUG_ON(!virt_addr_valid(buf));` in `include/linux/scatterlist.h:115`!

### Fix (`arch/arm/include/asm/memory.h`)
Preserved Qualcomm CAF's `virt_addr_valid(kaddr)` definition (`(unsigned long)(kaddr) >= PAGE_OFFSET && (unsigned long)(kaddr) < (unsigned long)high_memory`).

---

## 5. Upstream ASoC `soc_new_compress()` Breaking Qualcomm Bidirectional Compress DAI -> 400 ms Timer UAF (`PC = 0x0`)

### Captured `ramoops` Crash Trace (`t = 8.093s`)
```text
[    7.682412] msm8x16-asoc-wcd sound.70: Invalid direction for compress P 1, C 1
[    7.697348] msm8x16-asoc-wcd: probe of sound.70 failed with error -22
...
[    8.093677] Unable to handle kernel NULL pointer dereference at virtual address 00000000
[    8.093781] PC is at 0x0
[    8.093796] LR is at call_timer_fn+0xac/0x1e4
```

### Root Cause & Fix (`sound/soc/soc-compress.c`)
Upstream `3.10.108` added `if (playback + capture != 1) return -EINVAL;` in `soc_new_compress()`. Qualcomm's `msm8x16-asoc-wcd` pairs `snd-soc-dummy-dai` (`playback = 1, capture = 1`) with `msm-compr-dsp`, causing `snd_soc_register_card()` to abort with `-EINVAL` (`-22`) *after* scheduling a 400 ms jack/MBHC `timer_list` inside `devm_kzalloc` memory! When `devm` freed the failed card struct without `del_timer_sync()`, `run_timer_softirq` jumped to `timer->function == NULL` (`PC = 0x0`) 400 ms later at `t = 8.093s`. Restoring CAF's `soc_new_compress()` direction selection both registered `msm8909-skue-snd-card` cleanly and eliminated the timer UAF.

---

## 6. MStar `MSG2238` Touchscreen Recursive Spinlock Deadlock on Screen Timeout (`t = 59.11s`)

### Captured `ramoops` Console Trace
```text
[   59.112773] i2c-msm-v2 78b9000.i2c: NACK: slave not responding, ensure its powered: ... slv_addr:0x26
[   59.112811] IicReadData() error -107
<instant SMP CPU deadlock -> MSM hardware watchdog bite>
```

### Root Cause
In MStar's vendor driver (`drivers/input/touchscreen/msg2238/mstar_drv_platform_porting_layer.c`):
1. Both `_DrvPlatformLyrFingerTouchDoWork()` and `DrvPlatformLyrEnableFingerTouchReport()` called **`enable_irq(_gIrq)` while holding `spin_lock_irqsave(&_gIrqLock, nIrqFlag)`**!
2. During screen-off suspend, `DrvPlatformLyrDisableFingerTouchReport()` did not cancel in-flight `_gFingerTouchWork` before `DrvPlatformLyrTouchDevicePowerOff()` pulled `reset_gpio` / `vdd` low (which latched a pending edge on `irq_gpio` `GPIO 13` / `IRQ 924`).
3. Under `3.10.108`'s `kernel/irq/resend.c`, calling `enable_irq(_gIrq)` with a latched pending interrupt synchronously invokes `_DrvPlatformLyrFingerTouchInterruptHandler()` on the **same CPU**, which attempts `spin_lock_irqsave(&_gIrqLock, nIrqFlag)` while `_gIrqLock` is already held -> **instant recursive spinlock self-deadlock with IRQs disabled**!

### Fix (`mstar_drv_platform_porting_layer.c`)
- Added `_gTpShuttingDown` guard and `synchronize_irq(_gIrq)` + `cancel_work_sync(&_gFingerTouchWork)` inside `DrvPlatformLyrDisableFingerTouchReport()` before regulator power-off.
- Moved all `enable_irq(_gIrq)` calls **outside** `spin_lock_irqsave(&_gIrqLock, nIrqFlag)` (and replaced `disable_irq` with `disable_irq_nosync`).

---

## 7. Qualcomm `SMB1360` Charger `wakeup_source` Use-After-Free on `-EPROBE_DEFER` (`t = 60.82s`)

### Captured `ramoops` Crash Trace
```text
[    1.686267] smb1360-chg-fg 4-0014: Unable to parse DT nodes
[    1.686297] i2c 4-0014: Driver smb1360-chg-fg requests probe deferral
[    1.887847] smb1360-chg-fg 4-0014: SMB1360 revision=0x3 probe success!
...
[   60.826275] Unable to handle kernel NULL pointer dereference at virtual address 00000008
[   60.826802] [<c0a313d4>] (_raw_spin_lock_irqsave+0x18/0xa8) from [<c04859d0>] (wakeup_sources_stats_show+0x48/0x260)
```

### Root Cause
In Qualcomm's `drivers/power/smb1360-charger-fg.c`:
1. `smb1360_probe()` allocated `chip` with `devm_kzalloc()`, immediately called `smb1360_wakeup_src_init(chip)` -> `wakeup_source_init(&chip->smb1360_ws.source, "smb1360")` (linking `&chip->smb1360_ws.source.entry` into the global `wakeup_sources` RCU list), and **13 lines later** returned `-EPROBE_DEFER` from `smb_parse_dt(chip)` **without calling `wakeup_source_trash()`**!
2. `devm` immediately `kfree`d `chip` while its `list_head` remained linked at the tail of `wakeup_sources`!
3. ~59 seconds later, another kernel subsystem `kzalloc`'d that freed slab slot (zeroing `entry.next = NULL`), and when Android `system_server` (`android.fg`) read `/sys/kernel/debug/wakeup_sources`, `list_for_each_entry_rcu` stepped onto `container_of(NULL, struct wakeup_source, entry)` (`0xfffffffc`) and dereferenced `&ws->lock` (`0x00000008`)!

### Fix (`drivers/power/smb1360-charger-fg.c` & `drivers/base/power/wakeup.c`)
- Moved `smb1360_wakeup_src_init(chip)` after `smb_parse_dt()`, `smb1360_hw_init()`, and `smb1360_regulator_init()`, and added `wakeup_source_trash(&chip->smb1360_ws.source)` to `fail_hw_init:` and `smb1360_remove()`.
- Added defensive `< MODULES_VADDR` (`0xbf000000`) and `!ws->entry.next` list-integrity guards in `wakeup_sources_stats_show()`.

---

## 8. Additional Peripheral & Host Build Fixes
- **ELAN `EPL259x` (`drivers/input/misc/epl259x.c`)**: Added DT parsing for `"epl259x,irq-gpio"` and `"epl259x,prox_th_min"`/`"epl259x,prox_th_max"`, and fixed error-path UAF in `epl_sensor_probe`.
- **Bosch `BMA250` (`drivers/input/misc/bma2x2.c`)**: Added `{0x03, BMA250E_TYPE, "BMA250", "0.03828125"}` to `sensor_type_map[]` so chip ID `0x03` binds cleanly.
- **Dual Flash (`drivers/leds/leds-msm-gpio-flash.c`)**: Added 3-element `qcom,op-seq` support for independent front (`GPIO 9`) and rear (`GPIO 3`) camera flash/torch control.
- **Ubuntu 24.04 Host Compiler Fixes**: Marked `extern YYLTYPE yylloc;` in `scripts/dtc/dtc-lexer.lex.c_shipped` (`-fno-common`), updated `scripts/gcc-wrapper.py` for Python 3 `bytes.decode()` stderr forwarding (fixing `/proc/version` formatting), and guarded `static const` tables in `include/uapi/media/radio-iris.h`.
