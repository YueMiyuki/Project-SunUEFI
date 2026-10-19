#!/usr/bin/env python3
"""Replace the Frame device/session policy with Piano's KDE and Gamescope session."""
import shutil
import subprocess

from stage_arch_piano import put
from prepare_debian_desktop import configure as configure_kde

# These units control Frame hardware or its A/B partition layout. Piano uses
# the shared BSP and SunUEFI disk bootstrap instead.
FRAME_UNITS = (
    'deckard-factory-recovery.service', 'deckard-boot-images.service',
    'deckard-fan-control.service', 'deckard-fpga.service',
    'deckard-led-control.service', 'deckard-typec-logger.service',
    'deckard-charger.service', 'deckard-power-monitor.service',
    'steamos-headset-adb.service', 'steamos-headset-adb.path',
    'steamos-headset-usb-gadget.service', 'steamos-headset-usb-gadget.path',
    'steamos-headset-fpga-config.service', 'steamos-power-monitor.service',
    'dsp_service.service', 'iris-driver-rebind.service',
    'steamvr-program-ble.service', 'steamvr-set-kernel-thread-priorities.service',
    'steamvr-v4l2loopback.service', 'adbd.service', 'adbd-pre.service',
    'adbd-post.service', 'usb-gadget.target', 'usb-gadget.service',
    'usb-gadget-init.service', 'usb-ncm-gadget@.service',
    'usb-ncm-dnsmasq@.service', 'steamos-boot.service',
    'efi.mount', 'esp.mount', 'systemd-repart.service',
    'rauc.service', 'steamos-update.service',
    'setubootenv.service', 'deckard-fpga-resume.service',
    'deckard-audio-setup.service', 'steamos-offload.target', 'etc.mount',
    'steamos-post-update.service', 'holo-post-update-shutdown.service',
    'gpu-trace.service', 'softapmanager.service', 'steam-video-codec-setup.service',
)


def configure(root):
    # Frame mounts its own ESP at /efi and links /boot/efi there. SunUEFI's
    # shared fstab mounts SUNUEFI_ESP directly at /boot/efi.
    esp = root / 'boot/efi'
    if esp.is_symlink():
        esp.unlink()
    esp.mkdir(parents=True, exist_ok=True)
    for name in ('steamdeck.conf', 'steamos.conf'):
        (root / 'etc/sddm.conf.d' / name).unlink(missing_ok=True)
    # Frame's microphone processing and fixed two-channel spatial pipeline
    # must not override Piano's standard UCM routes and four-speaker layout.
    for name in ('40-mic-processing.conf', '50-alsa-config.conf',
                 '51-alsa-disable.conf', '60-spatial-audio.conf',
                 '70-spatial-node-config.conf'):
        (root / 'etc/wireplumber/wireplumber.conf.d' / name).unlink(missing_ok=True)
    for unit in FRAME_UNITS:
        link = root / 'etc/systemd/system' / unit
        link.unlink(missing_ok=True)
        link.symlink_to('/dev/null')
    offload = root / 'etc/tmpfiles.d/steamos-offload.conf'
    offload.parent.mkdir(parents=True, exist_ok=True)
    offload.unlink(missing_ok=True)
    offload.symlink_to('/dev/null')
    # The VR-only payload is not part of the handheld client/runtime. Its
    # PulseAudio link points at a Debian path absent from the Valve Arch image.
    if (root / 'opt/steamvr').exists():
        shutil.rmtree(root / 'opt/steamvr')
    desktop_link = root / 'usr/lib/steam/steam.desktop'
    desktop_link.unlink(missing_ok=True)
    desktop_link.symlink_to('/usr/share/applications/steam.desktop')
    # Keep this link relative to the *nested* x86 root, not the host ARM root.
    keyutils = root / 'usr/share/guestos/fex-mesa/usr/lib32/libkeyutils.so'
    keyutils.unlink()
    keyutils.symlink_to('libkeyutils.so.1')
    # Keep the native client and translation runtime. Do not reuse Frame's
    # VR session, including its low-space game deletion policy.
    put(root, 'usr/local/bin/piano-gamescope-session',
        '#!/bin/sh\nexec gamescope -e -- steam -steamdeck -gamepadui\n', 0o755)
    put(root, 'usr/share/wayland-sessions/gamescope-wayland.desktop',
        '[Desktop Entry]\nName=Steam Gaming Mode\n'
        'Exec=/usr/local/bin/piano-gamescope-session\n'
        'Type=Application\nDesktopNames=gamescope\n')
    # KDE is the initial session until the tablet's gaming session is validated.
    # Users can select Gaming Mode in SDDM without changing the boot firmware.
    configure_kde(root, subprocess.run)
    put(root, 'etc/locale.conf', 'LANG=zh_CN.UTF-8\n')
    put(root, 'etc/hostname', 'piano\n')
    put(root, 'etc/sudoers.d/20-piano', '%wheel ALL=(ALL:ALL) ALL\n', 0o440)
    resolver = root / 'etc/resolv.conf'
    resolver.unlink(missing_ok=True)
    resolver.symlink_to('/run/systemd/resolve/stub-resolv.conf')
    backlight = root / 'etc/systemd/system/systemd-backlight@.service'
    if backlight.is_symlink() and str(backlight.readlink()) == '/dev/null':
        backlight.unlink()
    put(root, 'etc/systemd/logind.conf.d/20-piano.conf',
        '[Login]\nHandlePowerKey=ignore\nHandleLidSwitch=suspend\n')
    # The standard distribution desktop owns power-button policy.
    for directory in ('etc/environment.d', 'etc/profile.d'):
        for file in (root / directory).glob('*'):
            if file.is_file() and not file.is_symlink():
                text = file.read_text(errors='replace')
                if '/usr/share/deckard/mesavars.sh' in text:
                    file.unlink()
    return {'initial_session': 'plasma', 'gaming_session': 'gamescope',
            'frame_units_disabled': list(FRAME_UNITS),
            'steam_first_launch_requires_network': True,
            'gaming_session_device_verified': False}
