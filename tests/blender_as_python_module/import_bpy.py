# SPDX-FileCopyrightText: 2002-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Add directory with module to the path.
import sys
sys.path.append(sys.argv[1])

# Just import `bpy` and see if there are any dynamic loader errors.
import bpy

# Try bundled libraries.
bpy.utils.expose_bundled_modules()

from pxr import Usd
import MaterialX
import OpenImageIO
import PyOpenColorIO

# Test both old and new names, remove when all 4.4 libs have landed.
try:
    import pyopenvdb
except ModuleNotFoundError:
    import openvdb
    import oslquery
