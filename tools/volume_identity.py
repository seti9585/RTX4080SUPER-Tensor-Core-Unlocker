# SPDX-License-Identifier: GPL-2.0-only
"""Inspect a user-selected Windows volume; never pin a personal device GUID."""
import ctypes
from ctypes import wintypes
import os
from pathlib import Path

def query_volume(root):
    if os.name != 'nt':
        raise OSError('Deployment requires Windows volume identity APIs')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    name = kernel.GetVolumeNameForVolumeMountPointW
    name.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
    name.restype = wintypes.BOOL
    info = kernel.GetVolumeInformationW
    info.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD,
                     ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD),
                     ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, wintypes.DWORD]
    info.restype = wintypes.BOOL
    guid = ctypes.create_unicode_buffer(128)
    label = ctypes.create_unicode_buffer(256)
    filesystem = ctypes.create_unicode_buffer(64)
    serial, limit, flags = wintypes.DWORD(), wintypes.DWORD(), wintypes.DWORD()
    if not name(str(root), guid, len(guid)):
        raise ctypes.WinError(ctypes.get_last_error())
    if not info(str(root), label, len(label), ctypes.byref(serial), ctypes.byref(limit),
                ctypes.byref(flags), filesystem, len(filesystem)):
        raise ctypes.WinError(ctypes.get_last_error())
    return {'guid': guid.value[guid.value.index('{') + 1:guid.value.index('}')], 'label': label.value, 'filesystem': filesystem.value}

def inspect_target(target):
    import hashlib
    root = Path(os.path.abspath(target))
    if not root.anchor or root != Path(root.anchor):
        raise ValueError('Specify a drive root')
    system_root = Path(os.environ.get('SystemDrive', 'C:') + '/')
    if root == system_root:
        raise ValueError('System drive refused')
    actual = query_volume(root)
    system_volume = query_volume(system_root)
    if actual['guid'].casefold() == system_volume['guid'].casefold():
        raise ValueError('System volume alias refused')
    efi = root / 'EFI' / 'BOOT' / 'BOOTX64.EFI'
    if any(part.is_symlink() or part.is_junction() for part in (root / 'EFI', root / 'EFI' / 'BOOT', efi)) or not efi.is_file():
        raise ValueError('Target boot EFI missing or redirected')
    with efi.open('rb') as stream:
        actual['efi_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    actual['efi_path'] = str(efi)
    return actual

EFI_SHA256 = "b6e0ecea47b8ca2d8b17c742d3960e60016a792d0b86fa86cefc13a72a1fcb35"

def validate_target(target, confirmed_guid, expected_efi_hash=EFI_SHA256):
    import uuid
    if not confirmed_guid:
        raise ValueError('Inspect the target and explicitly confirm its volume GUID')
    actual = inspect_target(target)
    if actual['filesystem'].casefold() != 'fat32':
        raise ValueError('FAT32 required')
    if uuid.UUID(actual['guid']) != uuid.UUID(confirmed_guid.strip('{}')):
        raise ValueError('Target volume GUID changed or mismatched')
    if actual['efi_sha256'].casefold() != expected_efi_hash.casefold():
        raise ValueError('Target EFI hash mismatch')
    return actual