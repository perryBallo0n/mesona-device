#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/input.h>

#include "common.h"
#include "recovery_ui.h"
#include "install.h"
#include "roots.h"
#include "mtdutils/mounts.h"

#define ACTION_REBOOT        0
#define ACTION_USB_MS        1
#define ACTION_BACKUP        2
#define ACTION_FLASH_ZIP     3
#define ACTION_WIPE          4
#define ACTION_MOUNTS        5
#define ACTION_OTHER         6
#define ACTION_POWER_OFF     7

#define BACKUP_BASE "/sdcard/clockworkmod/backup"
#define USB_LUN_FILE "/sys/class/android_usb/android0/f_mass_storage/lun/file"

char* MENU_HEADERS[] = {
    "[Vol+/-: move | Power: ok]",
    "",
    NULL
};

char* MENU_ITEMS[] = {
    "Reboot system now",
    "USB-MS toggle",
    "Backup/Restore",
    "Flash zip menu",
    "Wipe",
    "Mounts & Storage",
    "Other",
    "Power off",
    NULL
};

static int run_sh(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/sbin/busybox", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    if (pid < 0) return -1;
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

static int write_sysfs(const char* path, const char* val) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    ssize_t n = write(fd, val, strlen(val));
    close(fd);
    return (n >= 0) ? 0 : -1;
}

static int is_mounted_pt(const char* mp) {
    if (scan_mounted_volumes() < 0) return 0;
    return find_mounted_volume_by_mount_point(mp) != NULL;
}

static void toggle_usb_ms(void) {
    ensure_path_unmounted("/sdcard");
    ensure_path_unmounted("/external_sd");

    if (write_sysfs(USB_LUN_FILE, "/dev/block/mmcblk0p16") != 0) {
        ui_print("Failed to open USB LUN.\n");
        return;
    }

    const char* hdr[] = { "USB Mass Storage active", "Internal SD shared to PC", "", NULL };
    char** headers = rec_prepend_title(hdr);
    char* items[] = { "Disable USB-MS", NULL };
    rec_get_menu_selection(headers, items, 1, 0);
    free(headers);

    write_sysfs(USB_LUN_FILE, "");
    ui_print("USB Mass Storage disabled.\n");
}

static void do_nandroid_backup(void) {
    if (ensure_path_mounted("/sdcard") != 0) {
        ui_print("Failed to mount /sdcard!\n");
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char dir[256];
    if (t) {
        snprintf(dir, sizeof(dir), "%s/%04d-%02d-%02d.%02d.%02d.%02d",
                 BACKUP_BASE, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        snprintf(dir, sizeof(dir), "%s/backup-%ld", BACKUP_BASE, (long)now);
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/sbin/busybox mkdir -p \"%s\"", dir);
    run_sh(cmd);

    ui_print("\n-- Backing up to:\n%s\n", dir);
    ui_print("Backing up boot...\n");
    snprintf(cmd, sizeof(cmd), "/sbin/busybox dd if=/dev/block/mmcblk0p3 of=\"%s/boot.img\" bs=4096", dir);
    if (run_sh(cmd) != 0) {
        ui_print("Error backing up boot!\n");
        return;
    }

    ui_print("Backing up system...\n");
    if (ensure_path_mounted("/system") == 0) {
        snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -cpf \"%s/system.tar\" -C /system .", dir);
        if (run_sh(cmd) != 0) {
            ui_print("Error backing up system!\n");
            return;
        }
    }

    ui_print("Backing up data...\n");
    if (ensure_path_mounted("/data") == 0) {
        snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -cpf \"%s/data.tar\" -C /data .", dir);
        if (run_sh(cmd) != 0) {
            ui_print("Error backing up data!\n");
            return;
        }
    }

    ui_print("Backing up cache...\n");
    if (ensure_path_mounted("/cache") == 0) {
        snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -cpf \"%s/cache.tar\" -C /cache .", dir);
        run_sh(cmd);
    }

    sync();
    ui_print("Backup complete!\n");
}

static int compare_str_desc(const void* a, const void* b) {
    return strcmp(*(const char**)b, *(const char**)a);
}

