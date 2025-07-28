# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys
import bpy
import time

# Get the cycles test directory
filepath = bpy.data.filepath
blend_files_dir = os.path.dirname(filepath)
test_dir = os.path.dirname(__file__)

# Get the filename without ending
basename = os.path.basename(filepath)
filename = os.path.splitext(basename)[0]


def write_log_file(seconds, dir, blend_file, shading):
    rev = get_blender_revision()
    seconds = round(seconds, 2)
    path = "%s/test_renders/log.txt" % dir

    try:
        logfile = open(path, "a")
        try:
            logfile.write('%s - %s - %s - %s seconds\n' % (blend_file, rev, shading, seconds))
        finally:
            logfile.close()
    except IOError:
        pass


def is_background():
    return bpy.app.background


def get_blender_revision():
    return bpy.app.build_revision


def set_shading_system(scene, system):
    if system == 'svm':
        scene.cycles['shading_system'] = 0
    elif system == 'osl':
        scene.cycles['shading_system'] = 1


def render(shading_system):
    scene = bpy.context.scene

    if shading_system == "":
        scene.render.filepath = "%s/test_renders/%s" % (test_dir, filename)
    else:
        set_shading_system(scene, shading_system)
        scene.render.filepath = "%s/test_renders/%s_%s" % (test_dir, filename, shading_system)

    scene.render.image_settings.media_type = 'IMAGE'
    scene.render.image_settings.file_format = 'PNG'
    scene.render.use_file_extension = True

    start_time = time.time()
    bpy.ops.render.render(write_still=True)
    end_time = time.time()
    write_log_file((end_time - start_time), test_dir, basename, shading_system)


def main():
    # Only run in background mode
    if not is_background():
        return

    if filepath.find("cycles") == -1:
        render("")
    else:
        render("svm")
        render("osl")
