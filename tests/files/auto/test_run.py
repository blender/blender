# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Main imports
import bpy
import os
import sys

# Get the cycles test directory
filepath = bpy.data.filepath
test_dir = os.path.dirname(__file__)

# Append to sys.path
sys.path.append(test_dir)

# Import external script and execute
import test_utils
test_utils.main()
