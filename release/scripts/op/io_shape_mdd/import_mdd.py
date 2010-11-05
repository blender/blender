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


def load(operator, context, filepath, frame_start=0, frame_step=1):
    
    scene = context.scene
    obj = context.object
    
    print('\n\nimporting mdd %r' % filepath)

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT')

    file = open(filepath, 'rb')
    frames, points = unpack(">2i", file.read(8))
    time = unpack((">%df" % frames), file.read(frames * 4))

    print('\tpoints:%d frames:%d' % (points, frames))

    # If target object doesn't have Basis shape key, create it.
    try:
        num_keys = len(obj.data.shape_keys.keys)
    except:
        basis = obj.add_shape_key()
        basis.name = "Basis"
        obj.data.update()

    scene.frame_current = frame_start

    def UpdateMesh(ob, fr):

        # Insert new shape key
        new_shapekey = obj.add_shape_key()
        new_shapekey.name = ("frame_%.4d" % fr)
        new_shapekey_name = new_shapekey.name

        obj.active_shape_key_index = len(obj.data.shape_keys.keys)-1
        index = len(obj.data.shape_keys.keys)-1
        obj.show_only_shape_key = True

        verts = obj.data.shape_keys.keys[len(obj.data.shape_keys.keys)-1].data


        for v in verts: # 12 is the size of 3 floats
            v.co[:] = unpack('>3f', file.read(12))
        #me.update()
        obj.show_only_shape_key = False


        # insert keyframes
        shape_keys = obj.data.shape_keys

        scene.frame_current -= 1
        obj.data.shape_keys.keys[index].value = 0.0
        shape_keys.keys[len(obj.data.shape_keys.keys)-1].keyframe_insert("value")

        scene.frame_current += 1
        obj.data.shape_keys.keys[index].value = 1.0
        shape_keys.keys[len(obj.data.shape_keys.keys)-1].keyframe_insert("value")

        scene.frame_current += 1
        obj.data.shape_keys.keys[index].value = 0.0
        shape_keys.keys[len(obj.data.shape_keys.keys)-1].keyframe_insert("value")

        obj.data.update()


    for i in range(frames):
        UpdateMesh(obj, i)

    return {'FINISHED'}
