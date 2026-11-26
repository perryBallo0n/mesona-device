/* Qualcomm Camera Compatibility Shim for Android 2.3 Gingerbread */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <poll.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <camera/CameraHardwareInterface.h>
#include <hardware/hardware.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <utils/Timers.h>

#define LOG_TAG "libcamera_shim"

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

struct my_jpeg_destination_mgr {
    struct jpeg_destination_mgr pub;
    JOCTET *buffer;
    size_t bufsize;
    size_t *outsize;
};

static void my_init_destination(j_compress_ptr cinfo) {
    my_jpeg_destination_mgr *dest = (my_jpeg_destination_mgr *)cinfo->dest;
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = dest->bufsize;
}

static boolean my_empty_output_buffer(j_compress_ptr cinfo) {
    return FALSE;
}

static void my_term_destination(j_compress_ptr cinfo) {
    my_jpeg_destination_mgr *dest = (my_jpeg_destination_mgr *)cinfo->dest;
    *dest->outsize = dest->bufsize - dest->pub.free_in_buffer;
}

static int compress_nv21_to_jpeg(const uint8_t *nv21, int width, int height, int quality,
                                 int rotation_deg,
                                 uint8_t *out_jpeg, size_t max_out_sz, size_t *out_sz) {
    if (!nv21 || !out_jpeg || !out_sz || width <= 0 || height <= 0) return -1;

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    my_jpeg_destination_mgr dest;
    dest.pub.init_destination = my_init_destination;
    dest.pub.empty_output_buffer = my_empty_output_buffer;
    dest.pub.term_destination = my_term_destination;
    dest.buffer = (JOCTET *)out_jpeg;
    dest.bufsize = max_out_sz;
    dest.outsize = out_sz;
    *out_sz = 0;
    cinfo.dest = (struct jpeg_destination_mgr *)&dest;

    int rot = ((rotation_deg % 360) + 360) % 360;
    if (rot != 0 && rot != 90 && rot != 180 && rot != 270) rot = 0;
    int out_w = (rot == 90 || rot == 270) ? height : width;
    int out_h = (rot == 90 || rot == 270) ? width : height;

    cinfo.image_width = out_w;
    cinfo.image_height = out_h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    const uint8_t *y_plane = nv21;
    const uint8_t *vu_plane = nv21 + width * height;
    JSAMPROW row_pointer[1];
    uint8_t *rgb_row = (uint8_t *)malloc(out_w * 3);
    if (!rgb_row) {
        jpeg_destroy_compress(&cinfo);
        return -1;
    }

    while (cinfo.next_scanline < cinfo.image_height) {
        int oy = cinfo.next_scanline;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = ox, sy = oy;
            if (rot == 90) {
                sx = oy;
                sy = height - 1 - ox;
            } else if (rot == 180) {
                sx = width - 1 - ox;
                sy = height - 1 - oy;
            } else if (rot == 270) {
                sx = width - 1 - oy;
                sy = ox;
            }
            int Y = y_plane[sy * width + sx];
            const uint8_t *vu_row = vu_plane + (sy >> 1) * width;
            int V = vu_row[(sx & ~1)];
            int U = vu_row[(sx & ~1) + 1];

            int c = Y - 16;
            int d = U - 128;
            int e = V - 128;

            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;

            rgb_row[ox * 3 + 0] = (uint8_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
            rgb_row[ox * 3 + 1] = (uint8_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
            rgb_row[ox * 3 + 2] = (uint8_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
        }
        row_pointer[0] = rgb_row;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    free(rgb_row);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}

extern "C" {

/* Genlock buffer stubs */
int genlock_lock_buffer(void* hnd, int type, int timeout) { return 0; }
int genlock_unlock_buffer(void* hnd) { return 0; }

/* CameraParameters method stubs */
void _ZN7android16CameraParameters15setTouchIndexAfEii(void* self, int x, int y) {}
void _ZN7android16CameraParameters16setTouchIndexAecEii(void* self, int x, int y) {}
void _ZN7android16CameraParameters18setPreviewFpsRangeEii(void* self, int min, int max) {}
void _ZN7android16CameraParameters23setPreviewFrameRateModeEPKc(void* self, const char* m) {}
void _ZNK7android16CameraParameters18getFocusAreaCenterEPiS1_(void* self, int* x, int* y) { if (x) *x = 0; if (y) *y = 0; }
const char* _ZNK7android16CameraParameters23getPreviewFrameRateModeEv(void* self) { return "auto"; }

/* CameraParameters string constants */
const char _ZN7android16CameraParameters10DENOISE_ONE[] = "denoise-on";
const char _ZN7android16CameraParameters10HDR_ENABLEE[] = "hdr-enable";
const char _ZN7android16CameraParameters10MCE_ENABLEE[] = "mce-enable";
const char _ZN7android16CameraParameters11DENOISE_OFFE[] = "denoise-off";
const char _ZN7android16CameraParameters11HDR_DISABLEE[] = "hdr-disable";
const char _ZN7android16CameraParameters11KEY_DENOISEE[] = "denoise";
const char _ZN7android16CameraParameters11MCE_DISABLEE[] = "mce-disable";
const char _ZN7android16CameraParameters12KEY_CONTRASTE[] = "contrast";
const char _ZN7android16CameraParameters12KEY_ISO_MODEE[] = "iso";
const char _ZN7android16CameraParameters12VIDEO_HFR_2XE[] = "video-hfr-2x";
const char _ZN7android16CameraParameters12VIDEO_HFR_3XE[] = "video-hfr-3x";
const char _ZN7android16CameraParameters12VIDEO_HFR_4XE[] = "video-hfr-4x";
const char _ZN7android16CameraParameters13KEY_HISTOGRAME[] = "histogram";
const char _ZN7android16CameraParameters13KEY_LENSSHADEE[] = "lensshade";
const char _ZN7android16CameraParameters13KEY_SHARPNESSE[] = "sharpness";
const char _ZN7android16CameraParameters13SCENE_MODE_ARE[] = "scene-mode-ar";
const char _ZN7android16CameraParameters13VIDEO_HFR_OFFE[] = "video-hfr-off";
const char _ZN7android16CameraParameters14KEY_GPS_STATUSE[] = "gps-status";
const char _ZN7android16CameraParameters14KEY_SATURATIONE[] = "saturation";
const char _ZN7android16CameraParameters14KEY_VIDEO_SIZEE[] = "video-size";
const char _ZN7android16CameraParameters15KEY_CAMERA_MODEE[] = "camera-mode";
const char _ZN7android16CameraParameters15KEY_FOCUS_AREASE[] = "focus-areas";
const char _ZN7android16CameraParameters15SCENE_DETECT_ONE[] = "scene-detect-on";
const char _ZN7android16CameraParameters15SEMC_METRY_SPOTE[] = "spot";
const char _ZN7android16CameraParameters15TOUCH_AF_AEC_ONE[] = "touch-af-aec-on";
const char _ZN7android16CameraParameters16HISTOGRAM_ENABLEE[] = "histogram-enable";
const char _ZN7android16CameraParameters16KEY_MAX_CONTRASTE[] = "max-contrast";
const char _ZN7android16CameraParameters16KEY_SCENE_DETECTE[] = "scene-detect";
const char _ZN7android16CameraParameters16KEY_TOUCH_AF_AECE[] = "touch-af-aec";
const char _ZN7android16CameraParameters16LENSSHADE_ENABLEE[] = "lensshade-enable";
const char _ZN7android16CameraParameters16PIXEL_FORMAT_RAWE[] = "raw";
const char _ZN7android16CameraParameters16SCENE_DETECT_OFFE[] = "scene-detect-off";
const char _ZN7android16CameraParameters16SEMC_METRY_FRAMEE[] = "frame";
const char _ZN7android16CameraParameters16TOUCH_AF_AEC_OFFE[] = "touch-af-aec-off";
const char _ZN7android16CameraParameters17FACE_DETECTION_ONE[] = "face-detection-on";
const char _ZN7android16CameraParameters17HISTOGRAM_DISABLEE[] = "histogram-disable";
const char _ZN7android16CameraParameters17KEY_AUTO_EXPOSUREE[] = "auto-exposure";
const char _ZN7android16CameraParameters17KEY_EXIF_DATETIMEE[] = "exif-datetime";
const char _ZN7android16CameraParameters17KEY_MAX_SHARPNESSE[] = "max-sharpness";
const char _ZN7android16CameraParameters17LENSSHADE_DISABLEE[] = "lensshade-disable";
const char _ZN7android16CameraParameters17PIXEL_FORMAT_YV12E[] = "yv12";
const char _ZN7android16CameraParameters17SEMC_METRY_CENTERE[] = "center";
const char _ZN7android16CameraParameters18FACE_DETECTION_OFFE[] = "face-detection-off";
const char _ZN7android16CameraParameters18KEY_FACE_DETECTIONE[] = "face-detection";
const char _ZN7android16CameraParameters18KEY_MAX_SATURATIONE[] = "max-saturation";
const char _ZN7android16CameraParameters18KEY_METERING_AREASE[] = "metering-areas";
const char _ZN7android16CameraParameters18SCENE_MODE_FLOWERSE[] = "flowers";
const char _ZN7android16CameraParameters19KEY_SEMC_METRY_MODEE[] = "semc-metry-mode";
const char _ZN7android16CameraParameters20KEY_GPS_ALTITUDE_REFE[] = "gps-altitude-ref";
const char _ZN7android16CameraParameters20KEY_GPS_LATITUDE_REFE[] = "gps-latitude-ref";
const char _ZN7android16CameraParameters20KEY_REDEYE_REDUCTIONE[] = "redeye-reduction";
const char _ZN7android16CameraParameters20PIXEL_FORMAT_YUV420PE[] = "yuv420p";
const char _ZN7android16CameraParameters20SCENE_MODE_BACKLIGHTE[] = "backlight";
const char _ZN7android16CameraParameters21KEY_GPS_LONGITUDE_REFE[] = "gps-longitude-ref";
const char _ZN7android16CameraParameters22EX_SCENE_MODE_DOCUMENTE[] = "document";
const char _ZN7android16CameraParameters22KEY_SELECTABLE_ZONE_AFE[] = "selectable-zone-af";
const char _ZN7android16CameraParameters23AUTO_EXPOSURE_FRAME_AVGE[] = "frame-average";
const char _ZN7android16CameraParameters23KEY_MAX_NUM_FOCUS_AREASE[] = "max-num-focus-areas";
const char _ZN7android16CameraParameters23KEY_SUPPORTED_HFR_SIZESE[] = "supported-hfr-sizes";
const char _ZN7android16CameraParameters23KEY_SUPPORTED_ISO_MODESE[] = "supported-iso-modes";
const char _ZN7android16CameraParameters23KEY_SUPPORTED_ZSL_MODESE[] = "supported-zsl-modes";
const char _ZN7android16CameraParameters23REDEYE_REDUCTION_ENABLEE[] = "enable";
const char _ZN7android16CameraParameters23SELECTABLE_ZONE_AF_AUTOE[] = "auto";
const char _ZN7android16CameraParameters24REDEYE_REDUCTION_DISABLEE[] = "disable";
const char _ZN7android16CameraParameters25KEY_SKIN_TONE_ENHANCEMENTE[] = "skinToneEnhancement";
const char _ZN7android16CameraParameters25KEY_SUPPORTED_VIDEO_SIZESE[] = "video-size-values";
const char _ZN7android16CameraParameters25KEY_VIDEO_HIGH_FRAME_RATEE[] = "video-hfr";
const char _ZN7android16CameraParameters26KEY_MAX_NUM_METERING_AREASE[] = "max-num-metering-areas";
const char _ZN7android16CameraParameters26KEY_SUPPORTED_TOUCH_AF_AECE[] = "supported-touch-af-aec";
const char _ZN7android16CameraParameters27AUTO_EXPOSURE_SPOT_METERINGE[] = "spot-metering";
const char _ZN7android16CameraParameters27KEY_SUPPORTED_AUTO_EXPOSUREE[] = "auto-exposure-values";
const char _ZN7android16CameraParameters28KEY_MEMORY_COLOR_ENHANCEMENTE[] = "mce";
const char _ZN7android16CameraParameters28PIXEL_FORMAT_YUV420SP_ADRENOE[] = "yuv420sp-adreno";
const char _ZN7android16CameraParameters29KEY_SUPPORTED_LENSSHADE_MODESE[] = "lensshade-values";
const char _ZN7android16CameraParameters29SKIN_TONE_ENHANCEMENT_DISABLEE[] = "disable";
const char _ZN7android16CameraParameters30KEY_HIGH_DYNAMIC_RANGE_IMAGINGE[] = "hdr";
const char _ZN7android16CameraParameters31KEY_EX_SUPPORTED_METERING_MODESE[] = "metering-values";
const char _ZN7android16CameraParameters31KEY_SUPPORTED_HDR_IMAGING_MODESE[] = "hdr-values";
const char _ZN7android16CameraParameters32KEY_PREVIEW_FRAME_RATE_AUTO_MODEE[] = "auto";
const char _ZN7android16CameraParameters32SELECTABLE_ZONE_AF_FRAME_AVERAGEE[] = "frame-average";
const char _ZN7android16CameraParameters32SELECTABLE_ZONE_AF_SPOT_METERINGE[] = "spot-metering";
const char _ZN7android16CameraParameters33KEY_PREVIEW_FRAME_RATE_FIXED_MODEE[] = "fixed";
const char _ZN7android16CameraParameters34SELECTABLE_ZONE_AF_CENTER_WEIGHTEDE[] = "center-weighted";
const char _ZN7android16CameraParameters36KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEOE[] = "preferred-preview-size-for-video";
const char _ZN7android16CameraParameters37KEY_SUPPORTED_MEM_COLOR_ENHANCE_MODESE[] = "mce-values";
const char _ZN7android16CameraParameters6ZSL_ONE[] = "on";
const char _ZN7android16CameraParameters7ISO_100E[] = "ISO100";
const char _ZN7android16CameraParameters7ISO_200E[] = "ISO200";
const char _ZN7android16CameraParameters7ISO_400E[] = "ISO400";
const char _ZN7android16CameraParameters7ISO_800E[] = "ISO800";
const char _ZN7android16CameraParameters7ISO_HJRE[] = "ISO_HJR";
const char _ZN7android16CameraParameters7KEY_ZSLE[] = "zsl";
const char _ZN7android16CameraParameters7ZSL_OFFE[] = "off";
const char _ZN7android16CameraParameters8ISO_1600E[] = "ISO1600";
const char _ZN7android16CameraParameters8ISO_AUTOE[] = "auto";

/* ========================================================================= */
/* Persistent Disk Logger for Crash Diagnosis                                */
/* ========================================================================= */

static void disk_log(const char* fmt, ...) {
    static int s_file_log = -1;
    if (s_file_log < 0) {
        char prop[PROPERTY_VALUE_MAX] = {0};
        property_get("persist.camera.shim.log", prop, "0");
        s_file_log = (prop[0] == '1') ? 1 : 0;
    }
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    if (!s_file_log) return;
    FILE* fp = fopen("/data/local/tmp/shim.log", "a");
    if (fp) {
        fputs(buf, fp);
        fputc('\n', fp);
        fflush(fp);
        fclose(fp);
    }
}

/* ========================================================================= */
/* Dynamic GOT Hooking and Interception                                      */
/* ========================================================================= */

static int (*g_orig_ioctl)(int, int, ...) = NULL;
static int (*g_orig_open)(const char*, int, ...) = NULL;
static int (*g_orig_select)(int, fd_set*, fd_set*, fd_set*, struct timeval*) = NULL;
static int g_config_fd = -1;
static int g_frame_fd = -1;
static int g_cam_frame_fd = -1;
static volatile bool g_preview_active = false;
static volatile bool g_start_preview_in_progress = false;
static int g_frame_index = 0;
static uint32_t g_frame_seq = 0;

/* Control/Config thread synchronization */
struct gb_ctrl_cmd {
    uint16_t type;
    uint16_t length;
    void *value;
    uint16_t status;
    uint32_t timeout_ms;
    int resp_fd;
    int vnode_id;
    int queue_idx;
    uint32_t evt_id;
    uint32_t stream_type;
    int config_ident;
};

struct gb_cam_evt_msg {
    unsigned short type;
    unsigned short msg_id;
    unsigned int len;
    uint32_t frame_id;
    void *data;
    struct timespec timestamp;
};

struct gb_stats_event_ctrl {
    int resptype;
    int timeout_ms;
    struct gb_ctrl_cmd ctrl_cmd;
    struct gb_cam_evt_msg stats_event;
};

static pthread_mutex_t g_ctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_ctrl_queue_cond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t g_ctrl_done_cond = PTHREAD_COND_INITIALIZER;
static bool g_has_pending_ctrl = false;
static struct gb_ctrl_cmd g_pending_ctrl_cmd;
static bool g_ctrl_cmd_done = false;

struct preview_buf_info_t {
    void* base;
    int fd;
    size_t size;
    size_t offset;
};
static const int MAX_SHIM_BUFFERS = 8;
static preview_buf_info_t g_shim_buffers[MAX_SHIM_BUFFERS];
static int g_shim_buffer_count = 0;
static int g_shim_preview_w = 640;
static int g_shim_preview_h = 480;

struct shim_crop_info {
    uint32_t in_w;
    uint32_t out_w;
    uint32_t in_h;
    uint32_t out_h;
};
static shim_crop_info g_shim_crop = { 640, 640, 480, 480 };

struct gb_msm_frame {
    struct timespec ts;          /* offset 0..7 (8 bytes) */
    int path;                    /* offset 8 (4 bytes) */
    int type;                    /* offset 12 (4 bytes) */
    unsigned long buffer;        /* offset 16 (4 bytes) */
    uint32_t phy_offset;         /* offset 20 (4 bytes) */
    uint32_t y_off;              /* offset 24 (4 bytes) */
    uint32_t cbcr_off;           /* offset 28 (4 bytes) */
    uint32_t planar0_off;        /* offset 32 (4 bytes) */
    uint32_t planar1_off;        /* offset 36 (4 bytes) */
    uint32_t planar2_off;        /* offset 40 (4 bytes) */
    int fd;                      /* offset 44 (4 bytes) */
    void *cropinfo;              /* offset 48 (4 bytes) */
    int croplen;                 /* offset 52 (4 bytes) */
    uint32_t error_code;         /* offset 56 (4 bytes) */
    uint8_t roi_info[16];        /* offset 60..75 (16 bytes) */
    uint32_t frame_id;           /* offset 76 (4 bytes) */
    int stcam_quality_ind;       /* offset 80 (4 bytes) */
    uint32_t stcam_conv_value;   /* offset 84 (4 bytes) */
    uint8_t pad[16];             /* offset 88..103 (16 bytes) -> total 104 bytes */
};


struct msm_isp_cmd {
    int32_t  id;
    uint16_t length;
    void     *value;
};

struct msm_vfe_cfg_cmd {
    int cmd_type;
    uint16_t length;
    void *value;
};

static void init_real_libc();
static bool g_v4l2_streaming = false;
static enum v4l2_memory g_v4l2_memory_type = V4L2_MEMORY_USERPTR;
static int g_v4l2_preview_w = 640;
static int g_v4l2_preview_h = 480;
static struct {
    void* start;
    size_t length;
} g_v4l2_bufs[8];
static unsigned int g_v4l2_buf_count = 0;

static uint8_t g_last_sensor_frame[1280 * 960 * 3 / 2];
static volatile bool g_has_sensor_frame = false;
static volatile uint32_t g_sensor_frame_seq = 0;
static int g_sensor_frame_w = 640;
static int g_sensor_frame_h = 480;
static int64_t g_last_sensor_frame_ts_ns = 0;
static int64_t g_last_frame_done_ns = 0;
static pthread_mutex_t g_sensor_frame_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_pending_param_apply = false;
static volatile int g_neg_ev_level = 0;
static uint8_t g_neg_ev_lut[256];
static uint8_t g_neg_ev_uv_lut[256];

static void set_neg_ev_level(int ev) {
    if (ev >= 0) {
        g_neg_ev_level = 0;
        return;
    }
    if (ev < -6) ev = -6;
    /* 1/3 stop per step: gain = 2^(ev/3) scaled by 1024 */
    static const int kGainQ10[7] = { 1024, 813, 645, 512, 406, 323, 256 };
    int g = kGainQ10[-ev];
    for (int i = 0; i < 256; i++) {
        int y_out = 16 + (((i - 16) * g + 512) >> 10);
        if (y_out < 16) y_out = 16;
        if (y_out > 235) y_out = 235;
        g_neg_ev_lut[i] = (uint8_t)y_out;

        int uv_out = 128 + (((i - 128) * g + 512) >> 10);
        if (uv_out < 16) uv_out = 16;
        if (uv_out > 240) uv_out = 240;
        g_neg_ev_uv_lut[i] = (uint8_t)uv_out;
    }
    g_neg_ev_level = ev;
}

static inline void apply_neg_ev_y_plane(uint8_t* nv21, size_t y_count) {
    if (g_neg_ev_level >= 0 || !nv21) return;
    const uint8_t* y_lut = g_neg_ev_lut;
    const uint8_t* uv_lut = g_neg_ev_uv_lut;
    for (size_t i = 0; i < y_count; i++) {
        nv21[i] = y_lut[nv21[i]];
    }
    uint8_t* uv = nv21 + y_count;
    size_t uv_count = y_count / 2;
    for (size_t i = 0; i < uv_count; i++) {
        uv[i] = uv_lut[uv[i]];
    }
}
static void adapter_on_stream_ready();

static uint8_t* g_cached_stage_bufs[8] = {0};
static size_t g_cached_stage_sizes[8] = {0};
static int64_t g_cached_stage_ts_ns[8] = {0};
static volatile int g_last_stage_idx = 0;

static int64_t now_monotonic_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static const int V4L2_CAP_BUF_COUNT = 4;
static int g_v4l2_cap_fd = -1;
static void* g_v4l2_cap_base = NULL;
static size_t g_v4l2_cap_total_size = 0;
static struct {
    void* base;
    size_t offset;
    size_t size;
} g_v4l2_cap_bufs[V4L2_CAP_BUF_COUNT];

static int s_v4l2_fail_count = 0;
static volatile bool g_hal_preview_started = false;

static void free_v4l2_cap_pool() {
    if (g_v4l2_cap_base && g_v4l2_cap_total_size > 0) {
        munmap(g_v4l2_cap_base, g_v4l2_cap_total_size);
    }
    g_v4l2_cap_base = NULL;
    g_v4l2_cap_total_size = 0;
    if (g_v4l2_cap_fd >= 0) {
        close(g_v4l2_cap_fd);
        g_v4l2_cap_fd = -1;
    }
    memset(g_v4l2_cap_bufs, 0, sizeof(g_v4l2_cap_bufs));
}

static void stop_v4l2_streaming() {
    if (!g_v4l2_streaming) return;
    init_real_libc();
    s_v4l2_fail_count = 0;
    if (g_config_fd >= 0 && g_orig_ioctl) {
        struct msm_isp_cmd stop_isp_cmd;
        memset(&stop_isp_cmd, 0, sizeof(stop_isp_cmd));
        stop_isp_cmd.id = 65; /* VFE_CMD_STOP */
        stop_isp_cmd.length = 0;
        stop_isp_cmd.value = NULL;

        struct msm_vfe_cfg_cmd stop_cfg;
        memset(&stop_cfg, 0, sizeof(stop_cfg));
        stop_cfg.cmd_type = 0; /* CMD_GENERAL */
        stop_cfg.length = sizeof(stop_isp_cmd);
        stop_cfg.value = &stop_isp_cmd;

        int stop_rc = g_orig_ioctl(g_config_fd, 0x40046d05, &stop_cfg);
        disk_log("stop_v4l2_streaming: VFE_CMD_STOP on g_config_fd=%d -> rc=%d (errno=%d)",
                 g_config_fd, stop_rc, errno);
    }
    if (g_frame_fd >= 0 && g_orig_ioctl) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        g_orig_ioctl(g_frame_fd, VIDIOC_STREAMOFF, &type);
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 0;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = g_v4l2_memory_type;
        g_orig_ioctl(g_frame_fd, VIDIOC_REQBUFS, &req);
    }
    if (g_v4l2_memory_type == V4L2_MEMORY_MMAP) {
        for (unsigned int i = 0; i < g_v4l2_buf_count; i++) {
            if (g_v4l2_bufs[i].start && g_v4l2_bufs[i].start != MAP_FAILED) {
                munmap(g_v4l2_bufs[i].start, g_v4l2_bufs[i].length);
                g_v4l2_bufs[i].start = NULL;
            }
        }
    }
    free_v4l2_cap_pool();
    g_v4l2_buf_count = 0;
    g_v4l2_streaming = false;
    g_has_sensor_frame = false;
    disk_log("stop_v4l2_streaming: V4L2 stream stopped");
}

static void scale_nv21(const uint8_t *src, int src_w, int src_h,
                       uint8_t *dst, int dst_w, int dst_h) {
    if (!src || !dst || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    if (src_w == dst_w && src_h == dst_h) {
        memcpy(dst, src, (size_t)dst_w * dst_h * 3 / 2);
        return;
    }
    uint32_t step_x = ((uint32_t)src_w << 16) / (uint32_t)dst_w;
    uint32_t step_y = ((uint32_t)src_h << 16) / (uint32_t)dst_h;
    // Y plane
    uint32_t fy = 0;
    for (int y = 0; y < dst_h; y++, fy += step_y) {
        const uint8_t *src_row = src + (fy >> 16) * src_w;
        uint8_t *dst_row = dst + y * dst_w;
        uint32_t fx = 0;
        for (int x = 0; x < dst_w; x++, fx += step_x) {
            dst_row[x] = src_row[fx >> 16];
        }
    }
    // VU plane
    const uint8_t *src_uv = src + src_w * src_h;
    uint8_t *dst_uv = dst + dst_w * dst_h;
    int dst_uv_h = dst_h >> 1;
    int dst_uv_w = dst_w >> 1;
    uint32_t fy_uv = 0;
    for (int y = 0; y < dst_uv_h; y++, fy_uv += step_y) {
        const uint16_t *src_uv_row = (const uint16_t *)(src_uv + (fy_uv >> 16) * src_w);
        uint16_t *dst_uv_row = (uint16_t *)(dst_uv + y * dst_w);
        uint32_t fx = 0;
        for (int x = 0; x < dst_uv_w; x++, fx += step_x) {
            dst_uv_row[x] = src_uv_row[fx >> 16];
        }
    }
}

static void subscribe_config_events(int conf_fd);
static void drain_config_events(int conf_fd, const char *tag, int wait_ms);

static int send_vfe_general_cmd_retry(int conf_fd, uint32_t id, void *val, uint16_t len, const char *name) {
    struct msm_isp_cmd isp_cmd;
    memset(&isp_cmd, 0, sizeof(isp_cmd));
    isp_cmd.id = id;
    isp_cmd.length = len;
    isp_cmd.value = val;

    struct msm_vfe_cfg_cmd cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cmd_type = 0; /* CMD_GENERAL */
    cfg.length = sizeof(isp_cmd);
    cfg.value = &isp_cmd;

    int rc = -1;
    int tries = 0;
    for (int r = 0; r < 30; r++) {
        tries = r + 1;
        errno = 0;
        rc = g_orig_ioctl(conf_fd, 0x40046d05, &cfg);
        if (rc == 0 || errno != EAGAIN) {
            break;
        }
        usleep(20000);
    }
    disk_log("start_v4l2_streaming: %s (id=%u, len=%u) on conf_fd=%d -> rc=%d (errno=%d, tries=%d)",
             name, id, (unsigned int)len, conf_fd, rc, errno, tries);
    return rc;
}

static int start_v4l2_streaming(int fd, int w, int h) {
    if (g_v4l2_streaming) return 0;
    if (fd < 0) return -1;
    init_real_libc();
    if (!g_orig_ioctl) return -1;

    if (s_v4l2_fail_count >= 10) return -1;

    int req_w = 640;
    int req_h = 480;

    disk_log("start_v4l2_streaming: starting on fd=%d, g_config_fd=%d, req=%dx%d", fd, g_config_fd, req_w, req_h);

    if (g_config_fd >= 0) {
        uint32_t vfe_output_mode = 0x40; /* VFE_OUTPUTS_PREVIEW */
        int rc = g_orig_ioctl(g_config_fd, 0x40046d32, &vfe_output_mode);
        disk_log("start_v4l2_streaming: MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE (0x40) on g_config_fd=%d -> rc=%d (errno=%d)",
                 g_config_fd, rc, errno);
    }
    if (fd >= 0 && fd != g_config_fd) {
        uint32_t vfe_output_mode = 0x40;
        int rc = g_orig_ioctl(fd, 0x40046d32, &vfe_output_mode);
        disk_log("start_v4l2_streaming: MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE (0x40) on fd=%d -> rc=%d (errno=%d)",
                 fd, rc, errno);
    }

    int fl = fcntl(fd, F_GETFL, 0);
    if (fl >= 0) {
        fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.extendedmode = 1; /* MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW */
    if (g_orig_ioctl(fd, VIDIOC_S_PARM, &parm) < 0) {
        disk_log("start_v4l2_streaming: VIDIOC_S_PARM (extendedmode=1) failed: %s (%d)", strerror(errno), errno);
    } else {
        disk_log("start_v4l2_streaming: VIDIOC_S_PARM (extendedmode=1) SUCCESS");
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = req_w;
    fmt.fmt.pix.height = req_h;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV21;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (g_orig_ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        disk_log("start_v4l2_streaming: VIDIOC_S_FMT (%dx%d NV21) failed: %s (%d)", fmt.fmt.pix.width, fmt.fmt.pix.height, strerror(errno), errno);
        s_v4l2_fail_count++;
        return -1;
    }
    g_v4l2_preview_w = fmt.fmt.pix.width;
    g_v4l2_preview_h = fmt.fmt.pix.height;
    disk_log("start_v4l2_streaming: VIDIOC_S_FMT success: %dx%d", g_v4l2_preview_w, g_v4l2_preview_h);

    free_v4l2_cap_pool();
    bool use_userptr = false;
    int req_rc = -1;
    size_t min_buf_sz = ((size_t)req_w * req_h * 3 / 2 + 4095) & ~4095;
    size_t cap_total = min_buf_sz * V4L2_CAP_BUF_COUNT;
    g_v4l2_cap_fd = g_orig_open ? g_orig_open("/dev/pmem_adsp", O_RDWR, 0) : open("/dev/pmem_adsp", O_RDWR, 0);
    if (g_v4l2_cap_fd >= 0) {
        g_v4l2_cap_base = mmap(NULL, cap_total, PROT_READ | PROT_WRITE, MAP_SHARED, g_v4l2_cap_fd, 0);
        if (g_v4l2_cap_base == MAP_FAILED) {
            g_v4l2_cap_base = NULL;
            close(g_v4l2_cap_fd);
            g_v4l2_cap_fd = -1;
        } else {
            g_v4l2_cap_total_size = cap_total;
            for (int i = 0; i < V4L2_CAP_BUF_COUNT; i++) {
                g_v4l2_cap_bufs[i].base = (char*)g_v4l2_cap_base + (size_t)i * min_buf_sz;
                g_v4l2_cap_bufs[i].offset = (size_t)i * min_buf_sz;
                g_v4l2_cap_bufs[i].size = min_buf_sz;
            }
        }
    }
    if (g_v4l2_cap_fd >= 0 && g_v4l2_cap_base != NULL) {
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = V4L2_CAP_BUF_COUNT;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;
        req_rc = g_orig_ioctl(fd, VIDIOC_REQBUFS, &req);
        disk_log("start_v4l2_streaming: VIDIOC_REQBUFS USERPTR dedicated pool (count=%d) -> rc=%d (errno=%d)",
                 V4L2_CAP_BUF_COUNT, req_rc, errno);
        if (req_rc == 0 && req.count > 0 && req.count <= (unsigned int)V4L2_CAP_BUF_COUNT) {
            use_userptr = true;
            g_v4l2_memory_type = V4L2_MEMORY_USERPTR;
            g_v4l2_buf_count = req.count;
            for (unsigned int i = 0; i < req.count; i++) {
                struct v4l2_buffer buf;
                memset(&buf, 0, sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)g_v4l2_cap_fd;
                buf.reserved = g_v4l2_cap_bufs[i].offset;
                buf.length = g_v4l2_cap_bufs[i].size;
                int q_rc = g_orig_ioctl(fd, VIDIOC_QBUF, &buf);
                disk_log("start_v4l2_streaming: cap USERPTR QBUF %u (fd=%d, off=%zu, len=%zu) -> rc=%d (errno=%d)",
                         i, g_v4l2_cap_fd, g_v4l2_cap_bufs[i].offset, g_v4l2_cap_bufs[i].size, q_rc, errno);
            }
        } else {
            free_v4l2_cap_pool();
        }
    }

    if (!use_userptr) {
        disk_log("start_v4l2_streaming: fallback to MMAP (count=2) for %dx%d (%zu bytes)", req_w, req_h, min_buf_sz);
        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = 2;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        req_rc = g_orig_ioctl(fd, VIDIOC_REQBUFS, &req);
        disk_log("start_v4l2_streaming: VIDIOC_REQBUFS MMAP (count=2) -> rc=%d (errno=%d)", req_rc, errno);
        if (req_rc < 0 || req.count == 0) {
            disk_log("start_v4l2_streaming: VIDIOC_REQBUFS MMAP failed: %s (%d)", strerror(errno), errno);
            s_v4l2_fail_count++;
            return -1;
        }
        g_v4l2_memory_type = V4L2_MEMORY_MMAP;
        g_v4l2_buf_count = req.count;
        for (unsigned int i = 0; i < req.count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (g_orig_ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
                disk_log("start_v4l2_streaming: VIDIOC_QUERYBUF %u failed: %s", i, strerror(errno));
                s_v4l2_fail_count++;
                return -1;
            }
            g_v4l2_bufs[i].length = buf.length;
            g_v4l2_bufs[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            if (g_v4l2_bufs[i].start == MAP_FAILED) {
                disk_log("start_v4l2_streaming: mmap %u failed: %s", i, strerror(errno));
                s_v4l2_fail_count++;
                return -1;
            }
            if (g_orig_ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                disk_log("start_v4l2_streaming: VIDIOC_QBUF %u failed: %s", i, strerror(errno));
                s_v4l2_fail_count++;
                return -1;
            }
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (g_orig_ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        disk_log("start_v4l2_streaming: VIDIOC_STREAMON failed: %s (%d)", strerror(errno), errno);
        s_v4l2_fail_count++;
        return -1;
    }
    disk_log("start_v4l2_streaming: VIDIOC_STREAMON SUCCESS, pcam_inst->streamon is now 1");

    int conf_fd = (g_config_fd >= 0) ? g_config_fd : fd;
    if (conf_fd >= 0 && g_orig_ioctl) {
        subscribe_config_events(conf_fd);
        // 0. VFE_CMD_RESET (id = 2)
        int reset_rc = send_vfe_general_cmd_retry(conf_fd, 2, NULL, 0, "VFE_CMD_RESET");
        if (reset_rc == 0) {
            usleep(20000);
            drain_config_events(conf_fd, "after_RESET", 50);
        }

        // 1. VFE_CMD_CAMIF_CFG (id = 7, 32 bytes payload): NO CAMIF skipping (0xffffffff), full 1280x960 YUYV
        uint32_t camif_buf[8];
        memset(camif_buf, 0, sizeof(camif_buf));
        camif_buf[0] = 0x0100;                // cfg: MIPI_EN (0x0100), VFE_SUBSAMP_EN_DIS
        camif_buf[1] = 0;                     // efs_cfg
        camif_buf[2] = 2560 | (960 << 14);    // frame_cfg: 1280*2 YUV clocks x 960 lines
        camif_buf[3] = 2559;                  // window_width_cfg: lastPixel = 2559
        camif_buf[4] = 959;                   // window_height_cfg: lastLine = 959
        camif_buf[5] = 0xffffffff;            // subsamp1_cfg: no skip
        camif_buf[6] = 0x00000000;            // subsamp2_cfg
        camif_buf[7] = 0x0fffffff;            // epoch_irq

        int camif_rc = send_vfe_general_cmd_retry(conf_fd, 7, camif_buf, sizeof(camif_buf), "VFE_CMD_CAMIF_CFG");

        // 2. VFE_CMD_DEMUX_CFG (id = 11, 12 bytes payload)
        uint32_t demux_buf[3];
        demux_buf[0] = 0x0e; // CBYCRY (6) | CHROMA (8)
        demux_buf[1] = 0x00800080;
        demux_buf[2] = 0x00800080;

        int demux_rc = send_vfe_general_cmd_retry(conf_fd, 11, demux_buf, sizeof(demux_buf), "VFE_CMD_DEMUX_CFG");

        // 3. VFE_CMD_FRAME_SKIP_CFG (id = 27 in msm_isp.h, 8 bytes payload)
        uint32_t skip_buf[2];
        skip_buf[0] = 0x3fffffff;
        skip_buf[1] = 0x3fffffff;

        int skip_rc = send_vfe_general_cmd_retry(conf_fd, 27, skip_buf, sizeof(skip_buf), "VFE_CMD_FRAME_SKIP_CFG");

        // 1b. VFE_CMD_ACTIVE_REGION_CFG (id = 129, 8 bytes payload): full 2560 clocks x 960 lines
        uint32_t active_buf[2];
        active_buf[0] = (0u << 16) | 2559u;
        active_buf[1] = (0u << 16) | 959u;
        send_vfe_general_cmd_retry(conf_fd, 129, active_buf, sizeof(active_buf), "VFE_CMD_ACTIVE_REGION_CFG");

        // 3b. VFE_CMD_FOV_CFG (id = 12, 8 bytes payload): 1280 luma pixels x 960 lines
        uint32_t fov_buf[2];
        fov_buf[0] = (0u << 16) | 1279u;
        fov_buf[1] = (0u << 16) | 959u;
        send_vfe_general_cmd_retry(conf_fd, 12, fov_buf, sizeof(fov_buf), "VFE_CMD_FOV_CFG");

        // 3c. VFE_CMD_CHROMA_SUBS_CFG (id = 25, 4 bytes payload)
        uint32_t chroma_subs = 0x01; // VFE_DefaultChromaSubsampleCfgCmd = 0x01 (SCALE_OUTPUT2 outputs 320x240 CbCr)
        send_vfe_general_cmd_retry(conf_fd, 25, &chroma_subs, sizeof(chroma_subs), "VFE_CMD_CHROMA_SUBS_CFG");

        // 3d. VFE_CMD_OUT_CLAMP_CFG (id = 26, 8 bytes payload)
        uint32_t clamp_buf[2];
        clamp_buf[0] = 0x00ffffff;
        clamp_buf[1] = 0x00000000;
        send_vfe_general_cmd_retry(conf_fd, 26, clamp_buf, sizeof(clamp_buf), "VFE_CMD_OUT_CLAMP_CFG");

        // 4. OPERATION_CFG (VFE_CMD_OPERATION_CFG = 5)
        struct {
            uint32_t input_source:1;
            uint32_t mode_of_operation:1;
            uint32_t snap_number:4;
            uint32_t reserved1:26;
            uint32_t modules_enable;
            uint32_t last_pixel:12;
            uint32_t reserved2:4;
            uint32_t last_line:12;
            uint32_t reserved3:4;
        } start_cmd;
        memset(&start_cmd, 0, sizeof(start_cmd));
        start_cmd.input_source = 0; // 0 = CAMIF live sensor
        start_cmd.mode_of_operation = 0; // 0 = continuous/preview
        start_cmd.snap_number = 8;
        start_cmd.modules_enable = 0x40; // VFE_CMD_IMAGE_PL_CHROMA_SAMP_ENA
        start_cmd.last_pixel = 1279;
        start_cmd.last_line = 959;

        int op_rc = send_vfe_general_cmd_retry(conf_fd, 5, &start_cmd, sizeof(start_cmd), "VFE_CMD_OPERATION_CFG");

        // 4b. VFE_CMD_SCALE_OUTPUT2_CONFIG (id = 135, 288 bytes): hardware 1280x960 -> 640x480 polyphase scaler
        static const int16_t poly_coeffs[64] = {
            0x000, 0x06f, 0x132, 0x05f, 0x001, 0x07f, 0x12f, 0x051,
            0x002, 0x090, 0x12a, 0x044, 0x004, 0x0a1, 0x124, 0x037,
            0x006, 0x0b3, 0x11b, 0x02c, 0x008, 0x0c4, 0x110, 0x024,
            0x00c, 0x0d5, 0x103, 0x01c, 0x010, 0x0e5, 0x0f5, 0x016,
            0x015, 0x0f5, 0x0e5, 0x011, 0x01c, 0x103, 0x0d5, 0x00c,
            0x024, 0x110, 0x0c4, 0x008, 0x02d, 0x11b, 0x0b3, 0x005,
            0x038, 0x124, 0x0a1, 0x003, 0x044, 0x12a, 0x090, 0x002,
            0x051, 0x12f, 0x07f, 0x001, 0x05f, 0x132, 0x06f, 0x000
        };
        uint32_t scale2_buf[72];
        memset(scale2_buf, 0, sizeof(scale2_buf));
        uint32_t dim_in = (960u << 16) | 1280u;
        uint32_t dim_out = (480u << 16) | 640u;
        scale2_buf[0] = 0x00ca;       // H_Y | V_Y | H_CBCR | V_CBCR
        scale2_buf[1] = dim_in;       // y_scaler_cfg_part1 (in)
        scale2_buf[2] = dim_out;      // y_scaler_cfg_part2 (out)
        scale2_buf[3] = dim_in;       // cbcr_scaler_cfg_part1 (in)
        scale2_buf[4] = (240u << 16) | 320u; // cbcr_scaler_cfg_part2 (out_h/2, out_w/2 per liboemcamera.so)
        scale2_buf[5] = 0;            // cbcr_scaler_cfg_part3
        scale2_buf[6] = 0x80000000u | dim_out; // pp_y_scaler_cfg_part1 (LOAD_COEFFS)
        scale2_buf[7] = dim_out;      // pp_y_scaler_cfg_part2
        for (int i = 0; i < 16; i++) {
            int16_t c0 = poly_coeffs[i * 4 + 0];
            int16_t c1 = poly_coeffs[i * 4 + 1];
            int16_t c2 = poly_coeffs[i * 4 + 2];
            int16_t c3 = poly_coeffs[i * 4 + 3];
            uint32_t w0 = (uint32_t)((c0 >> 2) & 0xff);
            uint32_t w1 = ((uint32_t)(c3 & 0x3ff)) |
                          (((uint32_t)(c2 & 0x3ff)) << 10) |
                          (((uint32_t)(c1 & 0x3ff)) << 20) |
                          (((uint32_t)(c0 & 0x3)) << 30);
            scale2_buf[8 + i * 2]      = w0; // V coeff bank
            scale2_buf[8 + i * 2 + 1]  = w1;
            scale2_buf[40 + i * 2]     = w0; // H coeff bank
            scale2_buf[40 + i * 2 + 1] = w1;
        }
        send_vfe_general_cmd_retry(conf_fd, 135, scale2_buf, sizeof(scale2_buf), "VFE_CMD_SCALE_OUTPUT2_CONFIG");

        // 5. AXI_CONFIG (CMD_AXI_CFG_PRIM = 0x100)
        uint32_t axiout_buf[42];
        memset(axiout_buf, 0, sizeof(axiout_buf));

        int w = 640;
        int h = 480;
        int width_words = (w + 7) / 8;
        int h_y = h;
        int h_cbcr = (h + 1) / 2;

        uint32_t part1_y = ((width_words & 0x3ff) << 16) | (h_y & 0xfff);
        uint32_t part2_y = ((width_words & 0xfff) << 14) | ((h_y & 0xfff) << 2) | 1;
        uint32_t part1_cbcr = ((width_words & 0x3ff) << 16) | (h_cbcr & 0xfff);
        uint32_t part2_cbcr = ((width_words & 0xfff) << 14) | ((h_cbcr & 0xfff) << 2) | 1;

        axiout_buf[0] = 1;          // outputmode = 1 (Output 2 only), format = 0 (YCbCr)
        axiout_buf[1] = part1_y;    // out1y Part 1
        axiout_buf[2] = part2_y;    // out1y Part 2
        axiout_buf[3] = part1_cbcr; // out1cbcr Part 1
        axiout_buf[4] = part2_cbcr; // out1cbcr Part 2
        axiout_buf[5] = part1_y;    // out2y Part 1
        axiout_buf[6] = part2_y;    // out2y Part 2
        axiout_buf[7] = part1_cbcr; // out2cbcr Part 1
        axiout_buf[8] = part2_cbcr; // out2cbcr Part 2

        struct msm_isp_cmd axi_isp_cmd;
        memset(&axi_isp_cmd, 0, sizeof(axi_isp_cmd));
        axi_isp_cmd.id = 6; /* VFE_CMD_AXI_OUT_CFG */
        axi_isp_cmd.length = 164; /* 164 + 4 cmd_id = 168 (sizeof vfe_cmd_axi_op_cfg) */ /* sizeof(struct axiout) */
        axi_isp_cmd.value = axiout_buf;

        struct msm_vfe_cfg_cmd axi_cfg;
        memset(&axi_cfg, 0, sizeof(axi_cfg));
        axi_cfg.cmd_type = 0x100; /* CMD_AXI_CFG_PRIM */
        axi_cfg.length = sizeof(axi_isp_cmd);
        axi_cfg.value = &axi_isp_cmd;

        errno = 0;
        int axi_rc = g_orig_ioctl(conf_fd, 0x40046d10, &axi_cfg);
        disk_log("start_v4l2_streaming: MSM_CAM_IOCTL_AXI_CONFIG(CMD_AXI_CFG_PRIM) on conf_fd=%d -> rc=%d (errno=%d)",
                 conf_fd, axi_rc, errno);

        usleep(30000);
        // 6. VFE_CMD_START = 3
        int start_rc = send_vfe_general_cmd_retry(conf_fd, 3, &start_cmd, sizeof(start_cmd), "VFE_CMD_START");
        drain_config_events(conf_fd, "after_START", 250);

        if (op_rc != 0 || axi_rc != 0 || start_rc != 0) {
            disk_log("start_v4l2_streaming: VFE PIPELINE FAILED (rst=%d, camif=%d, demux=%d, skip=%d, op=%d, axi=%d, start=%d), will retry",
                     reset_rc, camif_rc, demux_rc, skip_rc, op_rc, axi_rc, start_rc);
            s_v4l2_fail_count++;
            return -1;
        }
    }

        g_v4l2_streaming = true;
    s_v4l2_fail_count = 0;
    disk_log("start_v4l2_streaming: V4L2 STREAMON SUCCESS! type=%s, %dx%d, %u bufs",
             use_userptr ? "USERPTR" : "MMAP", g_v4l2_preview_w, g_v4l2_preview_h, g_v4l2_buf_count);
    adapter_on_stream_ready();
    return 0;
}

static void init_real_libc() {
    if (!g_orig_ioctl) {
        void* libc = dlopen("libc.so", RTLD_NOW);
        if (libc) {
            g_orig_ioctl = (int (*)(int, int, ...))dlsym(libc, "ioctl");
            LOGI("init_real_libc: resolved libc ioctl = %p", g_orig_ioctl);
        }
    }
    if (!g_orig_open) {
        void* libc = dlopen("libc.so", RTLD_NOW);
        if (libc) {
            g_orig_open = (int (*)(const char*, int, ...))dlsym(libc, "open");
            LOGI("init_real_libc: resolved libc open = %p", g_orig_open);
        }
    }
    if (!g_orig_select) {
        void* libc = dlopen("libc.so", RTLD_NOW);
        if (libc) {
            g_orig_select = (int (*)(int, fd_set*, fd_set*, fd_set*, struct timeval*))dlsym(libc, "select");
            LOGI("init_real_libc: resolved libc select = %p", g_orig_select);
        }
    }
}

static uintptr_t get_lib_base(const char* libname) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) {
        LOGE("get_lib_base: could not open /proc/self/maps");
        return 0;
    }
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libname)) {
            char* endptr = NULL;
            base = (uintptr_t)strtoul(line, &endptr, 16);
            break;
        }
    }
    fclose(fp);
    return base;
}

static void setup_camera_callbacks();

static void inject_sensor_dimensions() {
    uintptr_t oem_base = get_lib_base("liboemcamera.so");
    if (!oem_base) {
        disk_log("inject_sensor_dimensions: liboemcamera.so base not found");
        LOGW("inject_sensor_dimensions: liboemcamera.so base not found");
        return;
    }
    uint32_t* cam_ctrl = (uint32_t*)(oem_base + 0x9c7c0);
    disk_log("inject_sensor_dimensions: BEFORE: snap=%ux%u, prev=%ux%u, state=%u",
             cam_ctrl[60/4], cam_ctrl[64/4], cam_ctrl[68/4], cam_ctrl[72/4], cam_ctrl[168/4]);
    cam_ctrl[60 / 4] = 2592; /* max snapshot width (isx006: 2592x1944) */
    cam_ctrl[64 / 4] = 1944; /* max snapshot height */
    cam_ctrl[68 / 4] = 1280; /* max preview width (1280x960) */
    cam_ctrl[72 / 4] = 960;  /* max preview height */
    cam_ctrl[168 / 4] = 3;   /* camera state: 3 = ready for query_parms */
    disk_log("inject_sensor_dimensions: AFTER: snap=%ux%u, prev=%ux%u, state=%u (addr=0x%08x)",
             cam_ctrl[60/4], cam_ctrl[64/4], cam_ctrl[68/4], cam_ctrl[72/4], cam_ctrl[168/4],
             (unsigned int)(oem_base + 0x9c7c0));
    LOGI("inject_sensor_dimensions: oem_base=0x%08x: injected 2592x1944, preview 1280x960, state=3",
         (unsigned int)oem_base);
    setup_camera_callbacks();
}


static int my_hooked_ioctl(int fd, int request, ...) {
    void* arg = NULL;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void*);
    va_end(ap);

    if ((unsigned int)request != 0x80046d07 && (unsigned int)request != 0x40046d0e && (unsigned int)request != 0x40046d02) {
        disk_log("ioctl ENTER: fd=%d, req=0x%08x, arg=%p", fd, (unsigned int)request, arg);
    }

    if ((unsigned int)request == 0x80046d24) { /* MSM_CAM_IOCTL_GET_CAMERA_INFO */
        disk_log("ioctl INTERCEPTED MSM_CAM_IOCTL_GET_CAMERA_INFO (0x80046d24)!");
        LOGI("my_hooked_ioctl: INTERCEPTED MSM_CAM_IOCTL_GET_CAMERA_INFO (0x80046d24)!");
        struct cam_info {
            int num_cameras;
            uint8_t has_3d_support[5];
            uint8_t is_internal_cam[5];
            uint8_t pad[2];
            uint32_t s_mount_angle[5];
            const char *video_dev_name[5];
            int sensor_type[5];
            int hw_version[5];
        };
        struct cam_info *info = (struct cam_info *)arg;
        if (info) {
            memset(info, 0, sizeof(*info));
            info->num_cameras = 1;
            info->has_3d_support[0] = 0;
            info->is_internal_cam[0] = 0; /* back camera */
            info->s_mount_angle[0] = 90;
            info->video_dev_name[0] = "/dev/video0";
            disk_log("ioctl INTERCEPTED: returning num_cameras=1, video_dev=/dev/video0");
            LOGI("my_hooked_ioctl: returned num_cameras=1, mount_angle=90, video_dev=/dev/video0");
            return 0;
        }
    }

    if ((unsigned int)request == 0x80046d01) { /* MSM_CAM_IOCTL_GET_SENSOR_INFO */
        disk_log("ioctl INTERCEPTED MSM_CAM_IOCTL_GET_SENSOR_INFO (0x80046d01)!");
        LOGI("my_hooked_ioctl: INTERCEPTED MSM_CAM_IOCTL_GET_SENSOR_INFO (0x80046d01)!");
        /* Gingerbread struct is exactly 36 bytes: 32 bytes name + 4 bytes flags */
        struct gb_camsensor_info {
            char name[32];
            uint8_t flash_enabled;
            uint8_t strobe_flash_enabled;
            uint8_t actuator_enabled;
            uint8_t ispif_supported;
        };
        struct gb_camsensor_info *info = (struct gb_camsensor_info *)arg;
        if (info) {
            memset(info, 0, sizeof(*info));
            strcpy(info->name, "isx006");
            info->flash_enabled = 1;
            info->strobe_flash_enabled = 0;
            info->actuator_enabled = 0;
            info->ispif_supported = 0;
            disk_log("ioctl INTERCEPTED MSM_CAM_IOCTL_GET_SENSOR_INFO: returned isx006 (36 bytes) successfully!");
            LOGI("my_hooked_ioctl: returned isx006 (36 bytes) successfully!");
            return 0;
        }
    }

    if ((unsigned int)request == 0x40046d02 || (unsigned int)request == 0x40046d03) { /* MSM_CAM_IOCTL_REGISTER_PMEM / UNREGISTER_PMEM */
        struct shim_pmem_info {
            int type;
            int fd;
            void *vaddr;
            uint32_t y_off;
            uint32_t cbcr_off;
            uint8_t active;
        };
        struct shim_pmem_info *pinfo = (struct shim_pmem_info *)arg;
        if (pinfo) {
            static int s_pmem_cnt = 0;
            if ((++s_pmem_cnt % 60) == 1) {
                disk_log("ioctl INTERCEPTED PMEM (0x%08x) on fd=%d: type=%d, fd=%d, vaddr=%p -> 0 (cnt=%d)",
                         (unsigned int)request, fd, pinfo->type, pinfo->fd, pinfo->vaddr, s_pmem_cnt);
            }
        }
        return 0;
    }

    if ((unsigned int)request == 0x40046d20) { /* MSM_CAM_IOCTL_FLASH_CTRL */
        disk_log("ioctl INTERCEPTED MSM_CAM_IOCTL_FLASH_CTRL (0x40046d20) -> 0");
        LOGI("my_hooked_ioctl: INTERCEPTED MSM_CAM_IOCTL_FLASH_CTRL (0x40046d20) -> 0");
        return 0;
    }

    if ((unsigned int)request == 0x80046d07) { /* MSM_CAM_IOCTL_GETFRAME */
        struct gb_msm_frame *f = (struct gb_msm_frame *)arg;
        if (f && g_shim_buffer_count > 0) {
            int max_bufs = g_shim_buffer_count;
            int idx = g_frame_index % max_bufs;
            g_frame_index = (idx + 1) % max_bufs;

            if (!g_v4l2_streaming && g_frame_fd >= 0) {
                start_v4l2_streaming(g_frame_fd, g_shim_preview_w, g_shim_preview_h);
            }

            bool got_real_frame = false;
            if (g_v4l2_streaming && g_frame_fd >= 0) {
                struct v4l2_buffer vbuf;
                memset(&vbuf, 0, sizeof(vbuf));
                vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                vbuf.memory = g_v4l2_memory_type;
                int dq_rc = -1;
                for (int r = 0; r < 60 && g_preview_active; r++) {
                    errno = 0;
                    dq_rc = g_orig_ioctl(g_frame_fd, VIDIOC_DQBUF, &vbuf);
                    if (dq_rc == 0 || (errno != EAGAIN && errno != EINTR)) {
                        break;
                    }
                    usleep(3000);
                }
                if (dq_rc == 0) {
                    size_t frame_bytes = (size_t)g_v4l2_preview_w * g_v4l2_preview_h * 3 / 2;
                    if (frame_bytes > sizeof(g_last_sensor_frame)) frame_bytes = sizeof(g_last_sensor_frame);
                    if (g_v4l2_memory_type == V4L2_MEMORY_USERPTR) {
                        unsigned int cidx = vbuf.index;
                        if (cidx < (unsigned int)V4L2_CAP_BUF_COUNT && g_v4l2_cap_bufs[cidx].base) {
                            if (g_v4l2_cap_fd >= 0 && g_orig_ioctl) {
                                struct { unsigned long vaddr; unsigned long offset; unsigned long length; } pm = {
                                    (unsigned long)g_v4l2_cap_bufs[cidx].base,
                                    (unsigned long)g_v4l2_cap_bufs[cidx].offset,
                                    (unsigned long)frame_bytes
                                };
                                g_orig_ioctl(g_v4l2_cap_fd, 0x4004700d /* PMEM_INV_CACHES */, &pm);
                            }
                            int64_t dq_ts_ns = now_monotonic_ns();
                            size_t out_bytes = (size_t)g_shim_preview_w * g_shim_preview_h * 3 / 2;
                            uint8_t* stage_dst = (g_cached_stage_bufs[idx] && g_cached_stage_sizes[idx] >= out_bytes)
                                                 ? g_cached_stage_bufs[idx]
                                                 : (uint8_t*)g_shim_buffers[idx].base;
                            pthread_mutex_lock(&g_sensor_frame_mutex);
                            memcpy(g_last_sensor_frame, g_v4l2_cap_bufs[cidx].base, frame_bytes);
                            g_sensor_frame_w = g_v4l2_preview_w;
                            g_sensor_frame_h = g_v4l2_preview_h;
                            g_last_sensor_frame_ts_ns = dq_ts_ns;
                            g_has_sensor_frame = true;
                            g_sensor_frame_seq++;
                            if (stage_dst) {
                                scale_nv21((const uint8_t*)g_last_sensor_frame, g_v4l2_preview_w, g_v4l2_preview_h,
                                           stage_dst, g_shim_preview_w, g_shim_preview_h);
                                apply_neg_ev_y_plane(stage_dst, (size_t)g_shim_preview_w * g_shim_preview_h);
                                g_cached_stage_ts_ns[idx] = dq_ts_ns;
                                g_last_stage_idx = idx;
                            }
                            pthread_mutex_unlock(&g_sensor_frame_mutex);
                            if (stage_dst && stage_dst != (uint8_t*)g_shim_buffers[idx].base &&
                                g_shim_buffers[idx].base && g_shim_buffers[idx].size >= out_bytes) {
                                memcpy(g_shim_buffers[idx].base, stage_dst, out_bytes);
                            }
                            got_real_frame = true;
                        }
                        if (cidx < (unsigned int)V4L2_CAP_BUF_COUNT && g_v4l2_cap_fd >= 0) {
                            vbuf.m.userptr = (unsigned long)g_v4l2_cap_fd;
                            vbuf.reserved = g_v4l2_cap_bufs[cidx].offset;
                            vbuf.length = g_v4l2_cap_bufs[cidx].size;
                        }
                    } else if (g_v4l2_memory_type == V4L2_MEMORY_MMAP) {
                        if (vbuf.index < g_v4l2_buf_count && g_v4l2_bufs[vbuf.index].start) {
                            int64_t dq_ts_ns = now_monotonic_ns();
                            size_t out_bytes = (size_t)g_shim_preview_w * g_shim_preview_h * 3 / 2;
                            uint8_t* stage_dst = (g_cached_stage_bufs[idx] && g_cached_stage_sizes[idx] >= out_bytes)
                                                 ? g_cached_stage_bufs[idx]
                                                 : (uint8_t*)g_shim_buffers[idx].base;
                            pthread_mutex_lock(&g_sensor_frame_mutex);
                            memcpy(g_last_sensor_frame, g_v4l2_bufs[vbuf.index].start, frame_bytes);
                            g_sensor_frame_w = g_v4l2_preview_w;
                            g_sensor_frame_h = g_v4l2_preview_h;
                            g_last_sensor_frame_ts_ns = dq_ts_ns;
                            g_has_sensor_frame = true;
                            g_sensor_frame_seq++;
                            if (stage_dst) {
                                scale_nv21((const uint8_t*)g_last_sensor_frame, g_v4l2_preview_w, g_v4l2_preview_h,
                                           stage_dst, g_shim_preview_w, g_shim_preview_h);
                                apply_neg_ev_y_plane(stage_dst, (size_t)g_shim_preview_w * g_shim_preview_h);
                                g_cached_stage_ts_ns[idx] = dq_ts_ns;
                                g_last_stage_idx = idx;
                            }
                            pthread_mutex_unlock(&g_sensor_frame_mutex);
                            if (stage_dst && stage_dst != (uint8_t*)g_shim_buffers[idx].base &&
                                g_shim_buffers[idx].base && g_shim_buffers[idx].size >= out_bytes) {
                                memcpy(g_shim_buffers[idx].base, stage_dst, out_bytes);
                            }
                            got_real_frame = true;
                        }
                    }
                    g_orig_ioctl(g_frame_fd, VIDIOC_QBUF, &vbuf);
                    static int s_real_frame_cnt = 0;
                    if (++s_real_frame_cnt <= 3) {
                        const unsigned char *sp = g_last_sensor_frame;
                        size_t y_tot = (size_t)g_sensor_frame_w * g_sensor_frame_h;
                        uint8_t ymin = 255, ymax = 0, uvmin = 255, uvmax = 0;
                        size_t ynz = 0, uvnz = 0;
                        for (size_t k = 0; k < y_tot; k++) {
                            uint8_t v = sp[k];
                            if (v) ynz++;
                            if (v < ymin) ymin = v;
                            if (v > ymax) ymax = v;
                        }
                        for (size_t k = y_tot; k < y_tot + y_tot / 2; k++) {
                            uint8_t v = sp[k];
                            if (v) uvnz++;
                            if (v < uvmin) uvmin = v;
                            if (v > uvmax) uvmax = v;
                        }
                        size_t yc = (size_t)(g_sensor_frame_h / 2) * g_sensor_frame_w + g_sensor_frame_w / 2;
                        size_t uvc = y_tot + (size_t)(g_sensor_frame_h / 4) * g_sensor_frame_w + g_sensor_frame_w / 2;
                        disk_log("REAL SENSOR FRAME #%d (type=%s, idx=%d): top=%02x %02x %02x %02x, Y[min=%u max=%u nz=%zu mid=%02x %02x], UV[min=%u max=%u nz=%zu mid=%02x %02x]",
                                 s_real_frame_cnt, (g_v4l2_memory_type == V4L2_MEMORY_USERPTR ? "USERPTR" : "MMAP"),
                                 idx, sp[0], sp[1], sp[2], sp[3],
                                 ymin, ymax, ynz, sp[yc], sp[yc+1],
                                 uvmin, uvmax, uvnz, sp[uvc], sp[uvc+1]);
                    }
                } else {
                    static int s_dq_err_cnt = 0;
                    if ((++s_dq_err_cnt % 30) == 1) {
                        disk_log("VIDIOC_DQBUF failed on fd=%d: rc=%d, errno=%d (%s)", g_frame_fd, dq_rc, errno, strerror(errno));
                    }
                }
            }

            memset(f, 0, sizeof(*f));
            clock_gettime(CLOCK_MONOTONIC, &f->ts);
            f->path = 1; /* OUTPUT_TYPE_P = BIT(0) = 1 (preview) */
            f->type = 0;
            /* f->buffer MUST point to the registered PMEM buffer g_shim_buffers[idx].base
             * so libcamera's g_orig_preview_thunk matches it and invokes enqueue_buffer(). */
            f->buffer = (unsigned long)g_shim_buffers[idx].base;
            f->phy_offset = g_shim_buffers[idx].offset;
            f->y_off = 0;
            f->cbcr_off = g_shim_preview_w * g_shim_preview_h;
            f->fd = g_shim_buffers[idx].fd;
            f->cropinfo = &g_shim_crop;
            f->croplen = sizeof(g_shim_crop);
            f->frame_id = ++g_frame_seq;

            if (!got_real_frame) {
                size_t out_bytes = (size_t)g_shim_preview_w * g_shim_preview_h * 3 / 2;
                uint8_t* stage_dst = (g_cached_stage_bufs[idx] && g_cached_stage_sizes[idx] >= out_bytes)
                                     ? g_cached_stage_bufs[idx]
                                     : (uint8_t*)g_shim_buffers[idx].base;
                bool copied = false;
                pthread_mutex_lock(&g_sensor_frame_mutex);
                if (g_has_sensor_frame && stage_dst) {
                    scale_nv21((const uint8_t*)g_last_sensor_frame, g_sensor_frame_w, g_sensor_frame_h,
                               stage_dst, g_shim_preview_w, g_shim_preview_h);
                    apply_neg_ev_y_plane(stage_dst, (size_t)g_shim_preview_w * g_shim_preview_h);
                    g_cached_stage_ts_ns[idx] = g_last_sensor_frame_ts_ns;
                    g_last_stage_idx = idx;
                    copied = true;
                }
                pthread_mutex_unlock(&g_sensor_frame_mutex);
                if (copied && stage_dst != (uint8_t*)g_shim_buffers[idx].base &&
                    g_shim_buffers[idx].base && g_shim_buffers[idx].size >= out_bytes) {
                    memcpy(g_shim_buffers[idx].base, stage_dst, out_bytes);
                }
            }
            g_last_frame_done_ns = now_monotonic_ns();

            setup_camera_callbacks();
            static bool s_has_seen_real_frame = false;
            if (got_real_frame) {
                s_has_seen_real_frame = true;
            }

            if (g_frame_seq <= 3) {
                disk_log("ioctl GETFRAME (0x80046d07): idx=%d, seq=%u, real_v4l2=%d", idx, g_frame_seq, got_real_frame ? 1 : 0);
            }
            return 0;
        } else {
            disk_log("ioctl GETFRAME: arg=%p, count=%d -> -EINVAL", f, g_shim_buffer_count);
            return -EINVAL;
        }
    }

    if ((unsigned int)request == 0x40046d0e || (unsigned int)request == 0x40046d08 ||
        (unsigned int)request == 0x40046d0b || (unsigned int)request == 0x40046d0d ||
        (unsigned int)request == 0x40046d0f || (unsigned int)request == 0x00006d17 ||
        (unsigned int)request == 0x40046d12) {
        /* MSM_CAM_IOCTL_RELEASE_FRAME_BUFFER (0x40046d0e)
         * MSM_CAM_IOCTL_ENABLE_VFE (0x40046d08)
         * MSM_CAM_IOCTL_DISABLE_VFE (0x40046d0b)
         * MSM_CAM_IOCTL_VFE_APPS_RESET (0x40046d0d)
         * MSM_CAM_IOCTL_RELEASE_STATS_BUFFER (0x40046d0f)
         * MSM_CAM_IOCTL_UNBLOCK_POLL_FRAME (0x00006d17)
         * MSM_CAM_IOCTL_SET_FD_ROI (0x40046d12) */
        static int s_vfe_misc_log = 0;
        if (++s_vfe_misc_log <= 5) {
            disk_log("ioctl INTERCEPTED VFE enable/disable/buf/reset/roi/unblock (0x%08x) on fd=%d -> 0", (unsigned int)request, fd);
        }
        return 0;
    }

    if ((unsigned int)request == 0x40046d15) { /* MSM_CAM_IOCTL_SENSOR_IO_CFG */
        uint8_t safe_buf[512];
        memset(safe_buf, 0, sizeof(safe_buf));
        if (arg) {
            memcpy(safe_buf, arg, 64);
        }
        uint32_t *u = (uint32_t *)safe_buf;
        disk_log("ioctl SENSOR_IO_CFG (0x40046d15) on fd=%d: cfgtype=%u, mode=%u, rs=%u",
                 fd, u[0], u[1], u[2]);

        if (u[0] == 0) { /* CFG_SET_MODE */
            static bool s_sensor_inited = false;
            if (!s_sensor_inited) {
                uint8_t init_buf[512];
                memset(init_buf, 0, sizeof(init_buf));
                uint32_t *init_u = (uint32_t *)init_buf;
                init_u[0] = 29; /* CFG_SENSOR_INIT */
                init_u[1] = 0;  /* mode = 0 */
                init_u[2] = 1;  /* rs = 1 */
                init_real_libc();
                errno = 0;
                int init_rc = g_orig_ioctl ? g_orig_ioctl(fd, request, init_buf) : -1;
                disk_log("ioctl SENSOR_IO_CFG CFG_SENSOR_INIT(29) on fd=%d -> rc=%d (errno=%d)",
                         fd, init_rc, errno);
                if (init_rc == 0) {
                    s_sensor_inited = true;
                }
            }

            if (u[1] == 1) {
                disk_log("ioctl SENSOR_IO_CFG CFG_SET_MODE: snapshot (mode=1) -> ISX006 RES_CAPTURE (0)");
                u[2] = 0; /* RES_CAPTURE */
            } else {
                disk_log("ioctl SENSOR_IO_CFG CFG_SET_MODE: preview (mode=%u, orig_rs=%u) -> ISX006 RES_PREVIEW (1)", u[1], u[2]);
                u[2] = 1; /* RES_PREVIEW */
            }
        }

        init_real_libc();
        errno = 0;
        int rc = g_orig_ioctl ? g_orig_ioctl(fd, request, safe_buf) : 0;
        disk_log("ioctl SENSOR_IO_CFG (0x40046d15) REAL -> rc=%d (errno=%d)", rc, errno);
        if (arg && rc == 0) {
            memcpy(arg, safe_buf, 64);
        }

        /* Note: sensor streaming is started cleanly by isx006_video_config() */

        /* liboemcamera.so expects EXACTLY 0 on success (it checks rsbs r0, r0, #1).
         * A positive return value from kernel (e.g. rc=2) or negative error will cause
         * isx006_process_start to return 0 and fail with CAMERA_MSG_ERROR.
         * Therefore, always return 0 and clear errno. */
        errno = 0;
        return 0;
    }

    if ((unsigned int)request == 0x40046d04 || (unsigned int)request == 0x40046d18) { /* MSM_CAM_IOCTL_CTRL_COMMAND / CTRL_COMMAND_2 */
        struct gb_ctrl_cmd *cmd = (struct gb_ctrl_cmd *)arg;
        if (cmd) {
            disk_log("ioctl CTRL_COMMAND (0x%08x): type=%u, len=%u, val=%p, timeout=%u, resp_fd=%d",
                     (unsigned int)request, cmd->type, cmd->length, cmd->value, cmd->timeout_ms, cmd->resp_fd);
            LOGI("my_hooked_ioctl: CTRL_COMMAND (0x%08x): type=%u, len=%u, val=%p, timeout=%u, resp_fd=%d",
                 (unsigned int)request, cmd->type, cmd->length, cmd->value, cmd->timeout_ms, cmd->resp_fd);
            if (cmd->value && cmd->length > 0) {
                char hexbuf[128] = {0};
                int print_len = cmd->length > 16 ? 16 : cmd->length;
                for (int i = 0; i < print_len; i++) {
                    sprintf(hexbuf + i*3, "%02x ", ((unsigned char*)cmd->value)[i]);
                }
                disk_log("ioctl CTRL_COMMAND data[%d]: %s", cmd->length, hexbuf);
                LOGI("my_hooked_ioctl: CTRL_COMMAND data[%d]: %s", cmd->length, hexbuf);
            }
            inject_sensor_dimensions();

            pthread_mutex_lock(&g_ctrl_mutex);
            memcpy(&g_pending_ctrl_cmd, cmd, sizeof(*cmd));
            bool is_async_cmd = (cmd->type == 37 || cmd->type == 38 || cmd->type == 39);
            if (is_async_cmd) {
                g_pending_ctrl_cmd.status = 1;
            }
            g_has_pending_ctrl = true;
            g_ctrl_cmd_done = false;
            pthread_cond_signal(&g_ctrl_queue_cond);

            if ((unsigned int)request == 0x40046d04) {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                uint32_t wait_ms = cmd->timeout_ms ? cmd->timeout_ms : 3000;
                if (is_async_cmd) {
                    /* CAMERA_START_PREVIEW (37/39) / CAMERA_STOP_PREVIEW (38) are async in liboemcamera.so.
                     * Wait only briefly (up to 100ms) for config thread to dequeue via GET_STATS */
                    wait_ms = 100;
                } else if (wait_ms > 5000) {
                    wait_ms = 5000;
                }
                ts.tv_sec += wait_ms / 1000;
                ts.tv_nsec += (wait_ms % 1000) * 1000000L;
                if (ts.tv_nsec >= 1000000000L) {
                    ts.tv_sec += 1;
                    ts.tv_nsec -= 1000000000L;
                }

                while (!g_ctrl_cmd_done) {
                    if (is_async_cmd && !g_has_pending_ctrl) {
                        /* Config thread has already dequeued async cmd via GET_STATS */
                        break;
                    }
                    int rc = pthread_cond_timedwait(&g_ctrl_done_cond, &g_ctrl_mutex, &ts);
                    if (rc == ETIMEDOUT) {
                        if (!is_async_cmd) {
                            disk_log("ioctl CTRL_COMMAND: TIMEOUT waiting for config thread on type=%u!", cmd->type);
                            LOGW("my_hooked_ioctl: CTRL_COMMAND timeout on type=%u", cmd->type);
                        } else {
                            disk_log("ioctl CTRL_COMMAND: async type=%u dispatched to config thread", cmd->type);
                        }
                        break;
                    }
                }
                if (is_async_cmd) {
                    cmd->status = 1;
                } else {
                    cmd->status = g_pending_ctrl_cmd.status ? g_pending_ctrl_cmd.status : 1;
                }
                disk_log("ioctl CTRL_COMMAND (0x%08x): completed type=%u with status=%u",
                         (unsigned int)request, cmd->type, cmd->status);
            } else {
                cmd->status = 1;
            }
            pthread_mutex_unlock(&g_ctrl_mutex);
            return 0;
        }
    }

    if ((unsigned int)request == 0x80046d06) { /* MSM_CAM_IOCTL_GET_STATS */
        pthread_mutex_lock(&g_ctrl_mutex);
        if (g_has_pending_ctrl) {
            struct gb_stats_event_ctrl *se = (struct gb_stats_event_ctrl *)arg;
            if (se) {
                memset(se, 0, sizeof(*se));
                se->resptype = 0; /* MSM_CAM_RESP_CTRL */
                se->timeout_ms = g_pending_ctrl_cmd.timeout_ms;
                se->ctrl_cmd = g_pending_ctrl_cmd;
                disk_log("ioctl GET_STATS (0x80046d06): delivered pending ctrl type=%u, len=%u, val=%p, resp_fd=%d",
                         g_pending_ctrl_cmd.type, g_pending_ctrl_cmd.length, g_pending_ctrl_cmd.value, g_pending_ctrl_cmd.resp_fd);
                LOGI("my_hooked_ioctl: GET_STATS (0x80046d06): delivered pending ctrl type=%u", g_pending_ctrl_cmd.type);
            }
            g_has_pending_ctrl = false;
            pthread_mutex_unlock(&g_ctrl_mutex);
            return 0;
        }
        pthread_mutex_unlock(&g_ctrl_mutex);

        disk_log("ioctl GET_STATS (0x80046d06): no pending ctrl command, returning -EAGAIN");
        return -EAGAIN;
    }

    if ((unsigned int)request == 0x40046d09) { /* MSM_CAM_IOCTL_CTRL_CMD_DONE */
        struct gb_ctrl_cmd *cmd = (struct gb_ctrl_cmd *)arg;
        pthread_mutex_lock(&g_ctrl_mutex);
        if (cmd) {
            g_pending_ctrl_cmd.status = cmd->status;
            disk_log("ioctl CTRL_CMD_DONE (0x40046d09): type=%u, status=%u", cmd->type, cmd->status);
            LOGI("my_hooked_ioctl: CTRL_CMD_DONE (0x40046d09): type=%u, status=%u", cmd->type, cmd->status);
        }
        g_ctrl_cmd_done = true;
        pthread_cond_broadcast(&g_ctrl_done_cond);
        pthread_mutex_unlock(&g_ctrl_mutex);
        return 0;
    }

    if ((unsigned int)request == 0x40046d05 || (unsigned int)request == 0x40046d10) { /* MSM_CAM_IOCTL_CONFIG_VFE / AXI_CONFIG */
        int target_fd = (g_config_fd >= 0) ? g_config_fd : fd;
        init_real_libc();
        struct msm_vfe_cfg_cmd *cfg = (struct msm_vfe_cfg_cmd *)arg;
        if (cfg) {
            if ((unsigned int)request == 0x40046d10 && cfg->cmd_type == 36 /* legacy CMD_AXI_CFG_PREVIEW */) {
                disk_log("my_hooked_ioctl: translating legacy CMD_AXI_CFG_PREVIEW(36) -> CMD_AXI_CFG_PRIM(0x100)");
                cfg->cmd_type = 0x100;
                struct msm_isp_cmd *isp = (struct msm_isp_cmd *)cfg->value;
                if (isp && isp->id == 27) {
                    isp->id = 6;
                }
            }
            disk_log("ioctl VFE_CFG (0x%08x) on fd=%d (target_fd=%d): cmd_type=%d, len=%u, val=%p",
                     (unsigned int)request, fd, target_fd, cfg->cmd_type, cfg->length, cfg->value);
            LOGI("my_hooked_ioctl: VFE_CFG (0x%08x) on fd=%d (target=%d): cmd_type=%d, len=%u, val=%p",
                 (unsigned int)request, fd, target_fd, cfg->cmd_type, cfg->length, cfg->value);
        }
        int ret = g_orig_ioctl ? g_orig_ioctl(target_fd, request, arg) : -1;
        disk_log("ioctl VFE_CFG (0x%08x) -> ret=%d (errno=%d)", (unsigned int)request, ret, errno);
        LOGI("my_hooked_ioctl: VFE_CFG (0x%08x) -> ret=%d (errno=%d)", (unsigned int)request, ret, errno);
        return ret;
    }

    init_real_libc();
    if (!g_orig_ioctl) {
        disk_log("ioctl ERROR: g_orig_ioctl is NULL!");
        LOGE("my_hooked_ioctl: g_orig_ioctl is NULL!");
        return -1;
    }

    if ((unsigned int)request != 0x80046d07 && (unsigned int)request != 0x40046d0e && (unsigned int)request != 0x40046d02) {
        disk_log("ioctl CALLING REAL: fd=%d, req=0x%08x", fd, (unsigned int)request);
    }
    int ret = g_orig_ioctl(fd, request, arg);
    if ((unsigned int)request != 0x80046d07 && (unsigned int)request != 0x40046d0e && (unsigned int)request != 0x40046d02) {
        disk_log("ioctl REAL RETURNED: fd=%d, req=0x%08x -> ret=%d (errno=%d)", fd, (unsigned int)request, ret, errno);
    }
    return ret;
}

int ioctl(int fd, int request, ...) {
    void* arg = NULL;
    va_list ap;
    va_start(ap, request);
    arg = va_arg(ap, void*);
    va_end(ap);
    return my_hooked_ioctl(fd, request, arg);
}

static int my_hooked_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    bool is_frame_wait = false;
    int cur_frame_fd = -1;
    if (readfds) {
        if (g_cam_frame_fd >= 0 && FD_ISSET(g_cam_frame_fd, readfds)) {
            is_frame_wait = true;
            cur_frame_fd = g_cam_frame_fd;
        } else if (g_frame_fd >= 0 && FD_ISSET(g_frame_fd, readfds)) {
            is_frame_wait = true;
            cur_frame_fd = g_frame_fd;
        }
    }

    if (is_frame_wait) {
        if (!g_preview_active) {
            usleep(33000);
            FD_ZERO(readfds);
            if (writefds) FD_ZERO(writefds);
            if (exceptfds) FD_ZERO(exceptfds);
            return 0;
        }

        if (!g_v4l2_streaming && g_frame_fd >= 0) {
            start_v4l2_streaming(g_frame_fd, g_shim_preview_w, g_shim_preview_h);
        }

        /* Smart pacing: avoid adding extra delay if the previous frame cycle already consumed >= 25ms. */
        int64_t elapsed_us = (now_monotonic_ns() - g_last_frame_done_ns) / 1000LL;
        if (elapsed_us >= 0 && elapsed_us < 25000) {
            usleep((useconds_t)(25000 - elapsed_us));
        }
        FD_ZERO(readfds);
        FD_SET(cur_frame_fd, readfds);
        if (writefds) FD_ZERO(writefds);
        if (exceptfds) FD_ZERO(exceptfds);
        return 1;
    }

    if (readfds && g_config_fd >= 0 && FD_ISSET(g_config_fd, readfds)) {
        pthread_mutex_lock(&g_ctrl_mutex);
        if (!g_has_pending_ctrl) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            long wait_sec = (timeout && timeout->tv_sec > 0) ? (timeout->tv_sec > 2 ? 2 : timeout->tv_sec) : 1;
            ts.tv_sec += wait_sec;
            pthread_cond_timedwait(&g_ctrl_queue_cond, &g_ctrl_mutex, &ts);
        }

        if (g_has_pending_ctrl) {
            pthread_mutex_unlock(&g_ctrl_mutex);
            disk_log("my_hooked_select: waking up cam_conf on g_config_fd=%d (has_pending_ctrl=true, type=%u)",
                     g_config_fd, g_pending_ctrl_cmd.type);
            LOGI("my_hooked_select: waking up cam_conf on g_config_fd=%d (has_pending_ctrl=true, type=%u)",
                 g_config_fd, g_pending_ctrl_cmd.type);
            FD_ZERO(readfds);
            FD_SET(g_config_fd, readfds);
            if (writefds) FD_ZERO(writefds);
            if (exceptfds) FD_ZERO(exceptfds);
            return 1;
        }
        pthread_mutex_unlock(&g_ctrl_mutex);

        FD_ZERO(readfds);
        if (writefds) FD_ZERO(writefds);
        if (exceptfds) FD_ZERO(exceptfds);
        return 0;
    }

    init_real_libc();
    if (!g_orig_select) {
        LOGE("my_hooked_select: g_orig_select is NULL!");
        return -1;
    }
    return g_orig_select(nfds, readfds, writefds, exceptfds, timeout);
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    return my_hooked_select(nfds, readfds, writefds, exceptfds, timeout);
}

static int my_hooked_open(const char* pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }

    disk_log("open ENTER: path='%s', flags=0x%x", pathname ? pathname : "(null)", flags);
    bool is_config = (pathname && strstr(pathname, "/dev/msm_camera/config"));
    bool is_frame = (pathname && strstr(pathname, "/dev/msm_camera/frame"));
    LOGI("my_hooked_open ENTER: path='%s', flags=0x%x", pathname ? pathname : "(null)", flags);

    init_real_libc();

    /* Crucial: in MSM7x27a kernel 3.4, config0 ioctls require an active camera session (pcam_active).
     * Opening /dev/video0 triggers msm_cam_server_open_session, powers on the ISX006 sensor,
     * creates p_mctl, and links config_cam->p_mctl so that SENSOR_IO_CFG (0x40046d15) succeeds! */
    if (is_config && g_frame_fd < 0) {
        disk_log("my_hooked_open: early opening /dev/video0 to initialize kernel pcam_active/p_mctl");
        g_frame_fd = g_orig_open ? g_orig_open("/dev/video0", O_RDWR | O_NONBLOCK, 0)
                                 : open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
        disk_log("my_hooked_open: early /dev/video0 fd=%d (errno=%d)", g_frame_fd, errno);
    }

    if (pathname && strstr(pathname, "/dev/msm_camera/control")) {
        disk_log("open REDIRECT: %s -> /dev/video100", pathname);
        LOGI("my_hooked_open: redirecting %s -> /dev/video100", pathname);
        pathname = "/dev/video100";
    } else if (is_frame) {
        if (g_frame_fd >= 0) {
            int dup_fd = dup(g_frame_fd);
            g_cam_frame_fd = (dup_fd >= 0) ? dup_fd : g_frame_fd;
            disk_log("open REUSING early /dev/video0: %s -> dup(%d)=%d (g_cam_frame_fd=%d)",
                     pathname, g_frame_fd, dup_fd, g_cam_frame_fd);
            LOGI("my_hooked_open: reusing /dev/video0: %s -> dup(%d)=%d (g_cam_frame_fd=%d)",
                 pathname, g_frame_fd, dup_fd, g_cam_frame_fd);
            return g_cam_frame_fd;
        }
        disk_log("open REDIRECT: %s -> /dev/video0", pathname);
        LOGI("my_hooked_open: redirecting %s -> /dev/video0", pathname);
        pathname = "/dev/video0";
    }

    init_real_libc();
    if (!g_orig_open) {
        disk_log("open ERROR: g_orig_open is NULL!");
        LOGE("my_hooked_open: g_orig_open is NULL!");
        return -1;
    }

    disk_log("open CALLING REAL: path='%s'", pathname ? pathname : "(null)");
    int fd = g_orig_open(pathname, flags, mode);
    disk_log("open REAL RETURNED: path='%s' -> fd=%d (errno=%d)", pathname ? pathname : "(null)", fd, errno);
    LOGI("my_hooked_open EXIT: pathname=%s, flags=0x%x -> fd=%d", pathname ? pathname : "(null)", flags, fd);
    if (fd >= 0) {
        if (is_config) {
            g_config_fd = fd;
            disk_log("my_hooked_open: captured g_config_fd=%d", g_config_fd);
            uint32_t vfe_output_mode = 0x40; /* VFE_OUTPUTS_PREVIEW */
            int rc = g_orig_ioctl ? g_orig_ioctl(g_config_fd, 0x40046d32, &vfe_output_mode) : -1;
            disk_log("my_hooked_open: early MSM_CAM_IOCTL_SET_VFE_OUTPUT_TYPE(0x40) on g_config_fd=%d -> rc=%d (errno=%d)",
                     g_config_fd, rc, errno);
            if (g_frame_fd >= 0 && g_shim_buffer_count > 0) {
                disk_log("my_hooked_open: starting early V4L2 streaming on g_frame_fd=%d", g_frame_fd);
                start_v4l2_streaming(g_frame_fd, g_shim_preview_w, g_shim_preview_h);
            }
        } else if (is_frame || (pathname && strstr(pathname, "video0"))) {
            if (g_frame_fd >= 0 && g_frame_fd != fd) {
                stop_v4l2_streaming();
            }
            g_frame_fd = fd;
            if (is_frame) {
                g_cam_frame_fd = fd;
            }
            disk_log("my_hooked_open: captured g_frame_fd=%d, g_cam_frame_fd=%d", g_frame_fd, g_cam_frame_fd);
        }
    }
    return fd;
}