static int choose_backup_dir(const char* title, char* out_dir, size_t out_sz) {
    if (ensure_path_mounted("/sdcard") != 0) {
        ui_print("Failed to mount /sdcard!\n");
        return -1;
    }
    DIR* d = opendir(BACKUP_BASE);
    if (!d) {
        ui_print("No backups found.\n");
        return -1;
    }
    int cap = 16, cnt = 1;
    char** items = malloc(cap * sizeof(char*));
    items[0] = strdup("Go back");
    struct dirent* de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (de->d_type == DT_DIR || de->d_type == DT_UNKNOWN) {
            if (cnt + 2 >= cap) {
                cap *= 2;
                items = realloc(items, cap * sizeof(char*));
            }
            items[cnt++] = strdup(de->d_name);
        }
    }
    closedir(d);

    if (cnt <= 1) {
        free(items[0]);
        free(items);
        ui_print("No backups found.\n");
        return -1;
    }

    qsort(items + 1, cnt - 1, sizeof(char*), compare_str_desc);
    items[cnt] = NULL;

    const char* hdr[] = { title, "", NULL };
    char** headers = rec_prepend_title(hdr);
    int ch = rec_get_menu_selection(headers, items, 1, 0);
    free(headers);

    int ret = -1;
    if (ch > 0 && ch < cnt) {
        snprintf(out_dir, out_sz, "%s/%s", BACKUP_BASE, items[ch]);
        ret = 0;
    }
    int i;
    for (i = 0; i < cnt; i++) free(items[i]);
    free(items);
    return ret;
}

static void do_nandroid_restore(void) {
    char dir[256];
    if (choose_backup_dir("Choose backup to restore:", dir, sizeof(dir)) != 0) return;
    if (!rec_confirm_selection("Confirm restore?", " Yes - Restore backup")) return;

    char path[300], cmd[512];
    struct stat st;

    snprintf(path, sizeof(path), "%s/boot.img", dir);
    if (stat(path, &st) == 0) {
        ui_print("Restoring boot...\n");
        snprintf(cmd, sizeof(cmd), "/sbin/busybox dd if=\"%s\" of=/dev/block/mmcblk0p3 bs=4096", path);
        run_sh(cmd);
    }

    snprintf(path, sizeof(path), "%s/system.tar", dir);
    if (stat(path, &st) == 0) {
        ui_print("Restoring system...\n");
        format_volume("/system");
        if (ensure_path_mounted("/system") == 0) {
            snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -xpf \"%s\" -C /system", path);
            run_sh(cmd);
        }
    }

    snprintf(path, sizeof(path), "%s/data.tar", dir);
    if (stat(path, &st) == 0) {
        ui_print("Restoring data...\n");
        format_volume("/data");
        if (ensure_path_mounted("/data") == 0) {
            snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -xpf \"%s\" -C /data", path);
            run_sh(cmd);
        }
    }

    snprintf(path, sizeof(path), "%s/cache.tar", dir);
    if (stat(path, &st) == 0) {
        ui_print("Restoring cache...\n");
        format_volume("/cache");
        if (ensure_path_mounted("/cache") == 0) {
            snprintf(cmd, sizeof(cmd), "/sbin/busybox tar -xpf \"%s\" -C /cache", path);
            run_sh(cmd);
        }
    }

    sync();
    ui_print("Restore complete!\n");
}

static void do_delete_backup(void) {
    char dir[256];
    if (choose_backup_dir("Choose backup to delete:", dir, sizeof(dir)) != 0) return;
    if (!rec_confirm_selection("Delete this backup?", " Yes - Delete backup")) return;
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/sbin/busybox rm -rf \"%s\"", dir);
    run_sh(cmd);
    sync();
    ui_print("Backup deleted.\n");
}

static void show_backup_menu(void) {
    const char* hdr[] = { "Backup / Restore", "", NULL };
    char** headers = rec_prepend_title(hdr);
    char* items[] = {
        "Backup",
        "Restore",
        "Delete backup",
        "Go back",
        NULL
    };
    int sel = 0;
    for (;;) {
        sel = rec_get_menu_selection(headers, items, 1, sel);
        if (sel == 0) do_nandroid_backup();
        else if (sel == 1) do_nandroid_restore();
        else if (sel == 2) do_delete_backup();
        else break;
    }
    free(headers);
}

