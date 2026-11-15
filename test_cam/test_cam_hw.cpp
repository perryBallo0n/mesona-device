#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <camera/CameraHardwareInterface.h>

using namespace android;

static void disk_log(const char* fmt, ...) {
    FILE* fp = fopen("/data/local/tmp/test.log", "a");
    if (!fp) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
}

static void print_r4(const char* tag) {
    register int r4 asm("r4");
    disk_log("DEBUG [%s]: r4 = 0x%08x", tag, r4);
}

static void cam_notify_cb(int32_t msg_type, int32_t ext1, int32_t ext2, void *user) {
    disk_log("CALLBACK notify: msg_type=0x%x, ext1=%d, ext2=%d", msg_type, ext1, ext2);
    printf("CALLBACK notify: msg_type=0x%x, ext1=%d, ext2=%d\n", msg_type, ext1, ext2);
}

static void cam_data_cb(int32_t msg_type, const sp<IMemory>& data, void *user) {
    disk_log("CALLBACK data: msg_type=0x%x, size=%zu", msg_type, data != 0 ? data->size() : 0);
    printf("CALLBACK data: msg_type=0x%x, size=%zu\n", msg_type, data != 0 ? data->size() : 0);
}

static void cam_data_cb_ts(nsecs_t timestamp, int32_t msg_type, const sp<IMemory>& data, void *user) {
    disk_log("CALLBACK data_ts: ts=%lld, msg_type=0x%x", (long long)timestamp, msg_type);
}

int main(int argc, char** argv) {
    disk_log("=== test_cam_hw START ===");
    printf("--- Testing Camera Hardware HAL ---\n");
    fflush(stdout);

    disk_log("0. Initializing ProcessState...");
    printf("0. Initializing ProcessState / Binder thread pool...\n");
    fflush(stdout);
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();
    disk_log("0. ProcessState initialized");
    
    disk_log("1. Calling HAL_getNumberOfCameras()...");
    int numCameras = HAL_getNumberOfCameras();
    disk_log("1. HAL_getNumberOfCameras: %d", numCameras);
    printf("1. HAL_getNumberOfCameras: %d\n", numCameras);
    fflush(stdout);
    if (numCameras <= 0) {
        disk_log("Error: no cameras detected!");
        printf("Error: no cameras detected!\n");
        return 1;
    }

    struct CameraInfo info;
    info.facing = -1;
    info.orientation = -1;
    disk_log("2. Calling HAL_getCameraInfo(0)...");
    HAL_getCameraInfo(0, &info);
    disk_log("2. HAL_getCameraInfo(0): facing=%d, orientation=%d", info.facing, info.orientation);
    printf("2. HAL_getCameraInfo(0): facing=%d, orientation=%d\n", info.facing, info.orientation);
    fflush(stdout);

    disk_log("3. Calling HAL_openCameraHardware(0)...");
    printf("3. Calling HAL_openCameraHardware(0)...\n");
    fflush(stdout);
    print_r4("before openCameraHardware");
    sp<CameraHardwareInterface> hw = HAL_openCameraHardware(0);
    print_r4("after openCameraHardware");
    if (hw == NULL) {
        disk_log("Error: HAL_openCameraHardware(0) returned NULL!");
        printf("Error: HAL_openCameraHardware(0) returned NULL!\n");
        return 1;
    }
    disk_log("SUCCESS: Got CameraHardwareInterface pointer: %p", hw.get());
    printf("SUCCESS: Got CameraHardwareInterface pointer: %p\n", hw.get());
    fflush(stdout);

    print_r4("before getParameters");
    disk_log("4. Calling hw->getParameters()...");
    printf("4. Calling hw->getParameters()...\n");
    fflush(stdout);
    CameraParameters params = hw->getParameters();
    print_r4("after getParameters");
    String8 str8 = params.flatten();
    print_r4("after flatten");
    const char* str = str8.string();
    print_r4("after str8.string()");
    disk_log("SUCCESS: Camera Parameters: %s", str ? str : "(null)");
    print_r4("after disk_log");
    printf("SUCCESS: Camera Parameters:\n%s\n", str ? str : "(null)");
    print_r4("after printf");
    fflush(stdout);
    print_r4("after fflush");

    disk_log("5. Setting callbacks & enabling CAMERA_MSG_PREVIEW_FRAME...");
    printf("5. Setting callbacks & enabling CAMERA_MSG_PREVIEW_FRAME...\n");
    fflush(stdout);
    hw->setCallbacks(cam_notify_cb, cam_data_cb, cam_data_cb_ts, NULL);
    hw->enableMsgType(CAMERA_MSG_PREVIEW_FRAME);

    disk_log("6. Calling hw->startPreview()...");
    printf("6. Calling hw->startPreview()...\n");
    fflush(stdout);
    status_t rc = hw->startPreview();
    disk_log("6. startPreview returned: %d", rc);
    printf("6. startPreview returned: %d\n", rc);
    fflush(stdout);

    disk_log("7. Sleeping 2 seconds while preview runs...");
    printf("7. Sleeping 2 seconds while preview runs...\n");
    fflush(stdout);
    sleep(2);

    disk_log("8. Calling hw->stopPreview()...");
    printf("8. Calling hw->stopPreview()...\n");
    fflush(stdout);
    hw->stopPreview();
    disk_log("8. stopPreview done");
    printf("8. stopPreview done\n");
    fflush(stdout);

    disk_log("9. Calling hw->release()...");
    printf("9. Calling hw->release()...\n");
    fflush(stdout);
    hw->release();
    disk_log("9. release done");
    printf("9. release done\n");
    fflush(stdout);

    disk_log("=== test_cam_hw COMPLETED SUCCESSFULLY ===");
    printf("=== test_cam_hw COMPLETED SUCCESSFULLY ===\n");
    fflush(stdout);
    return 0;
}