static bool patch_got_entry(const char* libname, uintptr_t offset, void* new_func) {
    uintptr_t base = get_lib_base(libname);
    if (!base) {
        disk_log("patch_got_entry: %s not found in /proc/self/maps", libname);
        LOGW("patch_got_entry: %s not found in /proc/self/maps", libname);
        return false;
    }
    uintptr_t target_addr = base + offset;
    uintptr_t page_start = target_addr & ~(4096 - 1);
    if (mprotect((void*)page_start, 4096 * 2, PROT_READ | PROT_WRITE) < 0) {
        disk_log("patch_got_entry: mprotect failed on %s (0x%08x): %s", libname, (unsigned int)page_start, strerror(errno));
        LOGE("patch_got_entry: mprotect failed on %s (0x%08x): %s", libname, (unsigned int)page_start, strerror(errno));
        return false;
    }
    disk_log("patch_got_entry: %s + 0x%08x (0x%08x): %p -> %p", libname, (unsigned int)offset, (unsigned int)target_addr, *(void**)target_addr, new_func);
    LOGI("patch_got_entry: %s + 0x%08x (0x%08x): %p -> %p", libname, (unsigned int)offset, (unsigned int)target_addr, *(void**)target_addr, new_func);
    *(void**)target_addr = new_func;
    return true;
}

