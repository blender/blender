#!/usr/bin/env python3

# This script updates icons from the BLEND file
import os
import subprocess
import sys


def run(cmd):
    print("   ", " ".join(cmd))
    # Don't use check_call because asan causes nonzero exitcode :S
    subprocess.call(cmd)


def edit_text_file(filename, marker_begin, marker_end, content):
    with open(filename, 'r', encoding='utf-8') as f:
        data = f.read()
    marker_begin_index = data.find(marker_begin)
    marker_end_index = data.find(marker_end, marker_begin_index)
    # include indentation of marker
    while data[marker_end_index - 1] in {'\t', ' '}:
        marker_end_index -= 1
    if marker_begin_index == -1:
        print('Error: %r not found' % marker_begin)
        return
    if marker_end_index == -1:
        print('Error: %r not found' % marker_end)
        return
    marker_begin_index += len(marker_begin) + 1
    data_update = data[:marker_begin_index] + content + data[marker_end_index:]
    if data != data_update:
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(data_update)


BASEDIR = os.path.abspath(os.path.dirname(__file__))
ROOTDIR = os.path.normpath(os.path.join(BASEDIR, "..", ".."))

blender_bin = os.environ.get("BLENDER_BIN", "blender")
if not os.path.exists(blender_bin):
    blender_bin = os.path.join(ROOTDIR, "blender.bin")

if not os.path.exists(blender_bin):
    if sys.platform == 'darwin':
        blender_app_path = '/Applications/Blender.app/Contents/MacOS/Blender'
        if os.path.exists(blender_app_path):
            blender_bin = blender_app_path

icons_blend = (
    os.path.join(ROOTDIR, "..", "lib", "resources", "icon_geom.blend"),
)


def names_and_time_from_path(path):
    for entry in os.scandir(path):
        name = entry.name
        if name.endswith(".dat"):
            yield (name, entry.stat().st_mtime)


# Collect icons files and update CMake.
icon_files = []

# create .dat geometry (which are stored in git)
for blend in icons_blend:
    output_dir = os.path.join(BASEDIR, "icons")
    files_old = set(names_and_time_from_path(output_dir))
    cmd = (
        blender_bin, "--background", "--factory-startup", "-noaudio",
        blend,
        "--python", os.path.join(BASEDIR, "blender_icons_geom.py"),
        "--",
        "--group", "Export",
        "--output-dir", output_dir,
    )
    run(cmd)
    files_new = set(names_and_time_from_path(output_dir))

    icon_files.extend([
        name[:-4]  # no .dat
        for (name, _) in sorted((files_new - files_old))
    ])


edit_text_file(
    os.path.join(ROOTDIR, "source", "blender", "editors", "datafiles", "CMakeLists.txt"),
    "# BEGIN ICON_GEOM_NAMES",
    "# END ICON_GEOM_NAMES",
    "  " + "\n  ".join(icon_files) + "\n",
)
