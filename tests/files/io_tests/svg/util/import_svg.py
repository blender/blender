# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from pathlib import Path


def import_svg(context):
    svg_filepath = Path(bpy.data.filepath).with_suffix(".svg")
    bpy.ops.import_curve.svg(filepath=str(svg_filepath))


if __name__ == "__main__":
    import_svg(bpy.context)