/* ========================================================================= */
/* Android 4.0 ICS C Camera HAL Types & Functions                            */
/* ========================================================================= */

struct camera_device;

typedef void (*camera_notify_callback)(int32_t msg_type,
        int32_t ext1,
        int32_t ext2,
        void *user);

typedef struct camera_memory {
    void *data;
    size_t size;
    void *handle;
    void (*release)(struct camera_memory *mem);
} camera_memory_t;

typedef void (*camera_data_callback)(int32_t msg_type,
        const camera_memory_t *data, unsigned int index,
        void *metadata,
        void *user);

typedef void (*camera_data_timestamp_callback)(int64_t timestamp,
        int32_t msg_type,
        const camera_memory_t *data, unsigned int index,
        void *user);

typedef camera_memory_t* (*camera_request_memory)(int fd, size_t buf_size,
        unsigned int num_bufs,
        void *user);

struct preview_stream_ops {
    int (*dequeue_buffer)(struct preview_stream_ops* w,
                          buffer_handle_t** buffer, int* stride);
    int (*enqueue_buffer)(struct preview_stream_ops* w,
                          buffer_handle_t* buffer);
    int (*cancel_buffer)(struct preview_stream_ops* w,
                         buffer_handle_t* buffer);
    int (*set_buffer_count)(struct preview_stream_ops* w, int count);
    int (*set_buffers_geometry)(struct preview_stream_ops* pw,
                                int w, int h, int format);
    int (*set_crop)(struct preview_stream_ops *w,
                    int left, int top, int right, int bottom);
    int (*set_usage)(struct preview_stream_ops* w, int usage);
    int (*set_swap_interval)(struct preview_stream_ops *w, int interval);
    int (*get_min_undequeued_buffer_count)(const struct preview_stream_ops *w,
                                           int *count);
    int (*lock_buffer)(struct preview_stream_ops* w,
                       buffer_handle_t* buffer);
};

