LOCAL_PATH := $(call my-dir)

DUALBOOT_RAMDISK_TARGET := $(PRODUCT_OUT)/ramdisk-dualboot.img
DUALBOOT_ROOT_STAGING := $(PRODUCT_OUT)/root_dualboot
MKELF_SCRIPT := device/sony/mesona/tools/mkelf.py

$(DUALBOOT_RAMDISK_TARGET): $(MKBOOTFS) $(INSTALLED_RAMDISK_TARGET) $(recovery_ramdisk) $(TARGET_ROOT_OUT)/sbin/bootrec | $(MINIGZIP)
	@echo ----- Making dual-boot ramdisk with embedded recovery ------
	rm -rf $(DUALBOOT_ROOT_STAGING)
	cp -a $(TARGET_ROOT_OUT) $(DUALBOOT_ROOT_STAGING)
	mv -f $(DUALBOOT_ROOT_STAGING)/init $(DUALBOOT_ROOT_STAGING)/init.android
	mv -f $(DUALBOOT_ROOT_STAGING)/sbin/bootrec $(DUALBOOT_ROOT_STAGING)/init
	chmod 750 $(DUALBOOT_ROOT_STAGING)/init
	ln -sf ../init.android $(DUALBOOT_ROOT_STAGING)/sbin/ueventd
	cp -f $(recovery_ramdisk) $(DUALBOOT_ROOT_STAGING)/sbin/ramdisk-recovery.img
	chmod 644 $(DUALBOOT_ROOT_STAGING)/sbin/ramdisk-recovery.img
	$(hide) $(MKBOOTFS) $(DUALBOOT_ROOT_STAGING) | $(MINIGZIP) > $@

$(INSTALLED_BOOTIMAGE_TARGET): $(INSTALLED_KERNEL_TARGET) $(DUALBOOT_RAMDISK_TARGET) $(MKELF_SCRIPT)
	$(call pretty,"Target Sony ELF dual-boot image: $@")
	$(hide) python $(MKELF_SCRIPT) --kernel $(INSTALLED_KERNEL_TARGET) --ramdisk $(DUALBOOT_RAMDISK_TARGET) $(if $(BOARD_KERNEL_CMDLINE),--cmdline "$(BOARD_KERNEL_CMDLINE)") --output $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_BOOTIMAGE_PARTITION_SIZE),raw)

$(INSTALLED_RECOVERYIMAGE_TARGET): $(recovery_ramdisk) $(recovery_kernel) $(MKELF_SCRIPT)
	@echo ----- Making Sony ELF recovery image ------
	$(hide) python $(MKELF_SCRIPT) --kernel $(recovery_kernel) --ramdisk $(recovery_ramdisk) $(if $(BOARD_KERNEL_CMDLINE),--cmdline "$(BOARD_KERNEL_CMDLINE)") --output $@
	@echo ----- Made recovery image -------- $@
	$(hide) $(call assert-max-image-size,$@,$(BOARD_RECOVERYIMAGE_PARTITION_SIZE),raw)
