################################################################################
#
# crossink-fbink
#
################################################################################

CROSSINK_FBINK_VERSION = 83110d3d278cf9cd44cc1d16237e284a89f72633
CROSSINK_FBINK_SITE = $(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/FBInk
CROSSINK_FBINK_SITE_METHOD = local
CROSSINK_FBINK_LICENSE = GPL-3.0-or-later
CROSSINK_FBINK_LICENSE_FILES = LICENSE
CROSSINK_FBINK_INSTALL_STAGING = YES

CROSSINK_FBINK_MAKE_OPTS = \
	CC="$(TARGET_CC)" \
	CXX="$(TARGET_CXX)" \
	AR="$(TARGET_AR)" \
	RANLIB="$(TARGET_RANLIB)" \
	STRIP="$(TARGET_STRIP)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	MINIMAL=true DRAW=true BITMAP=true

define CROSSINK_FBINK_BUILD_CMDS
	rm -rf $(@D)/libi2c-staged $(@D)/libi2c.built
	# The local FBInk source can contain host-built i2c-tools artefacts.  Their
	# timestamps make i2c-tools reuse x86-64 objects during an ARM rebuild.
	# Remove all generated library objects before invoking the target compiler.
	rm -f $(@D)/i2c-tools/lib/*.ao $(@D)/i2c-tools/lib/*.o \
		$(@D)/i2c-tools/lib/*.a $(@D)/i2c-tools/lib/*.so*
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) cleanstaticlib
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) staticlib $(CROSSINK_FBINK_MAKE_OPTS)
endef

define CROSSINK_FBINK_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/Release/libfbink.a $(STAGING_DIR)/usr/lib/libfbink.a
	$(INSTALL) -D -m 0644 $(@D)/fbink.h $(STAGING_DIR)/usr/include/fbink.h
endef

$(eval $(generic-package))