static void show_flash_menu(void) {
    const char* hdr[] = { "Flash zip menu", "", NULL };
    char** headers = rec_prepend_title(hdr);
    int sel = 0;
    for (;;) {
        char sig_label[40];
        snprintf(sig_label, sizeof(sig_label), "Sig verify: %s",
                 signature_check_enabled ? "Enabled" : "Disabled");
        char* items[] = {
            "Choose zip from /sdcard",
            "Choose zip from /external_sd",
            sig_label,
            "Go back",
            NULL
        };
        sel = rec_get_menu_selection(headers, items, 1, sel);
        if (sel == 0) {
            int st = rec_sdcard_directory("/sdcard");
            if (st >= 0) {
                ui_print(st == INSTALL_SUCCESS ? "\nInstall complete.\n" : "\nInstall aborted.\n");
            }
        } else if (sel == 1) {
            if (ensure_path_mounted("/external_sd") == 0) {
                int st = rec_sdcard_directory("/external_sd");
                if (st >= 0) {
                    ui_print(st == INSTALL_SUCCESS ? "\nInstall complete.\n" : "\nInstall aborted.\n");
                }
            } else {
                ui_print("No external SD mounted.\n");
            }
        } else if (sel == 2) {
            signature_check_enabled = !signature_check_enabled;
        } else {
            break;
        }
    }
    free(headers);
}

static void show_wipe_menu(void) {
    const char* hdr[] = { "Wipe menu", "", NULL };
    char** headers = rec_prepend_title(hdr);
    char* items[] = {
        "Wipe data/factory reset",
        "Wipe cache partition",
        "Wipe Dalvik Cache",
        "Go back",
        NULL
    };
    int sel = 0;
    for (;;) {
        sel = rec_get_menu_selection(headers, items, 1, sel);
        if (sel == 0) {
            if (rec_confirm_selection("Wipe all user data?", " Yes - delete all user data")) {
                ui_print("\n-- Wiping data...\n");
                erase_volume("/data");
                erase_volume("/cache");
                ui_print("Data wipe complete.\n");
            }
        } else if (sel == 1) {
            if (rec_confirm_selection("Wipe cache?", " Yes - Wipe cache")) {
                ui_print("\n-- Wiping cache...\n");
                erase_volume("/cache");
                ui_print("Cache wipe complete.\n");
            }
        } else if (sel == 2) {
            ui_print("\n-- Wiping Dalvik Cache...\n");
            if (ensure_path_mounted("/data") == 0) {
                run_sh("/sbin/busybox rm -rf /data/dalvik-cache/*");
            }
            if (ensure_path_mounted("/cache") == 0) {
                run_sh("/sbin/busybox rm -rf /cache/dalvik-cache/*");
            }
            ui_print("Dalvik Cache wiped.\n");
        } else {
            break;
        }
    }
    free(headers);
}

static void toggle_mount(const char* mp) {
    if (is_mounted_pt(mp)) {
        if (ensure_path_unmounted(mp) == 0) ui_print("Unmounted %s\n", mp);
        else ui_print("Failed to unmount %s\n", mp);
    } else {
        if (ensure_path_mounted(mp) == 0) ui_print("Mounted %s\n", mp);
        else ui_print("Failed to mount %s\n", mp);
    }
}

