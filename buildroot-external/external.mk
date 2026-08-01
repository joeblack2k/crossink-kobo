# Buildroot 2026.05's host-util-linux package enables cramfs helpers when
# host-e2fsprogs selects it.  They are not needed to create our ext4 image and
# are installed without an RPATH on Debian 13, which Buildroot correctly
# rejects.  Keep the required libuuid/libblkid/libmount libraries, but make
# the host package skip those two helpers.
HOST_UTIL_LINUX_CONF_OPTS += --disable-cramfs

define CROSSINK_KOBO_INSTALL_KERNEL_WAVEFORM
	test -f "$(CROSSINK_EPDC_FIRMWARE_FILE)"
	install -D -m 0644 "$(CROSSINK_EPDC_FIRMWARE_FILE)" \
		"$(LINUX_DIR)/firmware/imx/epdc/epdc.fw"
endef
LINUX_PRE_BUILD_HOOKS += CROSSINK_KOBO_INSTALL_KERNEL_WAVEFORM

include $(sort $(wildcard $(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/package/*/*.mk))
