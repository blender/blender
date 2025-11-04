# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Add directory with module to the path.
import sys
sys.path.append(sys.argv[1])

import bpy


def _update_handler(self, context):
    pass


def main():
    # This used to cause a memory leak and crash due to missing `bpy/__init__.so` cleanup,
    # something that CPython doesn't guarantee. See #148959.
    bpy.types.Object.custom_property = bpy.props.FloatProperty(update=_update_handler)


if __name__ == "__main__":
    main()
