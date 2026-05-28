# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0


def normalize_device_id(device_id: str) -> str:
    """Normalize a device ID by adding _0 suffix when there is no index."""
    parts = device_id.rsplit('_', 1)
    if len(parts) == 1 or not parts[1].isdigit():
        return device_id + '_0'
    return device_id
