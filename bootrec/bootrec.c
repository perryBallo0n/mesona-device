#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/input.h>
#include <zlib.h>

#ifndef KEY_VOLUMEDOWN
#define KEY_VOLUMEDOWN 114
#endif

static void write_led(const char *cmd) {
    int fd = open("/sys/class/led/fih_led/control", O_WRONLY);
    if (fd >= 0) {
        write(fd, cmd, strlen(cmd));
        close(fd);
    }
}

static void mkdir_p(const char *path, mode_t mode) {
    char tmp[512];
    char *p = NULL;
    size_t len;
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return;
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode ? mode : 0755);
            *p = '/';
        }
    }
    mkdir(tmp, mode ? mode : 0755);
}

static void create_dev_node_from_sys(const char *sys_dev_file, const char *dev_path, mode_t type_mode) {
    FILE *fp = fopen(sys_dev_file, "r");
    int maj = 0, min = 0;
    if (!fp) return;
    if (fscanf(fp, "%d:%d", &maj, &min) == 2) {
        unlink(dev_path);
        mknod(dev_path, type_mode | 0600, makedev(maj, min));
    }
    fclose(fp);
}

static void setup_minimal_dev(void) {
    DIR *d;
    struct dirent *de;
    mkdir("/dev/input", 0755);
    mkdir("/dev/block", 0755);
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));

    d = opendir("/sys/class/input");
    if (d) {
        while ((de = readdir(d)) != NULL) {
            if (strncmp(de->d_name, "event", 5) == 0) {
                char sysf[256], devp[256];
                snprintf(sysf, sizeof(sysf), "/sys/class/input/%s/dev", de->d_name);
                snprintf(devp, sizeof(devp), "/dev/input/%s", de->d_name);
                create_dev_node_from_sys(sysf, devp, S_IFCHR);
            }
        }
        closedir(d);
    }
    {
        int tries;
        for (tries = 0; tries < 20; tries++) {
            if (access("/sys/block/mmcblk0/mmcblk0p11/dev", R_OK) == 0) break;
            usleep(50000);
        }
    }
    create_dev_node_from_sys("/sys/block/mmcblk0/mmcblk0p11/dev", "/dev/block/mmcblk0p11", S_IFBLK);
}

static int check_cache_recovery_trigger(void) {
    struct stat st;
    int trig = 0;
    mkdir("/cache", 0770);
    if (mount("/dev/block/mmcblk0p11", "/cache", "ext4", MS_NOATIME, NULL) == 0) {
        if (stat("/cache/recovery/boot", &st) == 0) {
            unlink("/cache/recovery/boot");
            sync();
            trig = 1;
        } else if (stat("/cache/recovery/command", &st) == 0) {
            trig = 1;
        }
        umount("/cache");
    }
    return trig;
}

static int64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int poll_voldown_key(int timeout_ms) {
    struct pollfd pfds[16];
    int nfds = 0;
    DIR *d = opendir("/dev/input");
    struct dirent *de;
    int i, pressed = 0;

    if (d) {
        while ((de = readdir(d)) != NULL && nfds < 16) {
            if (strncmp(de->d_name, "event", 5) == 0) {
                char p[256];
                int fd;
                snprintf(p, sizeof(p), "/dev/input/%s", de->d_name);
                fd = open(p, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    pfds[nfds].fd = fd;
                    pfds[nfds].events = POLLIN;
                    nfds++;
                }
            }
        }
        closedir(d);
    }

    if (nfds == 0) return 0;

    int64_t end = now_ms() + timeout_ms;
    while (!pressed) {
        int rem = (int)(end - now_ms());
        int r;
        if (rem <= 0) break;
        r = poll(pfds, nfds, rem > 50 ? 50 : rem);
        if (r > 0) {
            for (i = 0; i < nfds; i++) {
                if (pfds[i].revents & POLLIN) {
                    struct input_event ev;
                    while (read(pfds[i].fd, &ev, sizeof(ev)) == sizeof(ev)) {
                        if (ev.type == EV_KEY && ev.value > 0 && ev.code == KEY_VOLUMEDOWN) {
                            pressed = 1;
                            break;
                        }
                    }
                }
                if (pressed) break;
            }
        }
    }

    for (i = 0; i < nfds; i++) close(pfds[i].fd);
    return pressed;
}

static unsigned long parse_hex8(const char *s) {
    char buf[9];
    memcpy(buf, s, 8);
    buf[8] = '\0';
    return strtoul(buf, NULL, 16);
}

