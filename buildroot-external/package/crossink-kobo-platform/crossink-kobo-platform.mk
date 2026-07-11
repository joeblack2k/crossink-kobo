################################################################################
#
# crossink-kobo-platform
#
################################################################################

CROSSINK_KOBO_PLATFORM_VERSION = 1.0.0
CROSSINK_KOBO_PLATFORM_SITE = $(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../platform/kobo
CROSSINK_KOBO_PLATFORM_SITE_METHOD = local
CROSSINK_KOBO_PLATFORM_LICENSE = GPL-3.0-or-later
CROSSINK_KOBO_PLATFORM_DEPENDENCIES = crossink-fbink
CROSSINK_KOBO_PLATFORM_INSTALL_STAGING = YES
CROSSINK_KOBO_PLATFORM_CONF_OPTS = -DBUILD_TESTING=OFF

$(eval $(cmake-package))
