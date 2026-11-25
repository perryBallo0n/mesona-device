# Sony Xperia Miro (ST23i / mesona) — CyanogenMod 7

Device tree for CyanogenMod 7 on Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` and `cm_mesona_gapps`.
- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` and `full_mesona_gapps`.

## Key Fixes Included
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: `waitForIdleAfterDraw = 0`, `waitForIdleOnSubmit = 1`, `enableOptimizedVboUpdates = 1`, `useUncachedVBOs = 1`, `enableMemoryPool = 1` (fixes MCPE chunk dropout and Gallery3D HUD/blur).
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing (`recomputeQuad` `modifyOverlay = true` + `bindArrays` VBO upload order).
- **CWM Recovery Exit**: Enabled `BOARD_HAS_NO_SELECT_BUTTON := true` (Power button selects menu items + Go Back entries) and bundled `/sbin/rmt_storage` via `recovery/init.rc` so reboot/power off completes modem EFS sync cleanly without hanging.
- **Battery / BootAnimation / Wi-Fi**: `libpng` infinite loop fix (`35b24a4`), smooth `BootAnimation` pacing, tuned `ondemand` governor, and default `WIFI_SLEEP_POLICY_NEVER`.

## Platform Patches (`patches/`)
- `external/libpng`: `patches/external_libpng_35b24a4.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `frameworks/base`: `patches/frameworks_base_fixes.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