typedef struct camera_device_ops {
    int (*set_preview_window)(struct camera_device *,
            struct preview_stream_ops *window);
    void (*set_callbacks)(struct camera_device *,
            camera_notify_callback notify_cb,
            camera_data_callback data_cb,
            camera_data_timestamp_callback data_cb_timestamp,
            camera_request_memory get_memory,
            void *user);
    void (*enable_msg_type)(struct camera_device *, int32_t msg_type);
    void (*disable_msg_type)(struct camera_device *, int32_t msg_type);
    int (*msg_type_enabled)(struct camera_device *, int32_t msg_type);
    int (*start_preview)(struct camera_device *);
    void (*stop_preview)(struct camera_device *);
    int (*preview_enabled)(struct camera_device *);
    int (*store_meta_data_in_buffers)(struct camera_device *, int enable);
    int (*start_recording)(struct camera_device *);
    void (*stop_recording)(struct camera_device *);
    int (*recording_enabled)(struct camera_device *);
    void (*release_recording_frame)(struct camera_device *,
                    const void *opaque);
    int (*auto_focus)(struct camera_device *);
    int (*cancel_auto_focus)(struct camera_device *);
    int (*take_picture)(struct camera_device *);
    int (*cancel_picture)(struct camera_device *);
    int (*set_parameters)(struct camera_device *, const char *parms);
    char *(*get_parameters)(struct camera_device *);
    void (*put_parameters)(struct camera_device *, char *);
    int (*send_command)(struct camera_device *,
                int32_t cmd, int32_t arg1, int32_t arg2);
    void (*release)(struct camera_device *);
    int (*dump)(struct camera_device *, int fd);
} camera_device_ops_t;

