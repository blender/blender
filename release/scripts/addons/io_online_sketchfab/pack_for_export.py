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

# This script is called from the sketchfab addon directly
# to pack and save the file from a blender instance
# so that the users file is left untouched.

import os
import bpy
import json
import sys


SKETCHFAB_EXPORT_DATA_FILENAME = 'sketchfab-export-data.json'

SKETCHFAB_EXPORT_TEMP_DIR = sys.argv[-1]
SKETCHFAB_EXPORT_DATA_FILE = os.path.join(
    bpy.utils.user_resource('SCRIPTS'),
    "presets",
    SKETCHFAB_EXPORT_DATA_FILENAME,
    )


# save a copy of the current blendfile
def save_blend_copy():
    import time

    filepath = SKETCHFAB_EXPORT_TEMP_DIR
    filename = time.strftime("Sketchfab_%Y_%m_%d_%H_%M_%S.blend",
                             time.localtime(time.time()))
    filepath = os.path.join(filepath, filename)
    bpy.ops.wm.save_as_mainfile(filepath=filepath,
                                compress=True,
                                copy=True)
    size = os.path.getsize(filepath)

    return (filepath, filename, size)


# change visibility statuses and pack images
def prepare_assets(export_settings):
    hidden = set()
    images = set()
    if (export_settings['models'] == 'SELECTION' or
        export_settings['lamps'] != 'ALL'):

        for ob in bpy.data.objects:
            if ob.type == 'MESH':
                for mat_slot in ob.material_slots:
                    if not mat_slot.material:
                        continue
                    for tex_slot in mat_slot.material.texture_slots:
                        if not tex_slot:
                            continue
                        tex = tex_slot.texture
                        if tex.type == 'IMAGE':
                            image = tex.image
                            if image is not None:
                                images.add(image)
            if ((export_settings['models'] == 'SELECTION' and ob.type == 'MESH') or
                (export_settings['lamps'] == 'SELECTION' and ob.type == 'LAMP')):

                if not ob.select and not ob.hide:
                    ob.hide = True
                    hidden.add(ob)
            elif export_settings['lamps'] == 'NONE' and ob.type == 'LAMP':
                if not ob.hide:
                    ob.hide = True
                    hidden.add(ob)

    for img in images:
        if not img.packed_file:
            try:
                img.pack()
            except:
                # can fail in rare cases
                import traceback
                traceback.print_exc()


def prepare_file(export_settings):
    prepare_assets(export_settings)
    return save_blend_copy()


def read_settings():
    with open(SKETCHFAB_EXPORT_DATA_FILE, 'r') as s:
        return json.load(s)


def write_result(filepath, filename, size):
    with open(SKETCHFAB_EXPORT_DATA_FILE, 'w') as s:
        json.dump({
                'filepath': filepath,
                'filename': filename,
                'size': size,
                }, s)


if __name__ == "__main__":
    try:
        export_settings = read_settings()
        filepath, filename, size = prepare_file(export_settings)
        write_result(filepath, filename, size)
    except:
        import traceback
        traceback.print_exc()

        sys.exit(1)

