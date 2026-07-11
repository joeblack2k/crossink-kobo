#!/bin/sh
# Read-only N437 inventory. Never assumes event numbers or sysfs names.
set -eu

section() {
	printf '\n[%s]\n' "$1"
}

read_file() {
	[ -r "$1" ] && tr '\n' ' ' <"$1" || printf '<unavailable>'
}

section identity
printf 'timestamp_utc='; date -u +%Y-%m-%dT%H:%M:%SZ
printf 'kernel='; uname -a
printf 'cmdline='; read_file /proc/cmdline; printf '\n'
printf 'cpuinfo='; read_file /proc/cpuinfo; printf '\n'
printf 'memory='; read_file /proc/meminfo; printf '\n'

section framebuffer
for fb in /sys/class/graphics/fb*; do
	[ -e "$fb" ] || continue
	printf '%s name=%s virtual_size=%s stride=%s bits_per_pixel=%s rotate=%s\n' \
		"$fb" "$(read_file "$fb/name")" "$(read_file "$fb/virtual_size")" \
		"$(read_file "$fb/stride")" "$(read_file "$fb/bits_per_pixel")" \
		"$(read_file "$fb/rotate")"
done

section input
for input in /sys/class/input/input*; do
	[ -e "$input" ] || continue
	printf '%s name=%s phys=%s modalias=%s\n' \
		"$input" "$(read_file "$input/name")" "$(read_file "$input/phys")" \
		"$(read_file "$input/modalias")"
	done
for event in /sys/class/input/event*; do
	[ -e "$event" ] || continue
	printf '%s device=%s\n' "$event" "$(readlink "$event/device" 2>/dev/null || true)"
done

section backlight
for light in /sys/class/backlight/*; do
	[ -e "$light" ] || continue
	printf '%s brightness=%s actual=%s max=%s power=%s\n' \
		"$light" "$(read_file "$light/brightness")" \
		"$(read_file "$light/actual_brightness")" "$(read_file "$light/max_brightness")" \
		"$(read_file "$light/bl_power")"
done

section power_supply
for supply in /sys/class/power_supply/*; do
	[ -e "$supply" ] || continue
	printf '%s type=%s status=%s capacity=%s voltage_now=%s current_now=%s online=%s\n' \
		"$supply" "$(read_file "$supply/type")" "$(read_file "$supply/status")" \
		"$(read_file "$supply/capacity")" "$(read_file "$supply/voltage_now")" \
		"$(read_file "$supply/current_now")" "$(read_file "$supply/online")"
done

section network
for net in /sys/class/net/*; do
	[ -e "$net" ] || continue
	printf '%s address=%s operstate=%s type=%s wireless=%s\n' \
		"$net" "$(read_file "$net/address")" "$(read_file "$net/operstate")" \
		"$(read_file "$net/type")" "$(test -d "$net/wireless" && printf yes || printf no)"
done

section storage
mount
df -h

section kernel_log_tail
dmesg | tail -n 250