static int gzread_exact(gzFile gz, void *buf, size_t len) {
    size_t got = 0;
    while (got < len) {
        int r = gzread(gz, (char *)buf + got, (unsigned)(len - got));
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}

static void gzskip_exact(gzFile gz, size_t len) {
    char tmp[512];
    while (len > 0) {
        size_t chunk = len > sizeof(tmp) ? sizeof(tmp) : len;
        if (gzread_exact(gz, tmp, chunk) < 0) break;
        len -= chunk;
    }
}

static int extract_cpio_gz(const char *archive_path) {
    gzFile gz = gzopen(archive_path, "rb");
    char hdr[110];
    if (!gz) return -1;

    while (gzread_exact(gz, hdr, 110) == 0) {
        if (memcmp(hdr, "070701", 6) != 0 && memcmp(hdr, "070702", 6) != 0) {
            break;
        }
        unsigned long mode = parse_hex8(hdr + 14);
        unsigned long filesize = parse_hex8(hdr + 54);
        unsigned long namesize = parse_hex8(hdr + 94);
        if (namesize == 0 || namesize > 4096) break;

        char *name = malloc(namesize + 1);
        if (!name) break;
        if (gzread_exact(gz, name, namesize) < 0) {
            free(name);
            break;
        }
        name[namesize] = '\0';

        size_t hdr_pad = (4 - ((110 + namesize) & 3)) & 3;
        if (hdr_pad) gzskip_exact(gz, hdr_pad);

        if (strcmp(name, "TRAILER!!!") == 0) {
            free(name);
            break;
        }

        const char *rel = name;
        while (*rel == '.' || *rel == '/') rel++;
        char dst[512];
        if (*rel) {
            snprintf(dst, sizeof(dst), "/%s", rel);
        } else {
            dst[0] = '\0';
        }

        size_t data_pad = (4 - (filesize & 3)) & 3;

        if (dst[0] == '\0') {
            gzskip_exact(gz, filesize + data_pad);
            free(name);
            continue;
        }

        if (S_ISDIR(mode)) {
            mkdir_p(dst, mode & 0777);
            chmod(dst, mode & 0777);
            gzskip_exact(gz, filesize + data_pad);
        } else if (S_ISLNK(mode)) {
            char *target = malloc(filesize + 1);
            if (target && gzread_exact(gz, target, filesize) == 0) {
                char *slash = strrchr(dst, '/');
                if (slash && slash != dst) {
                    *slash = '\0';
                    mkdir_p(dst, 0755);
                    *slash = '/';
                }
                target[filesize] = '\0';
                unlink(dst);
                symlink(target, dst);
            }
            free(target);
            if (data_pad) gzskip_exact(gz, data_pad);
        } else if (S_ISREG(mode)) {
            char *slash = strrchr(dst, '/');
            if (slash && slash != dst) {
                *slash = '\0';
                mkdir_p(dst, 0755);
                *slash = '/';
            }
            unlink(dst);
            int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
            size_t rem = filesize;
            char buf[4096];
            while (rem > 0) {
                size_t chunk = rem > sizeof(buf) ? sizeof(buf) : rem;
                if (gzread_exact(gz, buf, chunk) < 0) break;
                if (fd >= 0) write(fd, buf, chunk);
                rem -= chunk;
            }
            if (fd >= 0) close(fd);
            chmod(dst, mode & 0777);
            if (data_pad) gzskip_exact(gz, data_pad);
        } else {
            gzskip_exact(gz, filesize + data_pad);
        }
        free(name);
    }

    gzclose(gz);
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;

    if (getpid() != 1 && access("/init.android", X_OK) == 0) {
        execv("/init.android", argv);
        return 1;
    }

    if (access("/init.android", X_OK) != 0) {
        /* Fallback if somehow called without init.android */
        return 1;
    }

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/dev", 0755);
    mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755");

    setup_minimal_dev();

    int enter_recovery = check_cache_recovery_trigger();
    if (!enter_recovery && access("/sbin/ramdisk-recovery.img", R_OK) == 0) {
        /* Light cyan bottom LED bar during 1.8s Volume-Down recovery window */
        write_led("4 4 0\n1 4 0\n4 5 250\n1 5 1\n4 6 400\n1 6 1\n");
        enter_recovery = poll_voldown_key(1800);
    }

    if (enter_recovery && access("/sbin/ramdisk-recovery.img", R_OK) == 0) {
        write_led("4 4 0\n1 4 0\n4 5 500\n1 5 1\n4 6 0\n1 6 0\n");
        unlink("/init");
        unlink("/init.android");
        unlink("/init.rc");
        unlink("/init.qcom.rc");
        unlink("/init.mesona.rc");
        unlink("/default.prop");
        extract_cpio_gz("/sbin/ramdisk-recovery.img");
        unlink("/sbin/ramdisk-recovery.img");
        write_led("1 4 0\n1 5 0\n1 6 0\n");
        umount("/dev");
        umount("/sys");
        umount("/proc");
        execv("/init", argv);
    }

    write_led("1 4 0\n1 5 0\n1 6 0\n");
    umount("/dev");
    umount("/sys");
    umount("/proc");
    execv("/init.android", argv);
    return 1;
}
