# Sony Xperia Miro (ST23i / mesona) — Android 2.3.7 Gingerbread (AOSP & CM7)

Device tree for Sony Xperia Miro (`ST23i` / `mesona`, Qualcomm MSM7x27a, Linux 3.4).

- **`main` branch**: Pure Android 2.3.7 (AOSP) — `full_mesona` (vanilla) and `full_mesona_gapps` (with classic Android 2.3 GApps).
- **`cm-7` branch**: CyanogenMod 7 — `cm_mesona` (vanilla) and `cm_mesona_gapps`.

## Key Fixes Included
- **Kernel (`qcache`)**: Disabled cleancache (`qcache_cleancache_enabled = 0`) while keeping `fmem` for ION/Camera, fixing `/mnt/asec` binary corruption and Minecraft PE crashes.
- **Adreno 200 (`adreno_config.txt`)**: Fixed Minecraft PE missing chunks and Gallery3D HUD/background rendering (`enableMemoryPool = 1`, `useUncachedVBOs = 1`, `waitForIdleAfterDraw = 0`, `waitForIdleOnSubmit = 1`, `enableOptimizedVboUpdates = 1`).
- **Gallery3D (`GridQuad.java`)**: Fixed fullscreen photo horizontal squishing (`recomputeQuad` `modifyOverlay = true` + `bindArrays` VBO order).
- **Battery / CPU (`libpng`)**: Fixed `png_read_row` infinite loop on decompression errors (`external_libpng.patch`).
- **Recovery Exit**: Bundled `/sbin/rmt_storage` and started `service rmt_storage` in recovery `init.rc` so `reboot` and `power off` complete modem EFS sync instead of hanging; fixed `reboot recovery` to write `/cache/recovery/boot` + `RB_AUTOBOOT`.
- **Performance & Wi-Fi**: Tuned `ondemand` governor on `sys.boot_completed=1`, fixed `BootAnimation` frame timing, and defaulted `WIFI_SLEEP_POLICY_NEVER`.

## Applying Platform Patches (`patches/`)
- `external/libpng`: `patches/external_libpng.patch`
- `packages/apps/Gallery3D`: `patches/packages_apps_Gallery3D.patch`
- `bootable/recovery`: `patches/bootable_recovery.patch`
- `system/core`: `patches/system_core.patch`