typedef struct camera_device {
    hw_device_t common;
    camera_device_ops_t *ops;
    void *priv;
} camera_device_t;

typedef int (*camera_device_open_fn_t)(const struct hw_module_t* module, const char* name, struct hw_device_t** device);
typedef int (*close_camera_device_fn_t)(struct hw_device_t* device);
typedef void (*qcom_info_t)(int, struct android::CameraInfo*);
typedef int (*qcom_num_t)();
typedef void* (*qcom_open_raw_t)(int);

static void* g_hal_handle = NULL;
static struct hw_module_t* g_hmi = NULL;
static camera_device_open_fn_t g_cam_open_fn = NULL;
static close_camera_device_fn_t g_cam_close_fn = NULL;
static qcom_open_raw_t g_open_fn = NULL;
static qcom_info_t g_info_fn = NULL;
static qcom_num_t g_num_fn = NULL;

#include <gralloc_priv.h>

/* ========================================================================= */
/* V4L2 Event Dispatcher for MSM Kernel Server (/dev/video100)               */
/* ========================================================================= */

#ifndef V4L2_EVENT_ALL
#define V4L2_EVENT_ALL 0
#endif

struct shim_v4l2_event_subscription {
    uint32_t type;
    uint32_t id;
    uint32_t flags;
    uint32_t reserved[5];
};

struct shim_v4l2_event {
    uint32_t type;
    union {
        uint8_t data[64];
        uint64_t align64;
    } u;
    uint32_t pending;
    uint32_t sequence;
    struct timespec timestamp;
    uint32_t id;
    uint32_t reserved[8];
};

#define SHIM_VIDIOC_DQEVENT _IOR('V', 89, struct shim_v4l2_event)
#define SHIM_VIDIOC_SUBSCRIBE_EVENT _IOW('V', 90, struct shim_v4l2_event_subscription)

#define SHIM_BASE_VIDIOC_PRIVATE 192
#define SHIM_MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE _IOWR('V', SHIM_BASE_VIDIOC_PRIVATE + 4, struct shim_msm_camera_v4l2_ioctl_t *)
#define SHIM_MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD _IOWR('V', SHIM_BASE_VIDIOC_PRIVATE + 5, struct shim_msm_camera_v4l2_ioctl_t *)

struct shim_msm_camera_v4l2_ioctl_t {
    void* ioctl_ptr;
};

struct shim_msm_ctrl_cmd {
    uint16_t type;
    uint16_t length;
    void *value;
    uint16_t status;
    uint32_t timeout_ms;
    int resp_fd;
    int vnode_id;
    int queue_idx;
    uint32_t evt_id;
    uint32_t stream_type;
    int config_ident;
};

struct shim_msm_cam_evt_msg {
    unsigned short type;
    unsigned short msg_id;
    unsigned int len;
    uint32_t frame_id;
    void *data;
    struct timespec timestamp;
};

struct shim_msm_isp_event_ctrl {
    unsigned short resptype;
    union {
        struct shim_msm_cam_evt_msg isp_msg;
        struct shim_msm_ctrl_cmd ctrl;
        uint8_t raw[128];
    } isp_data;
};


static void subscribe_config_events(int conf_fd) {
    if (conf_fd < 0 || !g_orig_ioctl) return;
    struct shim_v4l2_event_subscription sub;
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_ALL;
    int rc = g_orig_ioctl(conf_fd, SHIM_VIDIOC_SUBSCRIBE_EVENT, &sub);
    disk_log("subscribe_config_events(fd=%d): rc=%d (errno=%d)", conf_fd, rc, errno);
}

static void drain_config_events(int conf_fd, const char *tag, int wait_ms) {
    if (conf_fd < 0 || !g_orig_ioctl) return;
    int elapsed = 0;
    int cnt = 0;
    while (elapsed <= wait_ms && cnt < 16) {
        struct pollfd pfd;
        pfd.fd = conf_fd;
        pfd.events = POLLPRI | POLLIN;
        pfd.revents = 0;
        int step = (cnt == 0 && wait_ms > 0) ? 50 : 10;
        int pr = poll(&pfd, 1, step);
        if (pr <= 0) {
            elapsed += step;
            continue;
        }
        if (!(pfd.revents & (POLLPRI | POLLIN))) break;
        struct shim_v4l2_event ev;
        struct shim_msm_isp_event_ctrl u_isp;
        uint8_t dummy_data[128];
        memset(&ev, 0, sizeof(ev));
        memset(&u_isp, 0, sizeof(u_isp));
        memset(dummy_data, 0, sizeof(dummy_data));
        u_isp.isp_data.isp_msg.data = dummy_data;
        *((uint32_t *)ev.u.data) = (uint32_t)&u_isp;
        int rc = g_orig_ioctl(conf_fd, SHIM_VIDIOC_DQEVENT, &ev);
        if (rc < 0) {
            disk_log("drain_config_events[%s]: DQEVENT rc=%d (errno=%d)", tag, rc, errno);
            break;
        }
        cnt++;
        disk_log("drain_config_events[%s] #%d: ev.type=0x%x, resptype=%u, msg.type=%u, msg_id=%u, frame_id=%u, len=%u",
                 tag, cnt, ev.type, u_isp.resptype,
                 u_isp.isp_data.isp_msg.type, u_isp.isp_data.isp_msg.msg_id,
                 u_isp.isp_data.isp_msg.frame_id, u_isp.isp_data.isp_msg.len);
    }
    if (cnt == 0) {
        disk_log("drain_config_events[%s]: 0 events in %dms", tag, wait_ms);
    }
}

static pthread_t g_server_thread = 0;
static int g_server_fd = -1;
static volatile bool g_server_running = false;

static void* server_event_worker(void* arg) {
    int fd = (int)(intptr_t)arg;
    disk_log("server_event_worker started on fd=%d", fd);

    struct shim_v4l2_event_subscription sub;
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_ALL;
    int ret = g_orig_ioctl ? g_orig_ioctl(fd, SHIM_VIDIOC_SUBSCRIBE_EVENT, &sub)
                           : ioctl(fd, SHIM_VIDIOC_SUBSCRIBE_EVENT, &sub);
    disk_log("server_event_worker: subscribed to V4L2_EVENT_ALL ret=%d (errno=%d)", ret, errno);

    while (g_server_running) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLPRI | POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 100);
        if (pr <= 0) {
            continue;
        }

        if (pfd.revents & (POLLPRI | POLLIN)) {
            struct shim_v4l2_event ev;
            memset(&ev, 0, sizeof(ev));
            int rc = g_orig_ioctl ? g_orig_ioctl(fd, SHIM_VIDIOC_DQEVENT, &ev)
                                  : ioctl(fd, SHIM_VIDIOC_DQEVENT, &ev);
            if (rc < 0) {
                if (errno != EINTR && errno != EAGAIN) {
                    disk_log("server_event_worker: DQEVENT failed: %s (%d)", strerror(errno), errno);
                }
                continue;
            }

            int q_idx = ev.u.data[0];
            disk_log("server_event_worker: GOT EVENT type=0x%x, q_idx=%d", ev.type, q_idx);

            struct shim_msm_isp_event_ctrl u_isp_event;
            memset(&u_isp_event, 0, sizeof(u_isp_event));
            u_isp_event.isp_data.ctrl.queue_idx = q_idx;

            struct shim_msm_camera_v4l2_ioctl_t v4l2_ioctl;
            v4l2_ioctl.ioctl_ptr = &u_isp_event;

            rc = g_orig_ioctl ? g_orig_ioctl(fd, SHIM_MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD, &v4l2_ioctl)
                              : ioctl(fd, SHIM_MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD, &v4l2_ioctl);
            disk_log("server_event_worker: GET_EVENT_PAYLOAD ret=%d, ctrl.type=%d, evt_id=%d",
                     rc, u_isp_event.isp_data.ctrl.type, u_isp_event.isp_data.ctrl.evt_id);

            struct shim_msm_ctrl_cmd cmd;
            memset(&cmd, 0, sizeof(cmd));
            cmd.type = u_isp_event.isp_data.ctrl.type;
            cmd.queue_idx = q_idx;
            cmd.evt_id = u_isp_event.isp_data.ctrl.evt_id;
            cmd.status = 1; /* CAMERA_STATUS_SUCCESS */

            v4l2_ioctl.ioctl_ptr = &cmd;
            rc = g_orig_ioctl ? g_orig_ioctl(fd, SHIM_MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &v4l2_ioctl)
                              : ioctl(fd, SHIM_MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE, &v4l2_ioctl);
            disk_log("server_event_worker: CTRL_CMD_DONE ret=%d (for type=%d, evt_id=%d)",
                     rc, cmd.type, cmd.evt_id);
        }
    }
    disk_log("server_event_worker exiting");
    return NULL;
}

static void start_server_thread_if_needed() {
    if (g_server_running) return;
    init_real_libc();

    g_server_fd = g_orig_open ? g_orig_open("/dev/video100", O_RDWR) : open("/dev/video100", O_RDWR);
    if (g_server_fd < 0) {
        disk_log("start_server_thread: failed to open /dev/video100: %s", strerror(errno));
        return;
    }
    disk_log("start_server_thread: opened /dev/video100 fd=%d", g_server_fd);
    g_server_running = true;
    if (pthread_create(&g_server_thread, NULL, server_event_worker, (void*)(intptr_t)g_server_fd) != 0) {
        disk_log("start_server_thread: pthread_create failed: %s", strerror(errno));
        g_server_running = false;
        close(g_server_fd);
        g_server_fd = -1;
    }
}

/* ========================================================================= */
/* Camera Frame Delivery and Notification Callbacks Hook                     */
/* ========================================================================= */

static void (*g_orig_preview_thunk)(void*) = NULL;
static void (*g_orig_recording_thunk)(void*) = NULL;
static void (*g_orig_error_thunk)(void*) = NULL;
static void (*g_orig_stats_thunk)(void*) = NULL;
static void (*g_orig_event_thunk)(void*) = NULL;

namespace android {
    class QcomCameraAdapter;
}
static android::QcomCameraAdapter* g_active_adapter = NULL;
static int g_cb_frame_count = 0;
static int g_enqueue_count = 0;
static void adapter_deliver_frame(void* frame);

static void dump_hex(const char* tag, const void* ptr, int len) {
    if (!ptr) {
        disk_log("%s: NULL", tag);
        return;
    }
    const uint8_t* b = (const uint8_t*)ptr;
    char hex[64];
    int n = len > 16 ? 16 : len;
    for (int i = 0; i < n; i++) sprintf(hex + i*3, "%02x ", b[i]);
    disk_log("%s @ %p: %s", tag, ptr, hex);
}

static void my_preview_frame_cb(void* frame) {
    g_cb_frame_count++;
    if (g_cb_frame_count <= 5 || (g_cb_frame_count % 30) == 0) {
        disk_log("my_preview_frame_cb #%d CALLED! frame=%p", g_cb_frame_count, frame);
        dump_hex("frame struct", frame, 32);
        if (frame) {
            uint32_t* u = (uint32_t*)frame;
            for (int i = 0; i < 8; i++) {
                disk_log("  frame[%d] = 0x%08x (%u)", i, u[i], u[i]);
            }
            if (u[4] != 0) {
                dump_hex("frame[4] sensor pixels", (void*)u[4], 32);
            }
        }
    }

    if (g_orig_preview_thunk) {
        g_orig_preview_thunk(frame);
    }

    adapter_deliver_frame(frame);
}

static void setup_camera_callbacks() {
    uintptr_t cam_base = get_lib_base("camera.msm7x27a.so");
    uintptr_t oem_base = get_lib_base("liboemcamera.so");

    if (!cam_base || !oem_base) {
        return;
    }

    void* p_preview_thunk   = (void*)(cam_base + 0x15ec5);
    void* p_recording_thunk = (void*)(cam_base + 0x15db1);
    void* p_error_thunk     = (void*)(cam_base + 0x15dc5);
    void* p_stats_thunk     = (void*)(cam_base + 0x15ead);
    void* p_event_thunk     = (void*)(cam_base + 0x15e15);

    g_orig_preview_thunk   = (void (*)(void*))p_preview_thunk;
    g_orig_recording_thunk = (void (*)(void*))p_recording_thunk;
    g_orig_error_thunk     = (void (*)(void*))p_error_thunk;
    g_orig_stats_thunk     = (void (*)(void*))p_stats_thunk;
    g_orig_event_thunk     = (void (*)(void*))p_event_thunk;

    /* Note: p_hw[52] in QualcommCameraHardware is mInitialPreviewRunning.
     * Do NOT set it to 1, otherwise setPreviewWindow triggers an extra
     * getBuffersAndStartPreviewEv(), causing startPreview to deadlock in pthread_cond_wait! */

    // 1. Populate camera.msm7x27a.so mCamNotify
    void** p_cam_notify = NULL;
    if (g_hal_handle) {
        p_cam_notify = (void**)dlsym(g_hal_handle, "_ZN7android10mCamNotifyE");
    }
    if (!p_cam_notify) {
        p_cam_notify = (void**)(cam_base + 0x21f48);
    }
    if (p_cam_notify) {
        p_cam_notify[0] = p_event_thunk;
        p_cam_notify[1] = (void*)&my_preview_frame_cb;
        p_cam_notify[2] = (void*)&my_preview_frame_cb;
        p_cam_notify[3] = p_error_thunk;
        p_cam_notify[4] = p_stats_thunk;
    }

    // 2. Populate liboemcamera.so notify pointer at oem_base + 0x9c7c0
    void*** pp_oem_ctrl = (void***)(oem_base + 0x9c7c0);
    void** p_oem_notify = *pp_oem_ctrl;
    if (!p_oem_notify && p_cam_notify) {
        *pp_oem_ctrl = p_cam_notify;
        p_oem_notify = p_cam_notify;
    }
    if (p_oem_notify) {
        p_oem_notify[0] = p_event_thunk;
        p_oem_notify[1] = (void*)&my_preview_frame_cb;
        p_oem_notify[2] = (void*)&my_preview_frame_cb;
        p_oem_notify[3] = p_error_thunk;
        p_oem_notify[4] = p_stats_thunk;
    }

    static bool s_logged = false;
    if (!s_logged) {
        disk_log("setup_camera_callbacks: SUCCESS! cam_base=0x%08x, oem_base=0x%08x, p_cam_notify=%p, p_oem_notify=%p [1]=%p",
                 (unsigned int)cam_base, (unsigned int)oem_base, p_cam_notify, p_oem_notify,
                 p_oem_notify ? p_oem_notify[1] : NULL);
        s_logged = true;
    }
}

static void ensure_hal_loaded() {
    if (g_hal_handle) return;

    disk_log("ensure_hal_loaded ENTER");
    /* Initialize real libc function pointers before patching any GOT entries */
    init_real_libc();

    /* Ensure device nodes exist and have proper permissions */
    mkdir("/dev/msm_camera", 0775);
    unlink("/dev/msm_camera/control0");
    symlink("/dev/video100", "/dev/msm_camera/control0");
    unlink("/dev/msm_camera/frame0");
    symlink("/dev/video0", "/dev/msm_camera/frame0");
    chmod("/dev/msm_camera/control0", 0666);
    chmod("/dev/msm_camera/frame0", 0666);
    chmod("/dev/msm_camera/config0", 0666);
    chmod("/dev/video0", 0666);
    chmod("/dev/video1", 0666);
    chmod("/dev/video100", 0666);
    chmod("/dev/pmem", 0666);
    chmod("/dev/pmem_adsp", 0666);

    start_server_thread_if_needed();

    disk_log("Loading /system/lib/hw/camera.msm7x27a.so ...");
    LOGI("Loading /system/lib/hw/camera.msm7x27a.so ...");
    g_hal_handle = dlopen("/system/lib/hw/camera.msm7x27a.so", RTLD_NOW | RTLD_GLOBAL);
    if (!g_hal_handle) {
        disk_log("Failed to dlopen camera.msm7x27a.so: %s", dlerror());
        LOGE("Failed to dlopen /system/lib/hw/camera.msm7x27a.so: %s", dlerror());
        return;
    }

    disk_log("Pre-loading liboemcamera.so ...");
    LOGI("Pre-loading liboemcamera.so ...");
    void* oem = dlopen("liboemcamera.so", RTLD_NOW | RTLD_GLOBAL);
    if (!oem) {
        disk_log("Failed to pre-dlopen liboemcamera.so: %s", dlerror());
        LOGW("Failed to pre-dlopen liboemcamera.so: %s", dlerror());
    }

    /* Install GOT hooks for camera.msm7x27a.so and liboemcamera.so */
    patch_got_entry("camera.msm7x27a.so", 0x21b28, (void*)&my_hooked_ioctl);
    patch_got_entry("camera.msm7x27a.so", 0x21bc0, (void*)&my_hooked_open);
    patch_got_entry("liboemcamera.so", 0x864e0, (void*)&my_hooked_ioctl);
    patch_got_entry("liboemcamera.so", 0x864fc, (void*)&my_hooked_open);
    patch_got_entry("liboemcamera.so", 0x86540, (void*)&my_hooked_select);

    g_hmi = (struct hw_module_t*)dlsym(g_hal_handle, "HMI");
    g_cam_open_fn = (camera_device_open_fn_t)dlsym(g_hal_handle, "camera_device_open");
    g_cam_close_fn = (close_camera_device_fn_t)dlsym(g_hal_handle, "close_camera_device");
    g_open_fn = (qcom_open_raw_t)dlsym(g_hal_handle, "HAL_openCameraHardware");
    g_info_fn = (qcom_info_t)dlsym(g_hal_handle, "HAL_getCameraInfo");
    g_num_fn = (qcom_num_t)dlsym(g_hal_handle, "HAL_getNumberOfCameras");
    disk_log("Qualcomm HAL loaded! HMI=%p, cam_open=%p, cam_close=%p, raw_open=%p, info=%p, num=%p",
             g_hmi, g_cam_open_fn, g_cam_close_fn, g_open_fn, g_info_fn, g_num_fn);
    LOGI("Qualcomm HAL loaded! HMI=%p, cam_open=%p, cam_close=%p, raw_open=%p, info=%p, num=%p",
         g_hmi, g_cam_open_fn, g_cam_close_fn, g_open_fn, g_info_fn, g_num_fn);
    inject_sensor_dimensions();
    setup_camera_callbacks();
}

} /* extern "C" */

/* FIH MSM7x27a/ISX006 kernel control ABI (matching kernel headers) */
#define K_MSM_CAM_IOCTL_SENSOR_IO_CFG    0x40046d15U
#define K_MSM_CAM_IOCTL_FLASH_CTRL       0x40046d20U
#define K_MSM_CAM_IOCTL_SENSOR_V4L2_S_CTRL 0x80146d2fU

#define K_V4L2_CID_EXPOSURE              0x00980911U
#define K_V4L2_CID_WB_TEMP               0x0098091aU
#define K_V4L2_CID_BESTSHOT_MODE         0x0098092cU
#define K_V4L2_CID_METER_MODE            0x0098092dU
#define K_V4L2_CID_FOCUS_ROI             0x0098092eU
#define K_V4L2_CID_AF                    0x0098092fU
#define K_V4L2_CID_AF_MODE               0x00980930U

#define K_CFG_GET_FLASH_STATE            47
#define K_CFG_GET_RC_AF_CHECK            48

enum {
    K_LED_MODE_OFF = 0,
    K_LED_MODE_AUTO = 1,
    K_LED_MODE_ON = 2,
    K_LED_MODE_TORCH = 3,
    K_LED_MODE_RED_EYE = 4,
};

enum {
    K_LED_STATE_OFF = 0,
    K_LED_STATE_LOW = 1,
    K_LED_STATE_HIGH = 2,
    K_LED_STATE_INIT = 3,
    K_LED_STATE_RELEASE = 4,
};

enum {
    K_AF_ON = 0,
    K_AF_MACRO = 1,
    K_AF_SAF = 2,
    K_AF_CAF = 3,
    K_AF_INFINITY = 4,
    K_AF_OFF = 5,
    K_AF_POLLING = 6,
    K_AF_GET_STATE = 7,
};

struct kernel_v4l2_control {
    uint32_t id;
    int32_t value;
    uint16_t focus_roi[6];
};

struct kernel_flash_ctrl_data {
    int flashtype;
    int ledmode;
    union {
        int led_state;
        int raw[2];
    } ctrl_data;
};

struct kernel_sensor_cfg_data {
    int cfgtype;
    int mode;
    int rs;
    uint8_t max_steps;
    uint8_t pad[3];
    union {
        int8_t flash_state;
        int8_t rc_af_check;
        uint8_t raw[20];
    } cfg;
};