static void show_mounts_menu(void) {
    const char* hdr[] = { "Mounts & Storage", "", NULL };
    char** headers = rec_prepend_title(hdr);
    int sel = 0;
    for (;;) {
        char m_sys[32], m_dat[32], m_cac[32], m_sdc[32], m_ext[32];
        snprintf(m_sys, sizeof(m_sys), "%s /system", is_mounted_pt("/system") ? "unmount" : "mount");
        snprintf(m_dat, sizeof(m_dat), "%s /data", is_mounted_pt("/data") ? "unmount" : "mount");
        snprintf(m_cac, sizeof(m_cac), "%s /cache", is_mounted_pt("/cache") ? "unmount" : "mount");
        snprintf(m_sdc, sizeof(m_sdc), "%s /sdcard", is_mounted_pt("/sdcard") ? "unmount" : "mount");
        snprintf(m_ext, sizeof(m_ext), "%s /external_sd", is_mounted_pt("/external_sd") ? "unmount" : "mount");
        char* items[] = {
            m_sys, m_dat, m_cac, m_sdc, m_ext,
            "format /system",
            "format /data",
            "format /cache",
            "Go back",
            NULL
        };
        sel = rec_get_menu_selection(headers, items, 1, sel);
        if (sel == 0) toggle_mount("/system");
        else if (sel == 1) toggle_mount("/data");
        else if (sel == 2) toggle_mount("/cache");
        else if (sel == 3) toggle_mount("/sdcard");
        else if (sel == 4) toggle_mount("/external_sd");
        else if (sel == 5) {
            if (rec_confirm_selection("Format /system?", " Yes - Format /system")) {
                ui_print("Formatting /system...\n");
                format_volume("/system");
                ui_print("Done.\n");
            }
        } else if (sel == 6) {
            if (rec_confirm_selection("Format /data?", " Yes - Format /data")) {
                ui_print("Formatting /data...\n");
                format_volume("/data");
                ui_print("Done.\n");
            }
        } else if (sel == 7) {
            if (rec_confirm_selection("Format /cache?", " Yes - Format /cache")) {
                ui_print("Formatting /cache...\n");
                format_volume("/cache");
                ui_print("Done.\n");
            }
        } else {
            break;
        }
    }
    free(headers);
}

static void show_other_menu(void) {
    const char* hdr[] = { "Other / Advanced", "", NULL };
    char** headers = rec_prepend_title(hdr);
    char* items[] = {
        "Reboot to bootloader",
        "Show recovery log",
        "Go back",
        NULL
    };
    int sel = 0;
    for (;;) {
        sel = rec_get_menu_selection(headers, items, 1, sel);
        if (sel == 0) {
            sync();
            __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "bootloader");
        } else if (sel == 1) {
            ui_print("\n--- Last log lines ---\n");
            run_sh("/sbin/busybox tail -n 15 /tmp/recovery.log");
        } else {
            break;
        }
    }
    free(headers);
}

int device_recovery_start() {
    ui_show_text(1);
    write_sysfs("/sys/module/msm_fb/parameters/align_buffer", "1");
    return 0;
}

int device_toggle_display(volatile char* key_pressed, int key_code) {
    (void)key_pressed;
    return key_code == KEY_HOME;
}

int device_reboot_now(volatile char* key_pressed, int key_code) {
    (void)key_pressed;
    (void)key_code;
    return 0;
}

int device_handle_key(int key_code, int visible) {
    if (visible) {
        switch (key_code) {
            case KEY_DOWN:
            case KEY_VOLUMEDOWN:
                return HIGHLIGHT_DOWN;

            case KEY_UP:
            case KEY_VOLUMEUP:
                return HIGHLIGHT_UP;

            case KEY_POWER:
            case KEY_ENTER:
                return SELECT_ITEM;
        }
    }
    return NO_ACTION;
}

int device_perform_action(int which) {
    switch (which) {
        case ACTION_REBOOT:
            return ITEM_REBOOT;
        case ACTION_USB_MS:
            toggle_usb_ms();
            return -1;
        case ACTION_BACKUP:
            show_backup_menu();
            return -1;
        case ACTION_FLASH_ZIP:
            show_flash_menu();
            return -1;
        case ACTION_WIPE:
            show_wipe_menu();
            return -1;
        case ACTION_MOUNTS:
            show_mounts_menu();
            return -1;
        case ACTION_OTHER:
            show_other_menu();
            return -1;
        case ACTION_POWER_OFF:
            ui_print("Powering off...\n");
            sync();
            __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
            return -1;
        default:
            return -1;
    }
}

int device_wipe_data() {
    return 0;
}
