# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import sys, os
import subprocess

import bpy

def generate(filename, external=True):
    if external:
        process = subprocess.Popen(
            [bpy.app.binary_path,
             "-b",
             "-y",
             "-noaudio",
             "-P", __file__,
             "--",
             filename,
             ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            )
        while process.poll() is None:
            process.stdout.read(1024) # empty buffer to be sure
        process.stdout.read()

        return _thumbname(filename)
    else:
        return _internal(filename)

def _thumbname(filename):
    root = os.path.splitext(filename)[0]
    return root + ".jpg"

def _internal(filename):
    imagename = os.path.split(filename)[1]
    thumbname = _thumbname(filename)

    if os.path.exists(thumbname):
        return thumbname

    if bpy:
        scene = bpy.data.scenes[0] # FIXME, this is dodgy!
        scene.render.image_settings.file_format = "JPEG"
        scene.render.image_settings.quality = 90

        # remove existing image, if there's a leftover (otherwise open changes the name)
        if imagename in bpy.data.images:
            img = bpy.data.images[imagename]
            bpy.data.images.remove(img)

        bpy.ops.image.open(filepath=filename)
        img = bpy.data.images[imagename]

        img.save_render(thumbname, scene=scene)

        img.user_clear()
        bpy.data.images.remove(img)

        try:
            process = subprocess.Popen(["convert", thumbname, "-resize", "300x300", thumbname])
            process.wait()
            return thumbname
        except Exception as exp:
            print("Error while generating thumbnail")
            print(exp)

    return None

if __name__ == "__main__":
    try:
        start = sys.argv.index("--") + 1
    except ValueError:
        start = 0
    for filename in sys.argv[start:]:
        generate(filename, external=False)