static pthread_mutex_t g_kctrl_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_flash_init_fd = -1;

static int kernel_v4l2_s_ctrl(uint32_t id, int32_t value, const uint16_t* roi6) {
    if (g_config_fd < 0 || !g_orig_ioctl) return -1;
    struct kernel_v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = id;
    ctrl.value = value;
    if (roi6) {
        memcpy(ctrl.focus_roi, roi6, sizeof(ctrl.focus_roi));
    }
    pthread_mutex_lock(&g_kctrl_mutex);
    int fd = g_config_fd;
    errno = 0;
    int rc = (fd >= 0) ? g_orig_ioctl(fd, K_MSM_CAM_IOCTL_SENSOR_V4L2_S_CTRL, &ctrl) : -1;
    int err = errno;
    pthread_mutex_unlock(&g_kctrl_mutex);
    if (id != K_V4L2_CID_AF_MODE || value != K_AF_POLLING) {
        disk_log("kernel_v4l2_s_ctrl(id=0x%08x, val=%d) fd=%d -> rc=%d (errno=%d)",
                 id, value, fd, rc, err);
    }
    return rc;
}

static int kernel_flash_ctrl_raw(int ledmode, int led_state) {
    if (g_config_fd < 0 || !g_orig_ioctl) return -1;
    struct kernel_flash_ctrl_data fi;
    memset(&fi, 0, sizeof(fi));
    fi.flashtype = 0; /* MSM_CAMERA_LED_FLASH */
    fi.ledmode = ledmode;
    fi.ctrl_data.led_state = led_state;
    errno = 0;
    int rc = g_orig_ioctl(g_config_fd, K_MSM_CAM_IOCTL_FLASH_CTRL, &fi);
    int err = errno;
    disk_log("kernel_flash_ctrl(mode=%d, state=%d) fd=%d -> rc=%d (errno=%d)",
             ledmode, led_state, g_config_fd, rc, err);
    return rc;
}

static int kernel_flash_ctrl(int ledmode, int led_state) {
    pthread_mutex_lock(&g_kctrl_mutex);
    int rc = -1;
    if (g_config_fd >= 0 && g_orig_ioctl) {
        if (g_flash_init_fd != g_config_fd && led_state != K_LED_STATE_INIT && led_state != K_LED_STATE_RELEASE) {
            kernel_flash_ctrl_raw(ledmode, K_LED_STATE_INIT);
            g_flash_init_fd = g_config_fd;
        }
        rc = kernel_flash_ctrl_raw(ledmode, led_state);
        if (led_state == K_LED_STATE_RELEASE) {
            g_flash_init_fd = -1;
        }
    }
    pthread_mutex_unlock(&g_kctrl_mutex);
    return rc;
}

static int kernel_cfg_get_i8(int cfgtype, int8_t* out_val) {
    if (g_config_fd < 0 || !g_orig_ioctl || !out_val) return -1;
    struct kernel_sensor_cfg_data cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.cfgtype = cfgtype;
    pthread_mutex_lock(&g_kctrl_mutex);
    errno = 0;
    int rc = (g_config_fd >= 0) ? g_orig_ioctl(g_config_fd, K_MSM_CAM_IOCTL_SENSOR_IO_CFG, &cfg) : -1;
    pthread_mutex_unlock(&g_kctrl_mutex);
    *out_val = (cfgtype == K_CFG_GET_FLASH_STATE) ? cfg.cfg.flash_state : cfg.cfg.rc_af_check;
    return rc;
}

struct StrIntMap {
    const char* key;
    int val;
};

static int map_lookup(const StrIntMap* tbl, size_t n, const char* s, int def_val) {
    if (!s) return def_val;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(tbl[i].key, s) == 0) return tbl[i].val;
    }
    return def_val;
}

static const StrIntMap kWbMap[] = {
    { "auto", 1 },
    { "incandescent", 3 },
    { "fluorescent", 4 },
    { "daylight", 5 },
    { "cloudy-daylight", 6 },
};

static const StrIntMap kSceneMap[] = {
    { "auto", 1 },
    { "landscape", 2 },
    { "snow", 3 },
    { "beach", 4 },
    { "night", 6 },
    { "portrait", 7 },
    { "sports", 9 },
    { "night-portrait", 15 },
    { "document", 19 },
};

static const StrIntMap kFlashMap[] = {
    { "off", K_LED_MODE_OFF },
    { "auto", K_LED_MODE_AUTO },
    { "on", K_LED_MODE_ON },
    { "torch", K_LED_MODE_TORCH },
    { "red-eye", K_LED_MODE_RED_EYE },
};

static const StrIntMap kMeterMap[] = {
    { "frame-average", 0 },
    { "center-weighted", 1 },
    { "spot-metering", 2 },
};

namespace android {

class QcomCameraAdapter : public CameraHardwareInterface {
private:
    camera_device_t* m_dev;

    notify_callback m_notify_cb;
    data_callback m_data_cb;
    data_callback_timestamp m_data_cb_ts;
    void* m_user;

    struct shim_buffer_t {
        private_handle_t* handle;
        int pmem_fd;
        void* base;
        size_t size;
        size_t offset;
        sp<IMemory> mem;
        bool queued;
    };
    static const int MAX_BUFFERS = 8;
    shim_buffer_t m_buffers[MAX_BUFFERS];
    int m_buffer_count;
    int m_preview_w;
    int m_preview_h;
    int m_preview_format;
    sp<MemoryHeapBase> m_preview_heap;
    int m_master_pmem_fd;
    void* m_master_base;
    int m_last_hal_buf_idx; // Last buffer index used by HAL (via s_get_memory)
    size_t m_master_size;
    int m_deq_cursor;

    struct RecSlot {
        sp<MemoryHeapBase> heap;
        sp<MemoryBase> mem;
        void* base;
        size_t size;
        bool in_use;
    };
    static const int MAX_REC_SLOTS = 8;
    RecSlot m_rec_slots[MAX_REC_SLOTS];
    pthread_mutex_t m_rec_lock;
    volatile bool m_recording;

    struct shim_stream_ops_t {
        struct preview_stream_ops ops;
        QcomCameraAdapter* adapter;
    };
    shim_stream_ops_t m_stream_ops;

    static void s_notify_cb(int32_t msg_type, int32_t ext1, int32_t ext2, void *user) {
        disk_log("s_notify_cb: msg_type=0x%x, ext1=%d, ext2=%d", msg_type, ext1, ext2);
        if (g_start_preview_in_progress) {
            disk_log("s_notify_cb: dropped during startPreview to prevent deadlock");
            return;
        }
        if (msg_type == 0x1) {
            disk_log("s_notify_cb: SUPPRESSED CAMERA_MSG_ERROR (0x1) ext1=%d ext2=%d", ext1, ext2);
            return;
        }
        QcomCameraAdapter* self = (QcomCameraAdapter*)user;
        if (self && self->m_notify_cb) {
            self->m_notify_cb(msg_type, ext1, ext2, self->m_user);
        }
    }

    static void s_data_cb(int32_t msg_type, const camera_memory_t *data, unsigned int index,
                          void *metadata, void *user) {
        if (g_start_preview_in_progress) {
            return;
        }
        if (msg_type == CAMERA_MSG_PREVIEW_FRAME) {
            // Preview frames are handled exclusively by preview_stream_ops::enqueue_buffer
            return;
        }
        static int s_data_count = 0;
        s_data_count++;
        if ((s_data_count % 30) == 1) {
            disk_log("s_data_cb #%d: msg_type=0x%x, index=%d", s_data_count, msg_type, index);
        }
        QcomCameraAdapter* self = (QcomCameraAdapter*)user;
        if (!self || !self->m_data_cb) return;

        sp<MemoryHeapBase> heap;
        sp<MemoryBase> mem;
        if (data && data->data && data->size > 0) {
            heap = new MemoryHeapBase(data->size, 0, "cam_data");
            memcpy(heap->getBase(), data->data, data->size);
            mem = new MemoryBase(heap, 0, data->size);
        }
        self->m_data_cb(msg_type, mem, self->m_user);
    }

    static void s_data_cb_ts(int64_t timestamp, int32_t msg_type,
                             const camera_memory_t *data, unsigned int index, void *user) {
        if (g_start_preview_in_progress) {
            return;
        }
        disk_log("s_data_cb_ts: ts=%lld, msg_type=0x%x", (long long)timestamp, msg_type);
        QcomCameraAdapter* self = (QcomCameraAdapter*)user;
        if (!self || !self->m_data_cb_ts) return;

        sp<MemoryHeapBase> heap;
        sp<MemoryBase> mem;
        if (data && data->data && data->size > 0) {
            heap = new MemoryHeapBase(data->size, 0, "cam_data_ts");
            memcpy(heap->getBase(), data->data, data->size);
            mem = new MemoryBase(heap, 0, data->size);
        }
        self->m_data_cb_ts(timestamp, msg_type, mem, self->m_user);
    }

    struct shim_cam_mem {
        camera_memory_t mem;
        void* mapped;
        size_t size;
        bool is_mmap;
        bool is_pmem_sub;
    };

    static void s_mem_release(camera_memory_t* mem) {
        if (!mem) return;
        shim_cam_mem* m = (shim_cam_mem*)mem->handle;
        if (m) {
            if (!m->is_pmem_sub) {
                if (m->is_mmap && m->mapped) {
                    munmap(m->mapped, m->size);
                } else if (!m->is_mmap && m->mapped) {
                    free(m->mapped);
                }
            }
            free(m);
        }
    }

    static camera_memory_t* s_get_memory(int fd, size_t buf_size, unsigned int num_bufs, void *user) {
        static int s_get_mem_count = 0;
        s_get_mem_count++;
        if (s_get_mem_count <= 3) {
            disk_log("s_get_memory #%d: fd=%d, buf_size=%zu, num_bufs=%u, user=%p", s_get_mem_count, fd, buf_size, num_bufs, user);
        }
        QcomCameraAdapter* self = (QcomCameraAdapter*)user;

        shim_cam_mem* m = (shim_cam_mem*)calloc(1, sizeof(shim_cam_mem));
        if (!m) return NULL;

        if (!self) self = g_active_adapter;
        if (self && self->m_master_pmem_fd >= 0 && fd == self->m_master_pmem_fd) {
            int matched_idx = -1;
            for (int i = 0; i < self->m_buffer_count; i++) {
                if (self->m_buffers[i].offset == buf_size) {
                    matched_idx = i;
                    break;
                }
            }
            if (matched_idx < 0) {
                static int s_pmem_seq = 0;
                matched_idx = (s_pmem_seq++) % (self->m_buffer_count > 0 ? self->m_buffer_count : 1);
            }

            self->m_last_hal_buf_idx = matched_idx;
            m->mapped = self->m_buffers[matched_idx].base;
            m->size = self->m_buffers[matched_idx].size;
            m->is_mmap = false;
            m->is_pmem_sub = true;
            m->mem.data = m->mapped;
            m->mem.size = m->size;
            m->mem.handle = m;
            m->mem.release = s_mem_release;
            if (s_get_mem_count <= 3) {
                disk_log("s_get_memory #%d: matched PMEM sub-buffer %d (offset=%zu) -> data=%p, size=%zu",
                         s_get_mem_count, matched_idx, self->m_buffers[matched_idx].offset, m->mem.data, m->mem.size);
            }
            return &m->mem;
        }

        if (buf_size == 0) {
            buf_size = (self && self->m_preview_w > 0 && self->m_preview_h > 0)
                     ? (self->m_preview_w * self->m_preview_h * 3 / 2)
                     : (640 * 480 * 3 / 2);
            buf_size = (buf_size + 4095) & ~4095;
            if (s_get_mem_count <= 5 || (s_get_mem_count % 30) == 0) {
                disk_log("s_get_memory #%d: corrected buf_size to %zu", s_get_mem_count, buf_size);
            }
        }
        size_t total = buf_size * (num_bufs > 0 ? num_bufs : 1);
        m->size = total;
        m->is_pmem_sub = false;

        if (fd >= 0) {
            m->mapped = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (m->mapped == MAP_FAILED) {
                disk_log("s_get_memory: mmap failed on fd %d (size=%zu): %s, using malloc fallback",
                         fd, total, strerror(errno));
                m->mapped = malloc(total);
                m->is_mmap = false;
                m->mem.data = m->mapped;
            } else {
                m->is_mmap = true;
                m->mem.data = m->mapped;
            }
        } else {
            m->mapped = malloc(total);
            m->is_mmap = false;
            m->mem.data = m->mapped;
        }
        m->mem.size = total;
        m->mem.handle = m;
        m->mem.release = s_mem_release;
        if (s_get_mem_count <= 5 || (s_get_mem_count % 30) == 0) {
            disk_log("s_get_memory #%d: allocated mem=%p, data=%p, size=%zu", s_get_mem_count, m, m->mem.data, total);
        }
        return &m->mem;
    }

    // preview_stream_ops trampolines
    static int s_dequeue_buffer(struct preview_stream_ops* w, buffer_handle_t** buffer, int* stride) {
        shim_stream_ops_t* shim = (shim_stream_ops_t*)w;
        return shim->adapter->handle_dequeue_buffer(buffer, stride);
    }

    static int s_enqueue_buffer(struct preview_stream_ops* w, buffer_handle_t* buffer) {
        shim_stream_ops_t* shim = (shim_stream_ops_t*)w;
        return shim->adapter->handle_enqueue_buffer(buffer);
    }

    static int s_cancel_buffer(struct preview_stream_ops* w, buffer_handle_t* buffer) {
        shim_stream_ops_t* shim = (shim_stream_ops_t*)w;
        return shim->adapter->handle_cancel_buffer(buffer);
    }

    static int s_set_buffer_count(struct preview_stream_ops* w, int count) {
        shim_stream_ops_t* shim = (shim_stream_ops_t*)w;
        return shim->adapter->handle_set_buffer_count(count);
    }

    static int s_set_buffers_geometry(struct preview_stream_ops* pw, int w, int h, int format) {
        shim_stream_ops_t* shim = (shim_stream_ops_t*)pw;
        return shim->adapter->handle_set_buffers_geometry(w, h, format);
    }

    static int s_set_crop(struct preview_stream_ops *w, int left, int top, int right, int bottom) {
        return 0;
    }

    static int s_set_usage(struct preview_stream_ops* w, int usage) {
        return 0;
    }

    static int s_set_swap_interval(struct preview_stream_ops *w, int interval) {
        return 0;
    }

    static int s_get_min_undequeued_buffer_count(const struct preview_stream_ops *w, int *count) {
        if (count) *count = 1;
        return 0;
    }

    static int s_lock_buffer(struct preview_stream_ops* w, buffer_handle_t* buffer) {
        return 0;
    }

    int handle_set_buffers_geometry(int w, int h, int format) {
        disk_log("preview_stream_ops::set_buffers_geometry: %dx%d, format=0x%x", w, h, format);
        m_preview_w = w;
        m_preview_h = h;
        m_preview_format = format;
        g_shim_preview_w = w;
        g_shim_preview_h = h;
        g_shim_crop.in_w = w;
        g_shim_crop.out_w = w;
        g_shim_crop.in_h = h;
        g_shim_crop.out_h = h;
        return 0;
    }

    void cleanup_buffers() {
        g_shim_buffer_count = 0;
        for (int i = 0; i < 8; i++) {
            if (g_cached_stage_bufs[i]) {
                free(g_cached_stage_bufs[i]);
                g_cached_stage_bufs[i] = NULL;
                g_cached_stage_sizes[i] = 0;
            }
        }
        m_preview_heap.clear();
        for (int i = 0; i < MAX_BUFFERS; i++) {
            m_buffers[i].mem.clear();
            if (m_buffers[i].handle) {
                delete m_buffers[i].handle;
                m_buffers[i].handle = NULL;
            }
            m_buffers[i].base = NULL;
            m_buffers[i].size = 0;
            m_buffers[i].offset = 0;
            m_buffers[i].pmem_fd = -1;
            m_buffers[i].queued = false;
        }
        if (m_master_base && m_master_size > 0) {
            munmap(m_master_base, m_master_size);
            m_master_base = NULL;
            m_master_size = 0;
        }
        if (m_master_pmem_fd >= 0) {
            close(m_master_pmem_fd);
            m_master_pmem_fd = -1;
        }
    }

    int handle_set_buffer_count(int count) {
        disk_log("preview_stream_ops::set_buffer_count: count=%d", count);
        if (count > MAX_BUFFERS) count = MAX_BUFFERS;
        m_buffer_count = count;

        stop_v4l2_streaming();
        cleanup_buffers();

        size_t max_w = (m_preview_w > 640) ? m_preview_w : 640;
        size_t max_h = (m_preview_h > 480) ? m_preview_h : 480;
        size_t frame_size = (max_w * max_h * 3 / 2 + 4095) & ~4095;
        frame_size = (frame_size + 4095) & ~4095;
        size_t total_size = frame_size * count;

        m_master_pmem_fd = open("/dev/pmem_adsp", O_RDWR | O_SYNC);
        if (m_master_pmem_fd < 0) {
            m_master_pmem_fd = open("/dev/pmem", O_RDWR | O_SYNC);
        }
        if (m_master_pmem_fd < 0) {
            disk_log("Failed to open /dev/pmem_adsp and /dev/pmem: %s", strerror(errno));
            return -ENOMEM;
        }

        m_master_base = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_master_pmem_fd, 0);
        if (m_master_base == MAP_FAILED) {
            disk_log("mmap failed on master pmem fd %d (size=%zu): %s", m_master_pmem_fd, total_size, strerror(errno));
            close(m_master_pmem_fd);
            m_master_pmem_fd = -1;
            m_master_base = NULL;
            return -ENOMEM;
        }
        m_master_size = total_size;

        m_preview_heap = new MemoryHeapBase(m_master_pmem_fd, total_size, 0, 0);
        disk_log("Allocated master PMEM fd=%d, total_size=%zu, base=%p, heap=%p",
                 m_master_pmem_fd, total_size, m_master_base, m_preview_heap.get());

        for (int i = 0; i < count; i++) {
            private_handle_t* hnd = new private_handle_t(
                m_master_pmem_fd, frame_size,
                private_handle_t::PRIV_FLAGS_USES_PMEM | private_handle_t::PRIV_FLAGS_USES_PMEM_ADSP
            );
            hnd->offset = i * frame_size;
            hnd->base = (int)((char*)m_master_base + i * frame_size);

            m_buffers[i].handle = hnd;
            m_buffers[i].pmem_fd = m_master_pmem_fd;
            m_buffers[i].base = (char*)m_master_base + i * frame_size;
            m_buffers[i].size = frame_size;
            m_buffers[i].offset = i * frame_size;
            m_buffers[i].mem = new MemoryBase(m_preview_heap, i * frame_size, frame_size);
            m_buffers[i].queued = true;

            g_shim_buffers[i].base = m_buffers[i].base;
            g_shim_buffers[i].fd = m_buffers[i].pmem_fd;
            g_shim_buffers[i].size = m_buffers[i].size;
            g_shim_buffers[i].offset = m_buffers[i].offset;
            g_cached_stage_bufs[i] = (uint8_t*)malloc(m_buffers[i].size);
            g_cached_stage_sizes[i] = g_cached_stage_bufs[i] ? m_buffers[i].size : 0;

            /* Pre-fill buffer with neutral gray YUV420sp (Y=128, UV=128) */
            size_t y_sz = (m_preview_w > 0 && m_preview_h > 0) ? (m_preview_w * m_preview_h) : (640 * 480);
            size_t uv_sz = y_sz / 2;
            char* p = (char*)m_buffers[i].base;
            memset(p, 128, y_sz);
            memset(p + y_sz, 128, uv_sz);

            disk_log("Preview buffer %d: handle=%p, offset=%zu, base=%p, mem=%p",
                     i, hnd, m_buffers[i].offset, m_buffers[i].base, m_buffers[i].mem.get());
        }
        g_shim_buffer_count = count;
        return 0;
    }

    int handle_dequeue_buffer(buffer_handle_t** buffer, int* stride) {
        if (m_buffer_count > 0) {
            int start = m_deq_cursor % m_buffer_count;
            for (int n = 0; n < m_buffer_count; n++) {
                int i = (start + n) % m_buffer_count;
                if (m_buffers[i].queued) {
                    m_buffers[i].queued = false;
                    m_deq_cursor = (i + 1) % m_buffer_count;
                    *buffer = (buffer_handle_t*)&m_buffers[i].handle;
                    if (stride) *stride = m_preview_w > 0 ? m_preview_w : 640;
                    return 0;
                }
            }
        }
        disk_log("preview_stream_ops::dequeue_buffer: no free buffer! re-using buffer 0");
        *buffer = (buffer_handle_t*)&m_buffers[0].handle;
        if (stride) *stride = m_preview_w > 0 ? m_preview_w : 640;
        return 0;
    }

