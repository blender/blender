# SPDX-FileCopyrightText: 2002-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Add directory with module to the path.
import sys
sys.path.append(sys.argv[1])

# Just import bpy and see if there are any dynamic loader errors.
import bpy
