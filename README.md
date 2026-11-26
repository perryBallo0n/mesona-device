# Sony Xperia Miro (ST23i / mesona) — CyanogenMod 7

Device tree for CyanogenMod 7 on Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` and `cm_mesona_gapps`.
- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` and `full_mesona_gapps`.
- Date-based build ID (`YYYYMMDD`) set via `BUILD_DISPLAY_ID` / `BUILD_NUMBER`.
- See [BUILD.md](BUILD.md) for complete kernel and ROM build instructions.

## Key Fixes Included
- **Thermal, Battery & Memory (`kernel` + `init.qcom.rc` + `init_zram.sh`)**:
  - Disabled heavy runtime kernel debugging/poisoning (`DEBUG_PAGEALLOC`, `DEBUG_SPINLOCK`, `DEBUG_MUTEXES`, `SLUB_DEBUG`, `PROFILING`, `TAMSUI_DUMP_WAKELOCK`, etc.).
  - Enabled MSM `RAMP_DOWN_AND_WAIT_FOR_INTERRUPT` idle mode (`board-tamsui-mes.c`), added `smartassV2` CPU governor (`cpufreq_smartass2.c`), and tuned `interactive` governor on `sys.boot_completed=1` (`scaling_min_freq=122880`, `hispeed_freq=600000`, `go_hispeed_load=90`, `timer_rate=40000`, `min_sample_time=40000`, `above_hispeed_delay=40000`).
  - Backported ARM `zsmalloc` + `zram` and enabled 128 MB `/dev/block/zram0` swap (`init_zram.sh`) with VM tuning (`swappiness=60`, `page-cluster=0`, `vfs_cache_pressure=80`).
  - Replaced permanent AC charger wakelock with `wake_lock_timeout(..., 5 * HZ)` in `fih_msm_battery.c` so deep sleep works while charging.
  - Enabled Adreno 200 KGSL power nap/idle scaling (`192 -> 245.76 MHz`) and `bcmdhd` `-DDHD_USE_EARLYSUSPEND` (`10/100/500 ms` adaptive watchdog backoff).
- **UI & Animation FPS (`DisplayHardware` / `CMParts` / `system.prop`)**: Removed redundant post-swap `glFinish()`, disabled default software dithering (`persist.sys.use_dithering=0`, `CMParts`), and tuned pointer/event responsiveness.
- **Flashlight / Torch (`flash.c` + `init.qcom.rc`)**: Registered LM3561 Torch `led_classdev` (`/sys/class/leds/flashlight/brightness`, `0666` permissions for CM7 `Torch` / Camera).
- **Camera (`libcamera_shim.cpp` + `VideoCamera.java`)**:
  - Added 4-buffer `1280x960` high-resolution snapshot capture (`capture_highres_snapshot`) with automatic preview stream resume, `KEY_JPEG_QUALITY` support (default `92`), NV21 downscaling for smaller picture sizes, Touch-AF parameter negotiation (`touch-af-aec`, `touch-index-af`, `focus-areas`), upright `KEY_ROTATION` encoding, and negative-modulo video orientation fix.
- **Bottom LED Bar (`CMParts` + `KeyguardViewMediator` + `liblights`)**:
  - Added CMParts **Bottom LED bar** (`Нижняя LED-панель`) settings screen controlling master switch, lockscreen glow, album-art music color & pulsing, notifications, charging indicator (off by default), and brightness (`50/75/100/130%`).
  - Music playback extracts dominant vibrant color from MediaStore album art on a background `HandlerThread` (with palette fallback) and drives hardware PMIC LPG breathing (`45 ms` interval).
  - Unread notifications trigger Nokia 3710 Fold-style warm yellow-orange PMIC breathing glow (`52 ms` interval) until screen wakes; lockscreen smoothly fades in/out battery color gradient.
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: `waitForIdleAfterDraw = 0`, `waitForIdleOnSubmit = 1`, `enableOptimizedVboUpdates = 1`, `useUncachedVBOs = 1`, `enableMemoryPool = 1`.
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing (`recomputeQuad` `modifyOverlay = true` + `bindArrays` VBO upload order).
- **CWM Recovery Exit**: Enabled `BOARD_HAS_NO_SELECT_BUTTON := true` and bundled `/sbin/rmt_storage` via `recovery/init.rc` so reboot/power off completes modem EFS sync cleanly without hanging.

## Platform Patches (`patches/`)
- `external/libpng`: `patches/external_libpng_35b24a4.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `packages/apps/Camera`: `patches/packages_apps_Camera.patch`
- `frameworks/base`: `patches/frameworks_base_fixes.patch`
- `hardware/msm7k`: `patches/hardware_msm7k_liblights.patch`
- `packages/apps/CMParts`: `patches/packages_apps_CMParts.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