    int handle_enqueue_buffer(buffer_handle_t* buffer) {
        g_enqueue_count++;
        int idx = -1;
        for (int i = 0; i < m_buffer_count; i++) {
            if ((buffer_handle_t*)m_buffers[i].handle == buffer ||
                (buffer_handle_t*)&m_buffers[i].handle == buffer ||
                (buffer && *(buffer_handle_t**)buffer == (buffer_handle_t*)m_buffers[i].handle)) {
                idx = i;
                m_buffers[i].queued = true;
                break;
            }
        }
        if (g_enqueue_count <= 3) {
            disk_log("preview_stream_ops::enqueue_buffer #%d: arg_buf=%p, idx=%d, mem=%p, cb=%p, user=%p",
                     g_enqueue_count, buffer, idx, idx >= 0 ? m_buffers[idx].mem.get() : NULL, m_data_cb, m_user);
            if (idx >= 0 && m_buffers[idx].base) {
                dump_hex("enqueued buf.base", m_buffers[idx].base, 16);
            }
        }
        // If handle matching failed, copy from last HAL buffer to buf 0
        if (idx < 0) {
            int hal_idx = m_last_hal_buf_idx;
            if (hal_idx >= 0 && hal_idx < m_buffer_count &&
                m_buffers[0].base && m_buffers[hal_idx].base &&
                hal_idx != 0) {
                size_t sz = m_preview_w * m_preview_h * 3 / 2;
                if (sz <= m_buffers[0].size && sz <= m_buffers[hal_idx].size) {
                    memcpy(m_buffers[0].base, m_buffers[hal_idx].base, sz);
                }
            }
            idx = 0;
        }
        if (!g_start_preview_in_progress && idx >= 0) {
            if (m_data_cb) {
                m_data_cb(CAMERA_MSG_PREVIEW_FRAME, m_buffers[idx].mem, m_user);
            }
            if (m_recording && m_data_cb_ts && m_buffers[idx].base && m_preview_w > 0 && m_preview_h > 0) {
                size_t sz = (size_t)m_preview_w * m_preview_h * 3 / 2;
                if (sz <= m_buffers[idx].size) {
                    sp<MemoryBase> rec_mem;
                    void* rec_dst = NULL;
                    pthread_mutex_lock(&m_rec_lock);
                    if (m_recording) {
                        for (int i = 0; i < MAX_REC_SLOTS; i++) {
                            if (!m_rec_slots[i].in_use) {
                                if (m_rec_slots[i].size != sz || m_rec_slots[i].heap == NULL) {
                                    m_rec_slots[i].mem.clear();
                                    m_rec_slots[i].heap = new MemoryHeapBase(sz, 0, "cam_rec_frame");
                                    m_rec_slots[i].base = (m_rec_slots[i].heap != NULL) ? m_rec_slots[i].heap->getBase() : NULL;
                                    m_rec_slots[i].size = sz;
                                    if (m_rec_slots[i].base) {
                                        m_rec_slots[i].mem = new MemoryBase(m_rec_slots[i].heap, 0, sz);
                                    }
                                }
                                if (m_rec_slots[i].mem != NULL && m_rec_slots[i].base) {
                                    m_rec_slots[i].in_use = true;
                                    rec_mem = m_rec_slots[i].mem;
                                    rec_dst = m_rec_slots[i].base;
                                }
                                break;
                            }
                        }
                    }
                    pthread_mutex_unlock(&m_rec_lock);
                    if (rec_mem != NULL && rec_dst) {
                        int st_idx = (idx >= 0 && idx < 8 && g_cached_stage_bufs[idx] && g_cached_stage_sizes[idx] >= sz && g_cached_stage_ts_ns[idx] > 0)
                                     ? idx : g_last_stage_idx;
                        const void* rec_src = (st_idx >= 0 && st_idx < 8 && g_cached_stage_bufs[st_idx] && g_cached_stage_sizes[st_idx] >= sz)
                                              ? (const void*)g_cached_stage_bufs[st_idx]
                                              : (const void*)m_buffers[idx].base;
                        nsecs_t ts_ns = (st_idx >= 0 && st_idx < 8 && g_cached_stage_ts_ns[st_idx] > 0)
                                        ? (nsecs_t)g_cached_stage_ts_ns[st_idx]
                                        : systemTime(SYSTEM_TIME_MONOTONIC);
                        memcpy(rec_dst, rec_src, sz);
                        m_data_cb_ts(ts_ns, CAMERA_MSG_VIDEO_FRAME, rec_mem, m_user);
                    }
                }
            }
        }
        return 0;
    }



    int handle_cancel_buffer(buffer_handle_t* buffer) {
        int idx = -1;
        for (int i = 0; i < m_buffer_count; i++) {
            if ((buffer_handle_t*)m_buffers[i].handle == buffer ||
                (buffer_handle_t*)&m_buffers[i].handle == buffer ||
                (buffer && *(buffer_handle_t**)buffer == (buffer_handle_t*)m_buffers[i].handle)) {
                idx = i;
                m_buffers[i].queued = true;
                break;
            }
        }
        disk_log("preview_stream_ops::cancel_buffer: idx=%d, buf_arg=%p", idx, buffer);
        return 0;
    }

public:
    void on_frame_delivered(void* frame) {
        if (g_start_preview_in_progress || g_enqueue_count > 0) {
            return;
        }
        static int s_fallback_frames = 0;
        s_fallback_frames++;
        if (s_fallback_frames <= 10 || (s_fallback_frames % 30) == 0) {
            disk_log("on_frame_delivered fallback #%d: m_buffer_count=%d, m_data_cb=%p",
                     s_fallback_frames, m_buffer_count, m_data_cb);
        }
        if (m_buffer_count > 0 && m_data_cb) {
            int idx = s_fallback_frames % m_buffer_count;
            if (frame) {
                char* src = *(char**)((char*)frame + 16);
                size_t sz = (m_preview_w * m_preview_h * 3) / 2;
                if (src && m_buffers[idx].base && sz <= m_buffers[idx].size) {
                    memcpy(m_buffers[idx].base, src, sz);
                }
            }
            m_data_cb(CAMERA_MSG_PREVIEW_FRAME, m_buffers[idx].mem, m_user);
        }
    }
    enum FocusModeKind {
        FOCUS_AUTO = 0,
        FOCUS_MACRO,
        FOCUS_INFINITY,
        FOCUS_CONTINUOUS,
        FOCUS_FIXED,
    };

    String8 m_wb_str;
    String8 m_scene_str;
    String8 m_flash_str;
    String8 m_focus_str;
    String8 m_meter_str;
    int m_ev_val;
    int m_led_mode;
    int m_rotation;
    FocusModeKind m_focus_kind;
    bool m_has_focus_roi;
    bool m_roi_dirty;
    uint16_t m_focus_roi[6];
    bool m_params_inited;
    int m_applied_config_fd;
    int m_applied_wb;
    int m_applied_ev;
    int m_applied_scene;
    int m_applied_meter;
    int m_applied_led_mode;
    FocusModeKind m_applied_focus;

    pthread_mutex_t m_af_lock;
    volatile uint32_t m_af_gen;
    volatile bool m_af_cancel;

    static FocusModeKind parse_focus_mode(const char* s) {
        if (!s) return FOCUS_AUTO;
        if (strcmp(s, "macro") == 0) return FOCUS_MACRO;
        if (strcmp(s, "infinity") == 0) return FOCUS_INFINITY;
        if (strcmp(s, "continuous-video") == 0 || strcmp(s, "continuous-picture") == 0) return FOCUS_CONTINUOUS;
        if (strcmp(s, "fixed") == 0) return FOCUS_FIXED;
        return FOCUS_AUTO;
    }

    static bool parse_focus_area(const char* area_str, int prev_w, int prev_h, uint16_t roi[6]) {
        if (!area_str || area_str[0] == '\0') return false;
        int l = 0, t = 0, r = 0, b = 0, w = 0;
        if (sscanf(area_str, " ( %d , %d , %d , %d , %d )", &l, &t, &r, &b, &w) != 5) {
            return false;
        }
        if (w <= 0 || (l == 0 && t == 0 && r == 0 && b == 0)) return false;
        int cx = (l + r) / 2 + 1000;
        int cy = (t + b) / 2 + 1000;
        if (cx < 0) cx = 0; if (cx > 2000) cx = 2000;
        if (cy < 0) cy = 0; if (cy > 2000) cy = 2000;
        int ratio = (prev_h > 0) ? (prev_w * 1000 / prev_h) : 1333;
        roi[0] = (uint16_t)cx;
        roi[1] = (uint16_t)cy;
        roi[2] = 0;
        roi[3] = 0;
        roi[4] = (uint16_t)ratio;
        roi[5] = 1;
        return true;
    }

