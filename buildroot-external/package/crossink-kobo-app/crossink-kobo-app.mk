################################################################################
#
# crossink-kobo-app
#
################################################################################

CROSSINK_KOBO_APP_VERSION = 1.4.0-kobo-beta3
CROSSINK_KOBO_APP_SITE = $(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../platform/kobo/app
CROSSINK_KOBO_APP_SITE_METHOD = local
CROSSINK_KOBO_APP_LICENSE = GPL-3.0-or-later
CROSSINK_KOBO_APP_DEPENDENCIES = crossink-kobo-platform openssl host-python3
CROSSINK_KOBO_APP_INSTALL_TARGET = YES
CROSSINK_KOBO_APP_CONF_OPTS = \
	-DBUILD_TESTING=OFF \
	-DPython3_EXECUTABLE=$(HOST_DIR)/bin/python3 \
	-DCROSSINK_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/.. \
	-DCROSSPOINT_SIMULATOR_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/crosspoint-simulator \
	-DARDUINOJSON_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/ArduinoJson \
	-DQRCODE_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/QRCode \
	-DPNGDEC_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/PNGdec \
	-DJPEGDEC_ROOT=$(BR2_EXTERNAL_CROSSINK_KOBO_PATH)/../vendor/JPEGDEC

$(eval $(cmake-package))
