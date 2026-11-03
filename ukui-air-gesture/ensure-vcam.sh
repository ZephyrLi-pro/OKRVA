#!/bin/sh

set -e

LABEL="UKUI-Air-Gesture-Camera"
STATE_DIR="/run/ukui-air-gesture"
REAL_CAM_FILE="$STATE_DIR/real-camera"
USB_IFACES_FILE="$STATE_DIR/usb-ifaces"

log() {
    echo "ensure-vcam: $*" >&2
}

die() {
    log "$*"
    exit 1
}

mkdir -p "$STATE_DIR"

is_v4l2loopback_device() {
    sysdir="$1"
    if [ -L "$sysdir/device/driver" ]; then
        case "$(readlink "$sysdir/device/driver")" in
            *v4l2loopback*) return 0 ;;
        esac
    fi
    [ "$(cat "$sysdir/name" 2>/dev/null)" = "$LABEL" ]
}

is_video0_virtual_camera() {
    [ -e /sys/class/video4linux/video0/name ] || return 1
    [ "$(cat /sys/class/video4linux/video0/name)" = "$LABEL" ]
}

find_real_capture_node() {
    for sysdir in /sys/class/video4linux/video*; do
        [ -e "$sysdir/name" ] || continue
        is_v4l2loopback_device "$sysdir" && continue
        index="$(cat "$sysdir/index" 2>/dev/null || echo "")"
        if [ -z "$index" ] || [ "$index" = "0" ]; then
            echo "/dev/$(basename "$sysdir")"
            return 0
        fi
    done
    return 1
}

save_real_camera() {
    real="$1"
    echo "$real" > "$REAL_CAM_FILE"
    chmod 644 "$REAL_CAM_FILE" || true
    echo "$real"
}

wait_for() {
    desc="$1"
    shift
    i=0
    while [ "$i" -lt 50 ]; do
        if "$@"; then
            return 0
        fi
        i=$((i + 1))
        sleep 0.1
    done
    die "timeout: $desc"
}

video0_video1_gone() {
    [ ! -e /dev/video0 ] && [ ! -e /sys/class/video4linux/video0 ] \
        && [ ! -e /dev/video1 ] && [ ! -e /sys/class/video4linux/video1 ]
}

collect_uvc_ifaces() {
    node="$1"
    sysname="$(basename "$node")"
    devlink="/sys/class/video4linux/$sysname/device"
    [ -e "$devlink" ] || die "no sysfs device for $node"

    iface_path="$(readlink -f "$devlink")"
    usb_dev_path="$(readlink -f "$iface_path/..")"
    : > "$USB_IFACES_FILE"

    for link in /sys/bus/usb/drivers/uvcvideo/*; do
        [ -L "$link" ] || continue
        name="$(basename "$link")"
        case "$name" in
            module|bind|unbind|uevent|new_id|remove_id) continue ;;
        esac
        target="$(readlink -f "$link")"
        case "$target" in
            "$usb_dev_path"/*) echo "$name" >> "$USB_IFACES_FILE" ;;
        esac
    done

    [ -s "$USB_IFACES_FILE" ] || die "no uvcvideo interfaces for $node"
}

unbind_uvc() {
    while read -r iface; do
        [ -n "$iface" ] || continue
        [ -e "/sys/bus/usb/drivers/uvcvideo/$iface" ] || continue
        log "unbind $iface"
        echo "$iface" > /sys/bus/usb/drivers/uvcvideo/unbind || true
    done < "$USB_IFACES_FILE"
}

bind_uvc() {
    while read -r iface; do
        [ -n "$iface" ] || continue
        [ -e "/sys/bus/usb/drivers/uvcvideo/$iface" ] && continue
        log "bind $iface"
        echo "$iface" > /sys/bus/usb/drivers/uvcvideo/bind || true
    done < "$USB_IFACES_FILE"
}

if is_video0_virtual_camera; then
    if [ -s "$REAL_CAM_FILE" ] && [ -e "$(cat "$REAL_CAM_FILE")" ]; then
        log "already active, real=$(cat "$REAL_CAM_FILE")"
        cat "$REAL_CAM_FILE"
        exit 0
    fi
    real="$(find_real_capture_node)" || die "video0 is virtual but real camera missing"
    log "already active, real=$real"
    save_real_camera "$real"
    exit 0
fi

command -v modprobe >/dev/null 2>&1 || die "modprobe not found"

if [ -e /dev/video0 ] && ! is_v4l2loopback_device /sys/class/video4linux/video0; then
    seed_node="/dev/video0"
else
    seed_node="$(find_real_capture_node)" || die "no real camera found"
fi

log "relocate $seed_node, put $LABEL on video0"
collect_uvc_ifaces "$seed_node"

if lsmod | grep -q '^v4l2loopback'; then
    log "unload v4l2loopback"
    modprobe -r v4l2loopback || die "failed to unload v4l2loopback"
fi

unbind_uvc
wait_for "video0/1 still held; close camera apps and retry" video0_video1_gone
sleep 0.2

log "modprobe v4l2loopback as video0"
modprobe v4l2loopback devices=1 video_nr=0 card_label="$LABEL" exclusive_caps=1
wait_for "virtual camera failed to claim video0" is_video0_virtual_camera

bind_uvc
wait_for "real camera did not reappear" find_real_capture_node >/dev/null
real="$(find_real_capture_node)"
log "active: virtual=/dev/video0 real=$real"
save_real_camera "$real"