    void apply_kernel_parameters(bool force) {
        if (g_config_fd < 0) return;
        if (m_applied_config_fd != g_config_fd) {
            force = true;
            m_applied_config_fd = g_config_fd;
            kernel_flash_ctrl(m_led_mode, K_LED_STATE_INIT);
        }

        int scene = map_lookup(kSceneMap, sizeof(kSceneMap) / sizeof(kSceneMap[0]), m_scene_str.string(), 1);
        int wb = map_lookup(kWbMap, sizeof(kWbMap) / sizeof(kWbMap[0]), m_wb_str.string(), 1);
        int meter = map_lookup(kMeterMap, sizeof(kMeterMap) / sizeof(kMeterMap[0]), m_meter_str.string(), 1);
        int ev = m_ev_val;
        if (ev < -6) ev = -6;
        if (ev > 6) ev = 6;

        bool scene_changed = force || (scene != m_applied_scene);
        if (scene_changed) {
            kernel_v4l2_s_ctrl(K_V4L2_CID_BESTSHOT_MODE, scene, NULL);
            m_applied_scene = scene;
        }

        if (force || wb != m_applied_wb) {
            kernel_v4l2_s_ctrl(K_V4L2_CID_WB_TEMP, wb, NULL);
            m_applied_wb = wb;
        }

        if (force || ev != m_applied_ev) {
            /* Kernel msm_sensor_write_enum_conf_array truncates enum_val to uint16_t,
             * breaking negative EVs; use HW EV for >=0 and fast Y-LUT for -6..-1. */
            kernel_v4l2_s_ctrl(K_V4L2_CID_EXPOSURE, (ev < 0) ? 0 : ev, NULL);
            set_neg_ev_level(ev);
            m_applied_ev = ev;
        }

        if (force || meter != m_applied_meter) {
            kernel_v4l2_s_ctrl(K_V4L2_CID_METER_MODE, meter, NULL);
            m_applied_meter = meter;
        }

        if (force || m_led_mode != m_applied_led_mode) {
            int led_state = (m_led_mode == K_LED_MODE_TORCH) ? K_LED_STATE_HIGH : K_LED_STATE_OFF;
            kernel_flash_ctrl(m_led_mode, led_state);
            m_applied_led_mode = m_led_mode;
        }

        if (force || m_focus_kind != m_applied_focus) {
            if (m_focus_kind == FOCUS_MACRO) {
                kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_MACRO, NULL);
            } else if (m_focus_kind == FOCUS_INFINITY) {
                kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_INFINITY, NULL);
            } else if (m_focus_kind == FOCUS_FIXED) {
                kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_OFF, NULL);
            } else if (m_focus_kind == FOCUS_CONTINUOUS) {
                if (m_applied_focus == FOCUS_MACRO || m_applied_focus == FOCUS_INFINITY) {
                    kernel_v4l2_s_ctrl(K_V4L2_CID_BESTSHOT_MODE, scene, NULL);
                }
                kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_CAF, NULL);
            } else { /* FOCUS_AUTO */
                if (!scene_changed && (m_applied_focus == FOCUS_MACRO || m_applied_focus == FOCUS_INFINITY)) {
                    kernel_v4l2_s_ctrl(K_V4L2_CID_BESTSHOT_MODE, scene, NULL);
                } else if (m_applied_focus == FOCUS_FIXED || m_applied_focus == FOCUS_CONTINUOUS) {
                    kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_CAF, NULL);
                }
            }
            m_applied_focus = m_focus_kind;
        }
    }

    QcomCameraAdapter(camera_device_t* dev) : m_dev(dev),
        m_notify_cb(NULL), m_data_cb(NULL), m_data_cb_ts(NULL), m_user(NULL),
        m_buffer_count(0), m_preview_w(640), m_preview_h(480), m_preview_format(0),
        m_master_pmem_fd(-1), m_master_base(NULL), m_master_size(0),
        m_last_hal_buf_idx(0),
        m_deq_cursor(0),
        m_recording(false),
        m_wb_str("auto"),
        m_scene_str("auto"),
        m_flash_str("off"),
        m_focus_str("auto"),
        m_meter_str("center-weighted"),
        m_ev_val(0),
        m_led_mode(K_LED_MODE_OFF),
        m_rotation(0),
        m_focus_kind(FOCUS_AUTO),
        m_has_focus_roi(false),
        m_roi_dirty(false),
        m_params_inited(false),
        m_applied_config_fd(-1),
        m_applied_wb(-999),
        m_applied_ev(-999),
        m_applied_scene(-999),
        m_applied_meter(-999),
        m_applied_led_mode(-999),
        m_applied_focus(FOCUS_AUTO),
        m_af_gen(0),
        m_af_cancel(false)
    {
        memset(m_focus_roi, 0, sizeof(m_focus_roi));
        pthread_mutex_init(&m_af_lock, NULL);
        pthread_mutex_init(&m_rec_lock, NULL);
        for (int i = 0; i < MAX_REC_SLOTS; i++) {
            m_rec_slots[i].base = NULL;
            m_rec_slots[i].size = 0;
            m_rec_slots[i].in_use = false;
        }
        disk_log("QcomCameraAdapter constructor: dev=%p, this=%p", dev, this);
        g_active_adapter = this;
        setup_camera_callbacks();
        memset(m_buffers, 0, sizeof(m_buffers));
        for (int i = 0; i < MAX_BUFFERS; i++) {
            m_buffers[i].pmem_fd = -1;
        }

        memset(&m_stream_ops, 0, sizeof(m_stream_ops));
        m_stream_ops.adapter = this;
        m_stream_ops.ops.dequeue_buffer = s_dequeue_buffer;
        m_stream_ops.ops.enqueue_buffer = s_enqueue_buffer;
        m_stream_ops.ops.cancel_buffer = s_cancel_buffer;
        m_stream_ops.ops.set_buffer_count = s_set_buffer_count;
        m_stream_ops.ops.set_buffers_geometry = s_set_buffers_geometry;
        m_stream_ops.ops.set_crop = s_set_crop;
        m_stream_ops.ops.set_usage = s_set_usage;
        m_stream_ops.ops.set_swap_interval = s_set_swap_interval;
        m_stream_ops.ops.get_min_undequeued_buffer_count = s_get_min_undequeued_buffer_count;
        m_stream_ops.ops.lock_buffer = s_lock_buffer;
    }

    virtual ~QcomCameraAdapter() {
        disk_log("~QcomCameraAdapter ENTER: m_dev=%p", m_dev);
        g_hal_preview_started = false;
        g_preview_active = false;
        if (g_active_adapter == this) {
            g_active_adapter = NULL;
        }
        if (m_dev) {
            if (m_dev->common.close) {
                disk_log("Closing camera_device...");
                m_dev->common.close(&m_dev->common);
            }
            m_dev = NULL;
        }
        m_recording = false;
        pthread_mutex_lock(&m_rec_lock);
        for (int i = 0; i < MAX_REC_SLOTS; i++) {
            m_rec_slots[i].mem.clear();
            m_rec_slots[i].heap.clear();
            m_rec_slots[i].base = NULL;
            m_rec_slots[i].size = 0;
            m_rec_slots[i].in_use = false;
        }
        pthread_mutex_unlock(&m_rec_lock);
        pthread_mutex_destroy(&m_rec_lock);
        pthread_mutex_destroy(&m_af_lock);
        cleanup_buffers();
        disk_log("~QcomCameraAdapter EXIT");
    }

    virtual sp<IMemoryHeap> getPreviewHeap() const {
        disk_log("QcomCameraAdapter::getPreviewHeap -> %p", m_preview_heap.get());
        return m_preview_heap;
    }

    virtual sp<IMemoryHeap> getRawHeap() const {
        disk_log("QcomCameraAdapter::getRawHeap");
        return NULL;
    }

    virtual void setCallbacks(notify_callback notify_cb,
                              data_callback data_cb,
                              data_callback_timestamp data_cb_timestamp,
                              void* user) {
        disk_log("QcomCameraAdapter::setCallbacks: notify=%p, data=%p, data_ts=%p, user=%p",
                 notify_cb, data_cb, data_cb_timestamp, user);
        m_notify_cb = notify_cb;
        m_data_cb = data_cb;
        m_data_cb_ts = data_cb_timestamp;
        m_user = user;
        disk_log("QcomCameraAdapter::setCallbacks: preserved user=%p", m_user);
        if (m_dev && m_dev->ops && m_dev->ops->set_callbacks) {
            m_dev->ops->set_callbacks(m_dev, s_notify_cb, s_data_cb, s_data_cb_ts, s_get_memory, this);
        }
    }

    virtual void enableMsgType(int32_t msgType) {
        disk_log("QcomCameraAdapter::enableMsgType(0x%x)", msgType);
        if (m_dev && m_dev->ops && m_dev->ops->enable_msg_type) {
            m_dev->ops->enable_msg_type(m_dev, msgType);
        }
    }

    virtual void disableMsgType(int32_t msgType) {
        disk_log("QcomCameraAdapter::disableMsgType(0x%x)", msgType);
        if (m_dev && m_dev->ops && m_dev->ops->disable_msg_type) {
            m_dev->ops->disable_msg_type(m_dev, msgType);
        }
    }

    virtual bool msgTypeEnabled(int32_t msgType) {
        disk_log("QcomCameraAdapter::msgTypeEnabled(0x%x)", msgType);
        if (m_dev && m_dev->ops && m_dev->ops->msg_type_enabled) {
            return m_dev->ops->msg_type_enabled(m_dev, msgType) != 0;
        }
        return false;
    }

    virtual status_t startPreview() {
        disk_log("QcomCameraAdapter::startPreview ENTER (already_active=%d)", g_preview_active ? 1 : 0);
        if (!m_dev || !m_dev->ops) return NO_INIT;

        if (g_preview_active) {
            disk_log("QcomCameraAdapter::startPreview: already active, skipping re-start");
            return NO_ERROR;
        }

        if (g_hal_preview_started && g_v4l2_streaming) {
            disk_log("QcomCameraAdapter::startPreview: HAL/V4L2 preview already running, resuming without re-calling set_preview_window");
            g_preview_active = true;
            if (m_params_inited) {
                apply_kernel_parameters(false);
            }
            return NO_ERROR;
        }

        g_start_preview_in_progress = true;
        g_preview_active = true;
        g_frame_index = 0;
        g_enqueue_count = 0;
        setup_camera_callbacks();

        if (m_dev->ops->set_preview_window) {
            disk_log("Calling set_preview_window with m_stream_ops.ops=%p...", &m_stream_ops.ops);
            m_dev->ops->set_preview_window(m_dev, &m_stream_ops.ops);
        }

        status_t rc = NO_ERROR;
        if (m_dev->ops->start_preview) {
            disk_log("Calling m_dev->ops->start_preview...");
            rc = m_dev->ops->start_preview(m_dev);
            disk_log("m_dev->ops->start_preview returned rc=%d", rc);
            if (rc != NO_ERROR) {
                disk_log("m_dev->ops->start_preview returned %d, masking to NO_ERROR", rc);
                rc = NO_ERROR;
            }
        }
        g_hal_preview_started = true;
        g_start_preview_in_progress = false;
        if (m_params_inited) {
            if (g_v4l2_streaming) {
                apply_kernel_parameters(true);
            } else {
                g_pending_param_apply = true;
            }
        }
        disk_log("QcomCameraAdapter::startPreview EXIT rc=%d", rc);
        return rc;
    }

    virtual void stopPreview() {
        disk_log("QcomCameraAdapter::stopPreview");
        g_preview_active = false;
        g_hal_preview_started = false;
        stop_v4l2_streaming();
        if (m_dev && m_dev->ops && m_dev->ops->stop_preview) {
            m_dev->ops->stop_preview(m_dev);
        }
    }

    virtual bool previewEnabled() {
        disk_log("QcomCameraAdapter::previewEnabled -> %d", g_preview_active ? 1 : 0);
        return g_preview_active;
    }

    virtual status_t startRecording() {
        disk_log("QcomCameraAdapter::startRecording (shim direct video feed, %dx%d)", m_preview_w, m_preview_h);
        pthread_mutex_lock(&m_rec_lock);
        for (int i = 0; i < MAX_REC_SLOTS; i++) {
            m_rec_slots[i].in_use = false;
        }
        m_recording = true;
        pthread_mutex_unlock(&m_rec_lock);
        return NO_ERROR;
    }

    virtual void stopRecording() {
        disk_log("QcomCameraAdapter::stopRecording");
        pthread_mutex_lock(&m_rec_lock);
        m_recording = false;
        pthread_mutex_unlock(&m_rec_lock);
    }

    virtual bool recordingEnabled() {
        return m_recording;
    }

    virtual void releaseRecordingFrame(const sp<IMemory>& mem) {
        if (mem == NULL) return;
        void* ptr = mem->pointer();
        pthread_mutex_lock(&m_rec_lock);
        for (int i = 0; i < MAX_REC_SLOTS; i++) {
            if (m_rec_slots[i].mem.get() == mem.get() ||
                (ptr && m_rec_slots[i].base == ptr)) {
                m_rec_slots[i].in_use = false;
                break;
            }
        }
        pthread_mutex_unlock(&m_rec_lock);
    }

    struct AfThreadCtx {
        QcomCameraAdapter* self;
        uint32_t gen;
    };

    static void* autofocus_thread_func(void* arg) {
        AfThreadCtx* ctx = (AfThreadCtx*)arg;
        if (!ctx) return NULL;
        QcomCameraAdapter* self = ctx->self;
        uint32_t my_gen = ctx->gen;
        free(ctx);
        if (!self) return NULL;

        usleep(30000); // Allow CameraService::autoFocus to return and release mLock
        if (self->m_af_cancel || self->m_af_gen != my_gen || g_active_adapter != self) {
            return NULL;
        }

        FocusModeKind fmode = self->m_focus_kind;
        if (fmode == FOCUS_FIXED || fmode == FOCUS_INFINITY || g_config_fd < 0) {
            if (self->m_notify_cb && !self->m_af_cancel && self->m_af_gen == my_gen) {
                disk_log("autofocus_thread: fixed/infinity quick success");
                self->m_notify_cb(CAMERA_MSG_FOCUS, 1, 0, self->m_user);
            }
            return NULL;
        }

        /* Re-apply focus-area ROI if user requested touch AF, or reset center if previously shifted */
        if (self->m_has_focus_roi) {
            kernel_v4l2_s_ctrl(K_V4L2_CID_FOCUS_ROI, 1, self->m_focus_roi);
            self->m_roi_dirty = true;
        } else if (self->m_roi_dirty) {
            uint16_t center_roi[6] = { 1000, 1000, 0, 0, 1333, 0 };
            kernel_v4l2_s_ctrl(K_V4L2_CID_FOCUS_ROI, 0, center_roi);
            self->m_roi_dirty = false;
        }

        if (kernel_v4l2_s_ctrl(K_V4L2_CID_AF, K_AF_ON, NULL) < 0) {
            if (self->m_notify_cb && !self->m_af_cancel && self->m_af_gen == my_gen) {
                self->m_notify_cb(CAMERA_MSG_FOCUS, 1, 0, self->m_user);
            }
            return NULL;
        }

        bool ready = false;
        for (int attempt = 0; attempt < 2 && !self->m_af_cancel && self->m_af_gen == my_gen && g_preview_active; attempt++) {
            int64_t t_start = now_monotonic_ns();
            ready = false;
            while (!self->m_af_cancel && self->m_af_gen == my_gen && g_preview_active) {
                kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_POLLING, NULL);
                int8_t poll_rc = -1;
                if (kernel_cfg_get_i8(K_CFG_GET_RC_AF_CHECK, &poll_rc) == 0 && poll_rc == 0) {
                    ready = true;
                    break;
                }
                if ((now_monotonic_ns() - t_start) > 2500000000LL) {
                    break;
                }
                usleep(20000);
            }
            if (self->m_af_cancel || self->m_af_gen != my_gen) break;

            kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_GET_STATE, NULL);
            int8_t af_state = -1;
            kernel_cfg_get_i8(K_CFG_GET_RC_AF_CHECK, &af_state);
            disk_log("autofocus_thread: attempt=%d ready=%d af_state=%d", attempt, ready ? 1 : 0, (int)af_state);

            if (af_state == 2 && attempt == 0) {
                /* Fine search failed and kernel switched to full-range retry */
                continue;
            }
            int success = (af_state == 0) ? 1 : 0;
            if (self->m_notify_cb && !self->m_af_cancel && self->m_af_gen == my_gen && g_active_adapter == self) {
                disk_log("autofocus_thread: notifying CAMERA_MSG_FOCUS success=%d", success);
                self->m_notify_cb(CAMERA_MSG_FOCUS, success, 0, self->m_user);
            }
            return NULL;
        }

        /* Cancelled or stopped: ensure AF pre-flash torch is turned off */
        kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_GET_STATE, NULL);
        if (self->m_led_mode == K_LED_MODE_TORCH) {
            kernel_flash_ctrl(K_LED_MODE_TORCH, K_LED_STATE_HIGH);
        }
        return NULL;
    }

    static void* snapshot_thread_func(void* arg) {
        QcomCameraAdapter* self = (QcomCameraAdapter*)arg;
        if (!self) return NULL;
        disk_log("snapshot_thread: started (flash=%s, led_mode=%d, preview_active=%d)",
                 self->m_flash_str.string(), self->m_led_mode, g_preview_active ? 1 : 0);
        usleep(30000); // 30ms to allow CameraService::takePicture to return and release mLock

        // 0. Fire flash illumination BEFORE stopping preview frame capture
        bool fire_flash = false;
        int led_mode = self->m_led_mode;
        if (g_config_fd >= 0) {
            if (led_mode == K_LED_MODE_ON || led_mode == K_LED_MODE_RED_EYE) {
                fire_flash = true;
            } else if (led_mode == K_LED_MODE_AUTO) {
                int8_t flash_needed = 0;
                if (kernel_cfg_get_i8(K_CFG_GET_FLASH_STATE, &flash_needed) == 0 && flash_needed != 0) {
                    fire_flash = true;
                }
                disk_log("snapshot_thread: auto flash check -> flash_needed=%d", (int)flash_needed);
            }
        }

        if (fire_flash) {
            if (led_mode == K_LED_MODE_RED_EYE) {
                kernel_flash_ctrl(led_mode, K_LED_STATE_HIGH);
                usleep(100000);
                kernel_flash_ctrl(led_mode, K_LED_STATE_OFF);
                usleep(80000);
            }
            uint32_t seq0 = g_sensor_frame_seq;
            kernel_flash_ctrl(led_mode, K_LED_STATE_HIGH);
            if (g_preview_active) {
                int64_t t0 = now_monotonic_ns();
                while ((now_monotonic_ns() - t0) < 350000000LL) {
                    int64_t dt_ms = (now_monotonic_ns() - t0) / 1000000LL;
                    if (g_sensor_frame_seq >= seq0 + 2 && dt_ms >= 90) {
                        break;
                    }
                    usleep(10000);
                }
                disk_log("snapshot_thread: flash lit seq %u -> %u", seq0, g_sensor_frame_seq);
            } else {
                usleep(180000);
            }
        }

        g_preview_active = false;

        // 1. Shutter sound
        if (self->m_notify_cb) {
            disk_log("snapshot_thread: notifying CAMERA_MSG_SHUTTER");
            self->m_notify_cb(CAMERA_MSG_SHUTTER, 0, 0, self->m_user);
        }

        // 2. Identify the NV21 frame to capture (copy under lock to avoid tearing)
        int w = 0, h = 0;
        uint8_t* nv21_copy = NULL;
        const uint8_t* nv21_src = NULL;
        pthread_mutex_lock(&g_sensor_frame_mutex);
        if (g_has_sensor_frame) {
            w = g_sensor_frame_w;
            h = g_sensor_frame_h;
            size_t snap_sz = (size_t)w * h * 3 / 2;
            nv21_copy = (uint8_t*)malloc(snap_sz);
            if (nv21_copy) {
                memcpy(nv21_copy, g_last_sensor_frame, snap_sz);
                apply_neg_ev_y_plane(nv21_copy, (size_t)w * h);
                nv21_src = nv21_copy;
            }
            disk_log("snapshot_thread: using real sensor frame (%dx%d)!", w, h);
        }
        pthread_mutex_unlock(&g_sensor_frame_mutex);

        if (fire_flash) {
            kernel_flash_ctrl(led_mode, (led_mode == K_LED_MODE_TORCH) ? K_LED_STATE_HIGH : K_LED_STATE_OFF);
        }
        if (!nv21_src) {
            w = self->m_preview_w > 0 ? self->m_preview_w : (g_shim_preview_w > 0 ? g_shim_preview_w : 640);
            h = self->m_preview_h > 0 ? self->m_preview_h : (g_shim_preview_h > 0 ? g_shim_preview_h : 480);
            int cur_shim_idx = (g_frame_index > 0 ? g_frame_index - 1 : 0) % (g_shim_buffer_count > 0 ? g_shim_buffer_count : 1);
            if (g_shim_buffer_count > 0 && g_shim_buffers[cur_shim_idx].base) {
                nv21_src = (const uint8_t*)g_shim_buffers[cur_shim_idx].base;
            } else {
                int buf_idx = (self->m_last_hal_buf_idx >= 0 && self->m_last_hal_buf_idx < self->m_buffer_count) ? self->m_last_hal_buf_idx : 0;
                if (self->m_buffers[buf_idx].base) {
                    nv21_src = (const uint8_t*)self->m_buffers[buf_idx].base;
                } else if (g_shim_buffers[0].base) {
                    nv21_src = (const uint8_t*)g_shim_buffers[0].base;
                }
            }
        }

        // NOTE: CAMERA_MSG_RAW_IMAGE is intentionally omitted because CameraService::Client::handleRawPicture
        // calls mSurface->postBuffer(), which crashes/hangs when the buffer is not from getPreviewHeap().

        // 3. Compress frame directly to JPEG in memory
        if (nv21_src) {
            size_t max_jpeg_sz = (size_t)w * h * 2;
            uint8_t* jpeg_out = (uint8_t*)malloc(max_jpeg_sz);
            size_t jpeg_sz = 0;
            if (jpeg_out) {
                int ret = compress_nv21_to_jpeg(nv21_src, w, h, 85, self->m_rotation, jpeg_out, max_jpeg_sz, &jpeg_sz);
                disk_log("snapshot_thread: compressed NV21 (%dx%d, rot=%d) to JPEG: ret=%d, size=%zu", w, h, self->m_rotation, ret, jpeg_sz);
                if (ret == 0 && jpeg_sz > 0 && self->m_data_cb) {
                    sp<MemoryHeapBase> heap = new MemoryHeapBase(jpeg_sz, 0, "cam_jpeg");
                    if (heap->getBase()) {
                        memcpy(heap->getBase(), jpeg_out, jpeg_sz);
                        sp<MemoryBase> mem = new MemoryBase(heap, 0, jpeg_sz);
                        disk_log("snapshot_thread: delivering CAMERA_MSG_COMPRESSED_IMAGE (%zu bytes)", jpeg_sz);
                        self->m_data_cb(CAMERA_MSG_COMPRESSED_IMAGE, mem, self->m_user);
                    }
                }
                free(jpeg_out);
            }
        } else {
            disk_log("snapshot_thread: ERROR - no nv21_src buffer found!");
        }
        if (nv21_copy) {
            free(nv21_copy);
        }

        disk_log("snapshot_thread: finished");
        return NULL;
    }

    virtual status_t autoFocus() {
        disk_log("QcomCameraAdapter::autoFocus ENTER (focus=%s)", m_focus_str.string());
        pthread_mutex_lock(&m_af_lock);
        m_af_cancel = false;
        uint32_t gen = ++m_af_gen;
        pthread_mutex_unlock(&m_af_lock);

        AfThreadCtx* ctx = (AfThreadCtx*)malloc(sizeof(AfThreadCtx));
        if (!ctx) return NO_MEMORY;
        ctx->self = this;
        ctx->gen = gen;

        pthread_t t;
        pthread_create(&t, NULL, autofocus_thread_func, ctx);
        pthread_detach(t);
        return NO_ERROR;
    }

    virtual status_t cancelAutoFocus() {
        disk_log("QcomCameraAdapter::cancelAutoFocus ENTER");
        pthread_mutex_lock(&m_af_lock);
        m_af_cancel = true;
        m_af_gen++;
        pthread_mutex_unlock(&m_af_lock);
        if (m_focus_kind == FOCUS_CONTINUOUS && g_config_fd >= 0) {
            kernel_v4l2_s_ctrl(K_V4L2_CID_AF_MODE, K_AF_CAF, NULL);
        }
        return NO_ERROR;
    }

    virtual status_t takePicture() {
        disk_log("QcomCameraAdapter::takePicture ENTER");
        pthread_mutex_lock(&m_af_lock);
        m_af_cancel = true;
        m_af_gen++;
        pthread_mutex_unlock(&m_af_lock);
        /* Keep g_preview_active=true so flash capture inside snapshot_thread_func gets fresh lit frames */
        pthread_t t;
        pthread_create(&t, NULL, snapshot_thread_func, this);
        pthread_detach(t);
        return NO_ERROR;
    }

    virtual status_t cancelPicture() {
        disk_log("QcomCameraAdapter::cancelPicture");
        return NO_ERROR;
    }

    virtual status_t setParameters(const CameraParameters& params) {
        const char* p_wb    = params.get(CameraParameters::KEY_WHITE_BALANCE);
        const char* p_scene = params.get(CameraParameters::KEY_SCENE_MODE);
        const char* p_flash = params.get(CameraParameters::KEY_FLASH_MODE);
        const char* p_focus = params.get(CameraParameters::KEY_FOCUS_MODE);
        const char* p_ev    = params.get(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        const char* p_meter = params.get("auto-exposure");
        const char* p_areas = params.get("focus-areas");
        const char* p_rot   = params.get(CameraParameters::KEY_ROTATION);

        disk_log("QcomCameraAdapter::setParameters ENTER: prev=%s pic=%s focus=%s flash=%s wb=%s ev=%s scene=%s",
                 params.get(CameraParameters::KEY_PREVIEW_SIZE),
                 params.get(CameraParameters::KEY_PICTURE_SIZE),
                 p_focus ? p_focus : "(null)",
                 p_flash ? p_flash : "(null)",
                 p_wb ? p_wb : "(null)",
                 p_ev ? p_ev : "(null)",
                 p_scene ? p_scene : "(null)");
        if (!m_dev || !m_dev->ops || !m_dev->ops->set_parameters) return NO_INIT;

        if (p_wb && map_lookup(kWbMap, sizeof(kWbMap)/sizeof(kWbMap[0]), p_wb, -1) >= 0) {
            m_wb_str = p_wb;
        }
        if (p_scene && map_lookup(kSceneMap, sizeof(kSceneMap)/sizeof(kSceneMap[0]), p_scene, -1) >= 0) {
            m_scene_str = p_scene;
        }
        if (p_flash) {
            int lm = map_lookup(kFlashMap, sizeof(kFlashMap)/sizeof(kFlashMap[0]), p_flash, -1);
            if (lm >= 0) {
                m_flash_str = p_flash;
                m_led_mode = lm;
            }
        }
        if (p_focus) {
            m_focus_str = p_focus;
            m_focus_kind = parse_focus_mode(p_focus);
        }
        if (p_ev) {
            int ev = atoi(p_ev);
            if (ev < -6) ev = -6;
            if (ev > 6) ev = 6;
            m_ev_val = ev;
        }
        if (p_meter && map_lookup(kMeterMap, sizeof(kMeterMap)/sizeof(kMeterMap[0]), p_meter, -1) >= 0) {
            m_meter_str = p_meter;
        }
        int pw = 640, ph = 480;
        params.getPreviewSize(&pw, &ph);
        if (parse_focus_area(p_areas, pw, ph, m_focus_roi)) {
            m_has_focus_roi = true;
        } else {
            m_has_focus_roi = false;
        }
        if (p_rot) {
            int r = ((atoi(p_rot) % 360) + 360) % 360;
            if (r == 0 || r == 90 || r == 180 || r == 270) {
                m_rotation = r;
            }
        }
        m_params_inited = true;

        String8 s = params.flatten();
        status_t rc = m_dev->ops->set_parameters(m_dev, s.string());
        if (g_v4l2_streaming) {
            apply_kernel_parameters(false);
        } else {
            g_pending_param_apply = true;
        }

        disk_log("QcomCameraAdapter::setParameters EXIT hal_rc=%d", rc);
        if (rc != NO_ERROR) {
            disk_log("QcomCameraAdapter::setParameters: HAL returned %d, masking to NO_ERROR", rc);
            return NO_ERROR;
        }
        return rc;
    }

    virtual CameraParameters getParameters() const {
        disk_log("QcomCameraAdapter::getParameters ENTER");
        CameraParameters params;
        if (!m_dev || !m_dev->ops || !m_dev->ops->get_parameters) return params;
        char* s = m_dev->ops->get_parameters(m_dev);
        if (s) {
            params.unflatten(String8(s));
            if (m_dev->ops->put_parameters) {
                m_dev->ops->put_parameters(m_dev, s);
            }
        }
        if (!params.get("zoom-ratios") || (params.get("zoom-supported") && strcmp(params.get("zoom-supported"), "true") == 0)) {
            params.set("zoom-supported", "false");
            params.set("smooth-zoom-supported", "false");
            params.set("zoom-ratios", "100");
            params.set("max-zoom", "0");
            params.set("zoom", "0");
        }
        const char* val = params.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, "2592x1944,2048x1536,1600x1200,1280x960,640x480,320x240");
        }
        val = params.get(CameraParameters::KEY_PICTURE_SIZE);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_PICTURE_SIZE, "2592x1944");
        }
        val = params.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, "640x480,384x288,352x288,320x240,176x144");
        }
        val = params.get(CameraParameters::KEY_PREVIEW_SIZE);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_PREVIEW_SIZE, "640x480");
        }
        val = params.get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
        }
        val = params.get(CameraParameters::KEY_PICTURE_FORMAT);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_PICTURE_FORMAT, CameraParameters::PIXEL_FORMAT_JPEG);
        }
        val = params.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
        if (!val || strlen(val) == 0) {
            params.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "512x288,480x288,432x288,320x240,0x0");
        }

        /* Advertise and keep synchronized all ISX006 + LM3561 camera controls */
        params.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, "off,auto,on,torch,red-eye");
        params.set(CameraParameters::KEY_FLASH_MODE, m_flash_str.string());

        params.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, "auto,macro,infinity,continuous-video,fixed");
        params.set(CameraParameters::KEY_FOCUS_MODE, m_focus_str.string());
        if (m_focus_kind == FOCUS_INFINITY) {
            params.set(CameraParameters::KEY_FOCUS_DISTANCES, "1.50,Infinity,Infinity");
        } else if (m_focus_kind == FOCUS_MACRO) {
            params.set(CameraParameters::KEY_FOCUS_DISTANCES, "0.08,0.20,0.60");
        } else {
            params.set(CameraParameters::KEY_FOCUS_DISTANCES, "0.10,1.20,Infinity");
        }

        params.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, "auto,incandescent,fluorescent,daylight,cloudy-daylight");
        params.set(CameraParameters::KEY_WHITE_BALANCE, m_wb_str.string());

        params.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, "auto,portrait,landscape,night,night-portrait,beach,snow,sports,document");
        params.set(CameraParameters::KEY_SCENE_MODE, m_scene_str.string());

        params.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "-6");
        params.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "6");
        /* 0.333334 so floor(6 * step) == 2 in Gingerbread CameraSettings */
        params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0.333334");
        char ev_buf[16];
        snprintf(ev_buf, sizeof(ev_buf), "%d", m_ev_val);
        params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, ev_buf);

        params.set("auto-exposure-values", "frame-average,center-weighted,spot-metering");
        params.set("auto-exposure", m_meter_str.string());
        char rot_buf[16];
        snprintf(rot_buf, sizeof(rot_buf), "%d", m_rotation);
        params.set(CameraParameters::KEY_ROTATION, rot_buf);

        disk_log("QcomCameraAdapter::getParameters EXIT: prev=%s pic=%s focus=%s flash=%s wb=%s ev=%s scene=%s",
                 params.get(CameraParameters::KEY_PREVIEW_SIZE),
                 params.get(CameraParameters::KEY_PICTURE_SIZE),
                 params.get(CameraParameters::KEY_FOCUS_MODE),
                 params.get(CameraParameters::KEY_FLASH_MODE),
                 params.get(CameraParameters::KEY_WHITE_BALANCE),
                 params.get(CameraParameters::KEY_EXPOSURE_COMPENSATION),
                 params.get(CameraParameters::KEY_SCENE_MODE));
        return params;
    }

    virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2) {
        disk_log("QcomCameraAdapter::sendCommand(%d, %d, %d)", cmd, arg1, arg2);
        if (cmd == 1 /* CAMERA_CMD_SET_DISPLAY_ORIENTATION */ ||
            cmd == 2 /* CAMERA_CMD_ENABLE_SHUTTER_SOUND */ ||
            cmd == 3 /* CAMERA_CMD_PLAY_RECORDING_SOUND */) {
            if (m_dev && m_dev->ops && m_dev->ops->send_command) {
                m_dev->ops->send_command(m_dev, cmd, arg1, arg2);
            }
            return NO_ERROR;
        }
        if (!m_dev || !m_dev->ops || !m_dev->ops->send_command) return NO_ERROR;
        return m_dev->ops->send_command(m_dev, cmd, arg1, arg2);
    }

    virtual void release() {
        disk_log("QcomCameraAdapter::release");
        pthread_mutex_lock(&m_af_lock);
        m_af_cancel = true;
        m_af_gen++;
        pthread_mutex_unlock(&m_af_lock);
        set_neg_ev_level(0);
        if (g_config_fd >= 0) {
            kernel_flash_ctrl(K_LED_MODE_OFF, K_LED_STATE_OFF);
            kernel_flash_ctrl(K_LED_MODE_OFF, K_LED_STATE_RELEASE);
        }
        if (m_dev && m_dev->ops && m_dev->ops->release) {
            m_dev->ops->release(m_dev);
        }
    }

    virtual status_t dump(int fd, const Vector<String16>& args) const {
        disk_log("QcomCameraAdapter::dump");
        if (!m_dev || !m_dev->ops || !m_dev->ops->dump) return NO_INIT;
        return m_dev->ops->dump(m_dev, fd);
    }
};

} // namespace android

static void adapter_deliver_frame(void* frame) {
    if (g_active_adapter) {
        g_active_adapter->on_frame_delivered(frame);
    }
}

static void adapter_on_stream_ready() {
    if (g_active_adapter && g_pending_param_apply) {
        g_pending_param_apply = false;
        g_active_adapter->apply_kernel_parameters(true);
    }
}

extern "C" {

int HAL_getNumberOfCameras() {
    disk_log("HAL_getNumberOfCameras ENTER");
    ensure_hal_loaded();
    if (g_num_fn) {
        int n = g_num_fn();
        disk_log("HAL_getNumberOfCameras: g_num_fn returned %d", n);
        LOGI("HAL_getNumberOfCameras: g_num_fn returned %d", n);
        if (n > 0) return n;
    }
    return 1;
}

void HAL_getCameraInfo(int cameraId, struct android::CameraInfo* cameraInfo) {
    disk_log("HAL_getCameraInfo ENTER: cameraId=%d", cameraId);
    if (cameraInfo) {
        cameraInfo->facing = 0; /* CAMERA_FACING_BACK */
        cameraInfo->orientation = 90;
    }
    ensure_hal_loaded();
    if (g_info_fn && cameraInfo) {
        /* Qualcomm HAL expects CameraInfo structure to be at least 12 bytes
         * (facing, orientation, flags). If we pass an 8-byte struct on stack,
         * Qualcomm HAL writes flags (value 5) into the caller's saved registers
         * on the stack (r4 = cameraId), corrupting cameraId to 5!
         * Use a generous 64-byte buffer so Qualcomm HAL never overflows our stack frame! */
        uint32_t qcom_buf[16] = {0};
        qcom_buf[0] = 0;
        qcom_buf[1] = 90;
        g_info_fn(cameraId, (struct android::CameraInfo*)qcom_buf);
        cameraInfo->facing = (int)qcom_buf[0];
        if (qcom_buf[1] != 0) {
            cameraInfo->orientation = (int)qcom_buf[1];
        }
    }
    disk_log("HAL_getCameraInfo EXIT: facing=%d, orientation=%d", cameraInfo ? cameraInfo->facing : -1, cameraInfo ? cameraInfo->orientation : -1);
}

android::sp<android::CameraHardwareInterface> HAL_openCameraHardware(int cameraId) {
    disk_log("HAL_openCameraHardware ENTER: cameraId=%d", cameraId);
    ensure_hal_loaded();
    inject_sensor_dimensions();
    if (g_cam_open_fn && g_hmi) {
        char name[16];
        snprintf(name, sizeof(name), "%d", cameraId);
        camera_device_t* dev = NULL;
        disk_log("HAL_openCameraHardware: Calling camera_device_open(%p, '%s')...", g_hmi, name);
        int rc = g_cam_open_fn(g_hmi, name, (hw_device_t**)&dev);
        disk_log("HAL_openCameraHardware: camera_device_open returned rc=%d, dev=%p", rc, dev);
        if (rc == 0 && dev) {
            setup_camera_callbacks();
            disk_log("HAL_openCameraHardware: Creating QcomCameraAdapter with camera_device_t...");
            return new android::QcomCameraAdapter(dev);
        }
        disk_log("HAL_openCameraHardware: camera_device_open failed rc=%d!", rc);
    }
    return NULL;
}

} /* extern "C" */
