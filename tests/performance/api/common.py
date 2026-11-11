# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0


def normalize_device_id(device_id: str) -> str:
    """Normalize a device ID by adding _0 suffix when there is no index."""
    new_device_ids = []
    individual_device_ids = device_id.split("+")
    for id in individual_device_ids:
        parts = id.rsplit('_', 1)
        if len(parts) == 1 or not parts[1].isdigit():
            new_device_ids.append(id + '_0')
        else:
            new_device_ids.append(id)

    return "+".join(new_device_ids)
