#!/bin/sh
set -eu

target_dir="$1"
key_file="${CROSSINK_SSH_PUBLIC_KEY_FILE:-}"
waveform_file="${CROSSINK_EPDC_FIRMWARE_FILE:-}"

if [ -z "$key_file" ] || [ ! -f "$key_file" ]; then
	echo "CROSSINK_SSH_PUBLIC_KEY_FILE must name the dedicated public SSH key" >&2
	exit 1
fi

case "$(head -n 1 "$key_file")" in
	ssh-ed25519\ *) ;;
	*)
		echo "Only a dedicated Ed25519 public key is accepted" >&2
		exit 1
		;;
esac

install -d -m 0700 "$target_dir/root/.ssh"
install -m 0600 "$key_file" "$target_dir/root/.ssh/authorized_keys"

if [ -z "$waveform_file" ] || [ ! -f "$waveform_file" ]; then
	echo "CROSSINK_EPDC_FIRMWARE_FILE must name the validated N437 waveform" >&2
	exit 1
fi
install -d -m 0755 "$target_dir/lib/firmware/imx/epdc"
install -m 0644 "$waveform_file" "$target_dir/lib/firmware/imx/epdc/epdc.fw"

# linux-firmware stores this redistributable BCM43362 image under its Cypress
# name.  brcmfmac requests the historic Broadcom path.  The N437 uses the same
# WC121 calibration profile as the explicitly supported Kobo Aura and Tolino
# Shine 2HD boards in linux-firmware/Debian.
test -f "$target_dir/lib/firmware/cypress/cyfmac43362-sdio.bin"
install -d -m 0755 "$target_dir/lib/firmware/brcm"
ln -snf ../cypress/cyfmac43362-sdio.bin \
	"$target_dir/lib/firmware/brcm/brcmfmac43362-sdio.bin"
# The board-specific request is issued before brcmfmac falls back to the
# generic filename.  Point it at the same validated firmware so cold boots
# have a clean, deterministic driver probe.
ln -snf brcmfmac43362-sdio.bin \
	"$target_dir/lib/firmware/brcm/brcmfmac43362-sdio.kobo,glo-hd-n437.bin"
ln -snf brcmfmac43362-sdio.WC121.txt \
	"$target_dir/lib/firmware/brcm/brcmfmac43362-sdio.kobo,glo-hd-n437.txt"

# Buildroot's example files contain a dummy hotspot SSID and an open-network
# stanza. Ship no remembered network identity; CrossInk creates runtime files
# under /data only after the user explicitly configures networking.
rm -f "$target_dir/etc/hostapd.conf"
printf '%s\n' 'ctrl_interface=/run/wpa_supplicant' 'update_config=0' > "$target_dir/etc/wpa_supplicant.conf"
chmod 0600 "$target_dir/etc/wpa_supplicant.conf"

# Password authentication stays unavailable even if package defaults change.
sed -i '/^root:/c\root:*:0:0:root:/root:/bin/sh' "$target_dir/etc/shadow"
