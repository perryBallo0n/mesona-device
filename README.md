# Sony Xperia Miro (ST23i / mesona) — CyanogenMod 7

Device tree for CyanogenMod 7 on Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` and `cm_mesona_gapps`.
- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` and `full_mesona_gapps`.
- See [BUILD.md](BUILD.md) for complete kernel and ROM build instructions.

## Key Fixes Included
- **Thermal & Battery (`kernel` + `init.qcom.rc`)**:
  - Disabled heavy runtime kernel debugging/poisoning (`DEBUG_PAGEALLOC`, `DEBUG_SPINLOCK`, `DEBUG_MUTEXES`, `SLUB_DEBUG`, `PROFILING`, `TAMSUI_DUMP_WAKELOCK`, etc.) that heated up CPU/RAM on every allocation.
  - Enabled MSM `RAMP_DOWN_AND_WAIT_FOR_INTERRUPT` idle mode (`board-tamsui-mes.c`) and tuned `ondemand` governor (`122.88 MHz` min, `up_threshold=90`, `sampling_rate=50000`, `io_is_busy=0`).
  - Enabled Adreno 200 KGSL clock gating (`nap_allowed=true`, `idle_timeout=200ms`) and `192 MHz` default GPU clock with dynamic in-kernel scaling up to `245.76 MHz` under >=70% load (`kgsl_pwrscale_idlestats.c`).
  - Added `bcmdhd` `-DDHD_USE_EARLYSUSPEND` (`pktfilter`/`roam_off`/`bcn_li_dtim`) and adaptive watchdog backoff (`10 ms` active -> `100 ms` idle -> `500 ms` in suspend).
- **UI & Animation FPS (`DisplayHardware` / `CMParts` / `system.prop`)**: Removed redundant post-swap `glFinish()`, disabled default software dithering (`persist.sys.use_dithering=0`, `CMParts`), and tuned pointer/event responsiveness.
- **Flashlight / Torch (`flash.c` + `init.qcom.rc`)**: Registered LM3561 Torch `led_classdev` (`/sys/class/leds/flashlight/brightness`, `0666` permissions for CM7 `Torch` / Camera).
- **Camera Portrait Orientation (`libcamera_shim.cpp`)**: Honored `CameraParameters::KEY_ROTATION` (`0`/`90`/`180`/`270`) during NV21->JPEG encoding so portrait photos are saved upright.
- **Bottom LED Bar (`liblights` + `KeyguardViewMediator`)**: Bottom bar stays off while charging; smoothly fades in on lockscreen showing battery level (bright green -> dimmer green -> yellow -> orange -> red), smoothly fades out on unlock/screen-off, and lights for notifications and music playback; silenced `fih_led` debug `printk` spam.
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: `waitForIdleAfterDraw = 0`, `waitForIdleOnSubmit = 1`, `enableOptimizedVboUpdates = 1`, `useUncachedVBOs = 1`, `enableMemoryPool = 1`.
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing (`recomputeQuad` `modifyOverlay = true` + `bindArrays` VBO upload order).
- **CWM Recovery Exit**: Enabled `BOARD_HAS_NO_SELECT_BUTTON := true` and bundled `/sbin/rmt_storage` via `recovery/init.rc` so reboot/power off completes modem EFS sync cleanly without hanging.

## Platform Patches (`patches/`)
- `external/libpng`: `patches/external_libpng_35b24a4.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `frameworks/base`: `patches/frameworks_base_fixes.patch`
- `hardware/msm7k`: `patches/hardware_msm7k_liblights.patch`
- `packages/apps/CMParts`: `patches/packages_apps_CMParts.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
