#!/usr/bin/env python3
"""Replace the Frame device/session policy with Piano's KDE and Gamescope session."""
import shutil
import subprocess
from pathlib import Path

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
    'home.mount', 'set-bluetooth-mac-address.service', 'set-wifi-mac-address.service',
)


def platform_units(root):
    # Merely masking the offload target is insufficient: RequiresMountsFor
    # can pull individual mounts into login, polkit and temporary-file setup.
    # Piano keeps these directories on its ordinary writable root filesystem.
    offload = tuple(
        path.name for path in sorted((root / 'usr/lib/systemd/system').glob('*.mount'))
        if any(line.startswith('What=/home/.steamos/offload/')
               for line in path.read_text().splitlines()))
    return FRAME_UNITS + offload


def configure_platform(root):
    units = platform_units(root)
    for unit in units:
        link = root / 'etc/systemd/system' / unit
        link.unlink(missing_ok=True)
        link.symlink_to('/dev/null')
    # The native DRM/KMS path is Wayland. The Frame root also contains Xorg
    # policy for its own displays; use KWin for SDDM as well as the session.
    put(root, 'etc/sddm.conf.d/20-piano-wayland.conf',
        '[General]\nDisplayServer=wayland\n'
        'GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell\n'
        '[Wayland]\n'
        'CompositorCommand=kwin_wayland --drm --no-lockscreen --no-global-shortcuts --locale1\n')
    # Valve's plasma.desktop starts its VR Gamescope target, which then asks
    # SteamVR to present Plasma inside the headset. Select native Plasma via
    # SDDM's standard local-session override instead.
    put(root, 'usr/local/share/wayland-sessions/plasma.desktop',
        '[Desktop Entry]\nName=Plasma (Wayland)\n'
        'Exec=/usr/bin/startplasma-wayland\nTryExec=/usr/bin/startplasma-wayland\n'
        'Type=Application\nDesktopNames=KDE\n')
    for unit in ('steamvr.service', 'steamvr-logs.service', 'steamvr-plasma.service',
                 'steamvr-proxmicmute.service', 'steamvr-v4l2cam.service',
                 'steamvr-nested-desktop.service'):
        link = root / 'etc/systemd/user' / unit
        link.parent.mkdir(parents=True, exist_ok=True)
        link.unlink(missing_ok=True)
        link.symlink_to('/dev/null')
    autostart = root / 'etc/xdg/autostart/steam.desktop'
    if autostart.is_file():
        text = autostart.read_text()
        autostart.write_text('\n'.join(
            'Exec=/usr/bin/steam -silent %U' if line.startswith('Exec=') else line
            for line in text.splitlines()) + '\n')
    # CPU policy is already configured by Piano. Do not load Valve's LAVD
    # sched_ext scheduler over the selected BORE/EEVDF kernel scheduler.
    put(root, 'etc/default/steamos-scheduler', 'GOVERNOR=schedutil\nSCHEDULER=none\n')
    bsp = Path(__file__).resolve().parents[1] / 'linux/bsp/steamos'
    put(root, 'usr/share/steamos-manager/devices/xiaomi-piano.toml',
        (bsp / 'xiaomi-piano.toml').read_text())
    return units


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
    units = configure_platform(root)
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
    bsp = Path(__file__).resolve().parents[1] / 'linux/bsp/steamos'
    put(root, 'usr/local/bin/piano-gamescope-session',
        (bsp / 'piano-gamescope-session').read_text(), 0o755)
    put(root, 'usr/share/wayland-sessions/gamescope-wayland.desktop',
        '[Desktop Entry]\nName=Steam Gaming Mode\n'
        'Exec=/usr/local/bin/piano-gamescope-session\n'
        'Type=Application\nDesktopNames=gamescope\n')
    configure_kde(root, subprocess.run)
    # SteamOS Manager writes a later temporary SDDM session override when
    # switching modes. Relogin lets SDDM follow it after the old session exits.
    put(root, 'etc/sddm.conf.d/10-sunuefi.conf',
        '[Autologin]\nUser=piano\nSession=gamescope-wayland.desktop\nRelogin=true\n')
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
    return {'initial_session': 'gamescope', 'desktop_session': 'plasma',
            'gaming_session': 'gamescope',
            'frame_units_disabled': list(units),
            'steam_first_launch_requires_network': True,
            'gaming_session_device_verified': False}
