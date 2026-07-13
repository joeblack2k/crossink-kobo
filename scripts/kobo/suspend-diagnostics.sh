#!/bin/sh
set -eu

read_or_na() { if [ -r "$1" ]; then cat "$1"; else printf 'N/A\n'; fi; }
printf 'boot_id='; read_or_na /proc/sys/kernel/random/boot_id
printf 'uptime='; read_or_na /proc/uptime
printf 'power_state='; read_or_na /sys/power/state
printf 'mem_sleep='; read_or_na /sys/power/mem_sleep
printf 'wakeup_count='; read_or_na /sys/power/wakeup_count
printf '\ninput devices:\n'
for input in /sys/class/input/input*; do [ -r "$input/name" ] || continue; printf '%s: ' "$(basename "$input")"; cat "$input/name"; done
printf '\nwakeup sources:\n'; read_or_na /sys/kernel/debug/wakeup_sources
printf '\ninterrupts:\n'; read_or_na /proc/interrupts
printf '\nrecent suspend events:\n'
if [ -r /data/.crossink/power/suspend-events.jsonl ]; then tail -n 40 /data/.crossink/power/suspend-events.jsonl; fi
printf '\nwatchdog early failures:\n'; read_or_na /data/.crossink/watchdog/early-start-failures
