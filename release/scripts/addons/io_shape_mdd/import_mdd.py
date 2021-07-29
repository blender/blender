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

# <pep8 compliant>

# mdd importer by Bill L.Nieuwendorp
# conversion to blender 2.5: Ivo Grigull (loolarge)
#
# Warning if the vertex order or vertex count differs from the
# origonal model the mdd was Baked out from their will be Strange
# behavior
#
# vertex animation to ShapeKeys with ipo  and gives the frame a value of 1.0
# A modifier to read mdd files would be Ideal but thats for another day :)
#
# Please send any fixes,updates,bugs to Slow67_at_Gmail.com
# Bill Niewuendorp

import bpy
from struct import unpack

def set_linear_interpolation(obj, shapekey):
    anim_data = obj.data.shape_keys.animation_data
    data_path = "key_blocks[\"" + shapekey.name + "\"].value"

    for fcu in anim_data.action.fcurves:
        if fcu.data_path == data_path:
            for keyframe in fcu.keyframe_points:
                keyframe.interpolation = 'LINEAR'


def obj_update_frame(file, scene, obj, start, fr, step):

    # Insert new shape key
    new_shapekey = obj.shape_key_add()
    new_shapekey.name = ("frame_%.4d" % fr)
    new_shapekey_index = len(obj.data.shape_keys.key_blocks) - 1

    obj.active_shape_key_index = new_shapekey_index
    obj.show_only_shape_key = True

    verts = new_shapekey.data

    for v in verts:  # 12 is the size of 3 floats
        v.co[:] = unpack('>3f', file.read(12))

    # me.update()
    obj.show_only_shape_key = False

    # insert keyframes
    new_shapekey = obj.data.shape_keys.key_blocks[new_shapekey_index]
    frame = start + fr*step

    new_shapekey.value = 0.0
    new_shapekey.keyframe_insert("value", frame=frame - step)

    new_shapekey.value = 1.0
    new_shapekey.keyframe_insert("value", frame=frame)

    new_shapekey.value = 0.0
    new_shapekey.keyframe_insert("value", frame=frame + step)

    set_linear_interpolation(obj, new_shapekey)

    obj.data.update()


def load(context, filepath, frame_start=0, frame_step=1):

    scene = context.scene
    obj = context.object

    print('\n\nimporting mdd %r' % filepath)

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    file = open(filepath, 'rb')
    frames, points = unpack(">2i", file.read(8))
    time = unpack((">%df" % frames), file.read(frames * 4))

    print('\tpoints:%d frames:%d' % (points, frames))
    print('\tstart frame:%d step:%d' % (frame_start, frame_step))

    # If target object doesn't have Basis shape key, create it.
    if not obj.data.shape_keys:
        basis = obj.shape_key_add()
        basis.name = "Basis"
        obj.data.update()

    for i in range(frames):
        obj_update_frame(file, scene, obj, frame_start, i, frame_step)

    return {'FINISHED'}
