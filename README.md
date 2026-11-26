# Sony Xperia Miro (ST23i / mesona) — Android 2.3.7 Gingerbread (AOSP & CM7)

Device tree for Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` (vanilla) and `full_mesona_gapps` (with classic Android 2.3 GApps).
- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` (vanilla) and `cm_mesona_gapps`.
- Date-based build ID (`YYYYMMDD`) set via `PRODUCT_BUILD_PROP_OVERRIDES` (`BUILD_DISPLAY_ID` / `BUILD_NUMBER`).
- See [BUILD.md](BUILD.md) for complete kernel and ROM build instructions.

## Key Fixes Included
- **Thermal, Battery & Memory (`kernel` + `init.qcom.rc` + `init_zram.sh`)**: Stripped heavy kernel runtime debug/poisoning (`DEBUG_PAGEALLOC`, `DEBUG_SPINLOCK`, `DEBUG_MUTEXES`, `SLUB_DEBUG`, `PROFILING`), enabled MSM idle ramp-down WFI, added `smartassV2` CPU governor (`cpufreq_smartass2.c`), tuned `interactive` governor + VM sysctls + 128 MB `/dev/block/zram0` swap (ARM `zsmalloc` port) on `sys.boot_completed=1`, switched `fih_msm_battery.c` AC wakelock to `wake_lock_timeout(5 * HZ)`, enabled Adreno 200 KGSL nap/dynamic scaling (`192 -> 245.76 MHz`), and added `bcmdhd` EarlySuspend + adaptive `10/100/500 ms` watchdog backoff.
- **UI Smoothness (`DisplayHardware` + `system.prop`)**: Removed redundant post-swap `glFinish()` stalling SurfaceFlinger and tuned event/pointer properties.
- **Flashlight & Bottom LED (`flash.c`, `init.qcom.rc`, `liblights`)**: Registered LM3561 `/sys/class/leds/flashlight/brightness` (`0666`), added Nokia 3710 Fold-style warm yellow-orange breathing glow (`52 ms` PMIC LPG fade) for notifications while honoring `persist.sys.bottom_led_*` properties (charging stays off by default).
- **Camera (`libcamera_shim.cpp`)**: Honored `CameraParameters::KEY_ROTATION` (`0`/`90`/`180`/`270`), added 4-buffer `1280x960` high-resolution snapshot capture (`capture_highres_snapshot`) with automatic preview resume, `KEY_JPEG_QUALITY` (default `92`), NV21 downscaling, and Touch-AF parameter support.
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` binary corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: Fixed Minecraft PE missing chunks and Gallery3D HUD/background rendering.
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing.
- **Battery / CPU (`libpng`)**: Fixed `png_read_row` infinite loop on decompression errors (`external_libpng.patch`).
- **Recovery Exit**: Bundled `/sbin/rmt_storage` and started `service rmt_storage` in recovery `init.rc` so `reboot` and `power off` complete modem EFS sync instead of hanging.

## Applying Platform Patches (`patches/`)
- `build`: `patches/build_display_id.patch`
- `hardware/msm7k`: `patches/hardware_msm7k_liblights.patch`
- `external/libpng`: `patches/external_libpng.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
