# SPDX-FileCopyrightText: 2025 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import bpy

sys.path.append(os.path.dirname(__file__))
import overlay_common

bpy.context.window.workspace = bpy.data.workspaces['Test']

ob = bpy.context.active_object
space = bpy.data.screens["Default"].areas[0].spaces[0]

permutations = overlay_common.ob_modes_permutations(ob, space)

overlay_common.run_test(permutations)
