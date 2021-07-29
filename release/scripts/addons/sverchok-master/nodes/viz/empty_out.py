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

import bpy
from mathutils import Matrix
from bpy.props import StringProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import node_id, Matrix_generate


class SvEmptyOutNode(bpy.types.Node, SverchCustomTreeNode):
    '''Create a blender empty object'''
    bl_idname = 'SvEmptyOutNode'
    bl_label = 'Empty out'
    bl_icon = 'OUTLINER_DATA_EMPTY'

    def rename_empty(self, context):
        empty = self.find_empty()
        if empty:
            empty.name = self.empty_name
            self.label = empty.name

    n_id = StringProperty(default='')
    empty_name = StringProperty(default='Sv empty', name="Base name",
                                description="Base name of empty",
                                update=rename_empty)
    auto_remove = BoolProperty(default=True,
                               description="Remove on node delete",
                               name="Auto delete")
    # To speed up finding the empty if many objects
    empty_ref_name = StringProperty(default='')

    def create_empty(self):
        n_id = node_id(self)
        scene = bpy.context.scene
        objects = bpy.data.objects
        empty = objects.new(self.empty_name, None)
        scene.objects.link(empty)
        scene.update()
        empty["SVERCHOK_REF"] = n_id
        self.empty_ref_name = empty.name
        return empty

    def sv_init(self, context):
        self.create_empty()
        self.inputs.new('MatrixSocket', "Matrix")
        self.outputs.new('SvObjectSocket', "Objects")

    def find_empty(self):
        n_id = node_id(self)

        def check_empty(obj):
            """ Check that it is the correct empty """
            if obj.type == 'EMPTY':
                return "SVERCHOK_REF" in obj and obj["SVERCHOK_REF"] == n_id
            return False

        objects = bpy.data.objects
        if self.empty_ref_name in objects:
            obj = objects[self.empty_ref_name]
            if check_empty(obj):
                return obj
        for obj in objects:
            if check_empty(obj):
                self.empty_ref_name = obj.name
                return obj
        return None

    def draw_buttons(self, context, layout):
        layout.label("Base name")
        row = layout.row()
        row.scale_y = 1.1
        row.prop(self, "empty_name", text="")

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, "auto_remove")

    def process(self):
        empty = self.find_empty()
        if not empty:
            empty = self.create_empty()
            print("created new empty")

        if self.inputs['Matrix'].is_linked:
            mats = self.inputs['Matrix'].sv_get()
            mat = Matrix_generate(mats)[0]
        else:
            mat = Matrix()
        self.label = empty.name
        empty.matrix_world = mat
        
        if 'Objects' in self.outputs:
            self.outputs['Objects'].sv_set([empty])

    def copy(self, node):
        self.n_id = ''
        empty = self.create_empty()
        self.label = empty.name

    def free(self):
        if self.auto_remove:
            empty = self.find_empty()
            if empty:
                scene = bpy.context.scene
                objects = bpy.data.objects
                try:
                    scene.objects.unlink(empty)
                    objects.remove(empty)
                except:
                    print("{0} failed to remove empty".format(self.name))


def register():
    bpy.utils.register_class(SvEmptyOutNode)


def unregister():
    bpy.utils.unregister_class(SvEmptyOutNode)
