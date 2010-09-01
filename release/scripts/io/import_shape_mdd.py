# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

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


def mdd_import(filepath, ob, scene, PREF_START_FRAME=0, PREF_JUMP=1):

    print('\n\nimporting mdd "%s"' % filepath)

    bpy.ops.object.mode_set(mode='OBJECT')

    file = open(filepath, 'rb')
    frames, points = unpack(">2i", file.read(8))
    time = unpack((">%df" % frames), file.read(frames * 4))

    print('\tpoints:%d frames:%d' % (points, frames))

    # If target object doesn't have Basis shape key, create it.
    try:
        num_keys = len(ob.data.shape_keys.keys)
    except:
        basis = ob.add_shape_key()
        basis.name = "Basis"
        ob.data.update()

    scene.frame_current = PREF_START_FRAME

    def UpdateMesh(ob, fr):

        # Insert new shape key
        new_shapekey = ob.add_shape_key()
        new_shapekey.name = ("frame_%.4d" % fr)
        new_shapekey_name = new_shapekey.name

        ob.active_shape_key_index = len(ob.data.shape_keys.keys)-1
        index = len(ob.data.shape_keys.keys)-1
        ob.show_shape_key = True

        verts = ob.data.shape_keys.keys[len(ob.data.shape_keys.keys)-1].data


        for v in verts: # 12 is the size of 3 floats
            v.co[:] = unpack('>3f', file.read(12))
        #me.update()
        ob.show_shape_key = False


        # insert keyframes
        shape_keys = ob.data.shape_keys

        scene.frame_current -= 1
        ob.data.shape_keys.keys[index].value = 0.0
        shape_keys.keys[len(ob.data.shape_keys.keys)-1].keyframe_insert("value")

        scene.frame_current += 1
        ob.data.shape_keys.keys[index].value = 1.0
        shape_keys.keys[len(ob.data.shape_keys.keys)-1].keyframe_insert("value")

        scene.frame_current += 1
        ob.data.shape_keys.keys[index].value = 0.0
        shape_keys.keys[len(ob.data.shape_keys.keys)-1].keyframe_insert("value")

        ob.data.update()


    for i in range(frames):
        UpdateMesh(ob, i)


from bpy.props import *
from io_utils import ImportHelper


class importMDD(bpy.types.Operator, ImportHelper):
    '''Import MDD vertex keyframe file to shape keys'''
    bl_idname = "import_shape.mdd"
    bl_label = "Import MDD"

    filename_ext = ".mdd"
    frame_start = IntProperty(name="Start Frame", description="Start frame for inserting animation", min=-300000, max=300000, default=0)

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        if not self.properties.filepath:
            raise Exception("filename not set")

        mdd_import(self.properties.filepath, bpy.context.active_object, context.scene, self.properties.frame_start, 1)

        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(importMDD.bl_idname, text="Lightwave Point Cache (.mdd)")


def register():
    bpy.types.INFO_MT_file_import.append(menu_func)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func)

if __name__ == "__main__":
    register()
