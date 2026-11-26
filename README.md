# Sony Xperia Miro (ST23i / mesona) — Android 2.3.7 Gingerbread (AOSP & CM7)

Device tree for Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` (vanilla) and `full_mesona_gapps` (with classic Android 2.3 GApps).
- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` (vanilla) and `cm_mesona_gapps`.
- See [BUILD.md](BUILD.md) for complete kernel and ROM build instructions.

## Key Fixes Included
- **Thermal & Battery (`kernel` + `init.qcom.rc`)**: Stripped heavy kernel runtime debug/poisoning (`DEBUG_PAGEALLOC`, `DEBUG_SPINLOCK`, `DEBUG_MUTEXES`, `SLUB_DEBUG`, `PROFILING`), enabled MSM idle ramp-down WFI + tuned `ondemand` governor (`122.88 MHz` min, `up_threshold=90`), enabled Adreno 200 KGSL nap/dynamic scaling (`192 -> 245.76 MHz`), and added `bcmdhd` EarlySuspend + adaptive `10/100/500 ms` watchdog backoff.
- **UI Smoothness (`DisplayHardware` + `system.prop`)**: Removed redundant post-swap `glFinish()` stalling SurfaceFlinger and tuned event/pointer properties.
- **Flashlight & Bottom LED (`flash.c`, `init.qcom.rc`, `liblights`)**: Registered LM3561 `/sys/class/leds/flashlight/brightness` (`0666`) and added bottom chin RGB notification accent (stays off while charging).
- **Camera Portrait Orientation (`libcamera_shim.cpp`)**: Honored `CameraParameters::KEY_ROTATION` (`0`/`90`/`180`/`270`) during NV21->JPEG encoding.
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` binary corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: Fixed Minecraft PE missing chunks and Gallery3D HUD/background rendering.
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing.
- **Battery / CPU (`libpng`)**: Fixed `png_read_row` infinite loop on decompression errors (`external_libpng.patch`).
- **Recovery Exit**: Bundled `/sbin/rmt_storage` and started `service rmt_storage` in recovery `init.rc` so `reboot` and `power off` complete modem EFS sync instead of hanging.

## Applying Platform Patches (`patches/`)
- `external/libpng`: `patches/external_libpng.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
