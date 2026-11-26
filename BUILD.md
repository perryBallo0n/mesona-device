# Building Android 2.3.7 (AOSP & CyanogenMod 7) for Sony Xperia Miro (ST23i / mesona)

This guide documents how to build the Linux 3.4 kernel, pure AOSP 2.3.7 (`full_mesona` / `full_mesona_gapps`), and CyanogenMod 7 (`cm_mesona` / `cm_mesona_gapps`) images and flashable OTA packages from source.

---

## 1. Host Prerequisites

- **OS**: Ubuntu 20.04/22.04 or WSL2 Ubuntu (64-bit) with at least 8–16 GB RAM and ~40 GB free disk space.
- **JDK**: Oracle/Sun Java SE 6 (`java-6-sun`, e.g. `/usr/lib/jvm/java-6-sun`) — Gingerbread requires JDK 6.
- **Build tools**: `git`, `gnupg`, `flex`, `bison`, `gperf`, `build-essential`, `zip`, `curl`, `zlib1g-dev`, `gcc-multilib`, `g++-multilib`, `libc6-dev-i386`, `lib32ncurses5-dev`, `x11proto-core-dev`, `libx11-dev`, `lib32z1-dev`, `python2.7`, `ccache`.
- **Cross-toolchain**: `arm-eabi-4.4.3` included in the ROM tree under `prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/`.

---

## 2. Source Tree Layout

Both trees (`/root/aosp` for AOSP 2.3.7, `/root/cm7` for CyanogenMod 7) use the same device/kernel/vendor repositories:

| Path | Repository / Branch |
|---|---|
| `device/sony/mesona` | `perryBallo0n/mesona-device` (`main` for AOSP, `cm-7` for CM7) |
| `kernel/sony/msm7x27a` | `perryBallo0n/mesona-kernel` (`main`) |
| `vendor/sony/mesona` | `perryBallo0n/mesona-vendor` |
| `hardware/msm7k` (AOSP) | `perryBallo0n/mesona-hardware-msm7k-v2` (`main`) |
| `frameworks/base` (AOSP) | `perryBallo0n/mesona-frameworks-base` (`main`) |

For **CyanogenMod 7**, start from a CyanogenMod 7 (`gingerbread`) source tree, clone `mesona-device` (`-b cm-7`), `mesona-kernel`, and `mesona-vendor`, then apply the patches in `device/sony/mesona/patches/`:

- `patches/external_libpng_35b24a4.patch` -> `external/libpng`
- `patches/packages_apps_Gallery3D.patch` -> `packages/apps/Gallery3D`
- `patches/frameworks_base_fixes.patch` -> `frameworks/base`
- `patches/hardware_msm7k_liblights.patch` -> `hardware/msm7k`
- `patches/packages_apps_CMParts.patch` -> `packages/apps/CMParts`
- `patches/bootable_recovery.patch` -> `bootable/recovery`
- `patches/system_core.patch` -> `system/core`

For **AOSP 2.3.7**, use the forked `mesona-frameworks-base` and `mesona-hardware-msm7k-v2` repos directly and apply `device/sony/mesona/patches/` (`external_libpng.patch`, `packages_apps_Gallery3D.patch`, `bootable_recovery.patch`, `system_core.patch`).

---

## 3. Building the Linux 3.4 Kernel

The prebuilt kernel image checked into `device/sony/mesona/kernel` is built from `kernel/sony/msm7x27a`:

```bash
export ARCH=arm
export SUBARCH=arm
export CROSS_COMPILE=/root/cm7/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-

cd kernel/sony/msm7x27a
make cm_tamsui_mes_defconfig
make -j16 zImage

cp arch/arm/boot/zImage ../../device/sony/mesona/kernel
```

Key defconfig & board settings enabled by default:
- `CONFIG_CPU_FREQ_DEFAULT_GOV_ONDEMAND=y` (with `scaling_min_freq=122880` in `init.qcom.rc`)
- Production config (no heavy runtime `DEBUG_PAGEALLOC`/`DEBUG_SPINLOCK`/`DEBUG_MUTEXES`/`SLUB_DEBUG`/`PROFILING`)
- `msm7x27a_pm_data` `RAMP_DOWN_AND_WAIT_FOR_INTERRUPT` enabled for short CPU idle gaps
- Adreno 200 KGSL (`init_level=1` 192 MHz default, dynamic scale to 245.76 MHz at >=70% busy, `nap_allowed=true`, `idle_timeout=HZ/5`)
- `bcmdhd` Wi-Fi (`-DDHD_USE_EARLYSUSPEND`, adaptive watchdog polling backoff up to `500 ms` in suspend)
- `LM3561` torch LED (`/sys/class/leds/flashlight/brightness`)

---

## 4. Environment Setup for Android Builds

```bash
export USE_CCACHE=1
export JAVA_HOME=/usr/lib/jvm/java-6-sun
export PATH=/usr/lib/jvm/java-6-sun/bin:/usr/bin:/bin:/usr/local/bin:$PATH
```

> Note: When switching `lunch` between clean (`*_mesona-eng`) and GApps (`*_mesona_gapps-eng`) targets, Android's build system triggers `installclean` automatically so packages do not mix across variants.

---

## 5. Building CyanogenMod 7 (`/root/cm7`)

```bash
cd /root/cm7
source build/envsetup.sh

# Clean CM7 (no GApps)
lunch cm_mesona-eng
make -j16 bootimage recoveryimage systemimage otapackage

# CM7 + Gingerbread GApps & Market
lunch cm_mesona_gapps-eng
make -j16 bootimage recoveryimage systemimage otapackage
```

Outputs in `out/target/product/mesona/`:
- `boot.img` (Sony S1Boot ELF with dualboot ramdisk, max 20 MB)
- `system.img` (ext4 image, max 650 MB)
- `recovery.img` (CWM Recovery image, max 8 MB)
- `cm_mesona-ota-eng.root.zip` / `cm_mesona_gapps-ota-eng.root.zip` (flashable ZIP for CWM)

---

## 6. Building Pure AOSP 2.3.7 (`/root/aosp`)

```bash
cd /root/aosp
source build/envsetup.sh

# Clean AOSP (no GApps)
lunch full_mesona-eng
make -j16 bootimage recoveryimage systemimage otapackage

# AOSP + Gingerbread GApps & Market
lunch full_mesona_gapps-eng
make -j16 bootimage recoveryimage systemimage otapackage
```

Outputs in `out/target/product/mesona/`:
- `boot.img`, `system.img`, `recovery.img`
- `full_mesona-ota-eng.root.zip` / `full_mesona_gapps-ota-eng.root.zip`

---

## 7. Flashing to Sony Xperia Miro (`ST23i`)

**Via Fastboot (Power off -> hold Vol+ and plug USB, blue LED):**
```bash
fastboot flash boot boot.img
fastboot flash system system.img
fastboot erase cache
fastboot erase userdata   # required when switching between AOSP and CM7
fastboot reboot
```

**Via CWM Recovery (hold Vol- during boot / `adb reboot recovery`):**
1. Wipe data/factory reset (if switching ROMs) + Wipe cache partition.
2. `install zip from sdcard` -> select `*-ota-eng.root.zip`.
3. Power button confirms selection; `reboot system now` exits cleanly via bundled `rmt_storage`.
