# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Full path to Blender executable, autodetected when running from Blender
blender_executable = ""

# Path to OpenImageIO idiff executable, empty if in PATH
idiff_path = ""

# list of .blend files and directories with .blend files to render
files = [
    "animation",
    "gameengine_visual",
    "modifier_stack",
    "sequence_editing",
    "glsl",
    "compositing",
    "particles_and_hair",
    "libraries_and_linking",
    "physics",
    "materials",
    "rendering",
    "modeling",
    "sculpting",
    "cycles",
]

files = ["../" + f for f in files]
