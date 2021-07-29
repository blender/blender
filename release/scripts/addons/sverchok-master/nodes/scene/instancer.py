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

import random

import bpy
import bmesh
import mathutils
from mathutils import Vector, Matrix
from bpy.props import BoolProperty, FloatVectorProperty, StringProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode, MatrixSocket
from sverchok.data_structure import dataCorrect, updateNode


def get_random_init():
    greek_alphabet = [
        'Alpha', 'Beta', 'Gamma', 'Delta',
        'Epsilon', 'Zeta', 'Eta', 'Theta',
        'Iota', 'Kappa', 'Lamda', 'Mu',
        'Nu', 'Xi', 'Omicron', 'Pi',
        'Rho', 'Sigma', 'Tau', 'Upsilon',
        'Phi', 'Chi', 'Psi', 'Omega']
    return random.choice(greek_alphabet)


def make_or_update_instance(node, obj_name, matrix):
    context = bpy.context
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    mesh_name = node.mesh_to_clone

    if not mesh_name:
        return

    if obj_name in objects:
        sv_object = objects[obj_name]
    else:
        mesh = meshes.get(mesh_name)
        sv_object = objects.new(obj_name, mesh)
        scene.objects.link(sv_object)

    # apply matrices
    if matrix:
        sv_object.matrix_local = list(zip(*matrix))
        sv_object.data.update()   # for some reason this _is_ necessary.


class SvInstancerOp(bpy.types.Operator):

    bl_idname = "node.instancer_config"
    bl_label = "Sverchok instancer op"
    bl_options = {'REGISTER', 'UNDO'}

    obj_name = StringProperty(default="")

    def execute(self, context):
        n = context.node
        named = self.obj_name

        if named == "__SV_INSTANCE_RESET__":
            n.mesh_to_clone = ""
            n.has_instance = False
        else:
            # we assume these objects have not disappeared in the mean time.
            n.mesh_to_clone = bpy.data.objects[named].data.name
            n.has_instance = True
        return {'FINISHED'}


class SvInstancerNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvInstancerNode'
    bl_label = 'Mesh instancer'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def obj_available(self, context):
        if not bpy.data.meshes:
            return [('None', 'None', "", 0)]

        objs = bpy.data.objects
        display = lambda i: (not i.name.startswith(self.basemesh_name)) and i.type == "MESH"
        sorted_named_objects = sorted([i.name for i in objs if display(i)])
        return [(name, name, "") for name in sorted_named_objects]

    objects_to_choose = EnumProperty(
        items=obj_available,
        name="Objects",
        description="Choose Object to take mesh from",
        update=updateNode)

    grouping = BoolProperty(default=False, update=updateNode)

    activate = BoolProperty(
        default=True,
        name='Show', description='Activate node?',
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        description='stores the mesh name found in the object, this mesh is instanced',
        update=updateNode)

    mesh_to_clone = StringProperty(
        default='',
        description='stores the name of the object from where to get the mesh',
        update=updateNode)

    has_instance = BoolProperty(default=False)

    def sv_init(self, context):
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "activate", text="Update")
        row.prop(self, "grouping", text="Grouped")

        cfg = "node.instancer_config"
        if not self.has_instance:
            row = layout.row(align=True)
            row.label('pick object by name')
            row = layout.row(align=True)
            row.prop(self, "objects_to_choose", '')
            row.operator(cfg, text="use").obj_name = self.objects_to_choose
        else:
            row = layout.row()
            col1 = row.column()
            col1.label(text=self.mesh_to_clone, icon='MESH_DATA')
            col2 = row.column()
            col2.scale_x = 0.3
            col2.operator(cfg, text="reset").obj_name = "__SV_INSTANCE_RESET__"

        layout.label("Object base name", icon='OUTLINER_OB_MESH')
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "basemesh_name", text="")

    def abort_processing(self):

        if not bpy.data.meshes:
            return True

        try:
            l = bpy.data.node_groups[self.id_data.name]
        except Exception as e:
            print(self.name, "cannot run during startup, press update.")
            return True

    def get_corrected_data(self, socket_name, socket_type):
        inputs = self.inputs
        socket = inputs[socket_name].links[0].from_socket
        if isinstance(socket, socket_type):
            return dataCorrect(inputs[socket_name].sv_get())
        else:
            return []

    def process(self):
        if self.abort_processing() and not self.activate:
            return

        inputs = self.inputs
        s_name, s_type = ['matrix', MatrixSocket]
        matrices = []
        if s_name in inputs and inputs[s_name].is_linked:
            matrices = self.get_corrected_data(s_name, s_type)

        if not matrices:
            return

        # we have matrices, we can process, go go go!
        for obj_index, matrix in enumerate(matrices):
            obj_name = self.basemesh_name + "_" + str(obj_index)
            make_or_update_instance(self, obj_name, matrix)

        # obj_index is now the last index found in matrices
        self.remove_non_updated_objects(obj_index, self.basemesh_name)

        if self.grouping:
            self.to_group()
        else:
            self.ungroup()

    def remove_non_updated_objects(self, obj_index, _name):
        meshes = bpy.data.meshes
        objects = bpy.data.objects

        objs = [obj for obj in objects if obj.type == 'MESH']
        objs = [obj for obj in objs if obj.name.startswith(_name)]
        objs = [obj.name for obj in objs if int(obj.name.split("_")[-1]) > obj_index]
        if not objs:
            return

        # select and finally remove all excess objects
        scene = bpy.context.scene  # fix for render mode is needed?

        for object_name in objs:
            obj = objects[object_name]
            obj.hide_select = False  # needed?
            scene.objects.unlink(obj)
            objects.remove(obj)

    def to_group(self):

        objs = bpy.data.objects
        if not (self.basemesh_name in bpy.data.groups):
            newgroup = bpy.data.groups.new(self.basemesh_name)
        else:
            newgroup = bpy.data.groups[self.basemesh_name]

        for obj in objs:
            if self.basemesh_name in obj.name:
                if obj.name not in newgroup.objects:
                    newgroup.objects.link(obj)

    def ungroup(self):
        g = bpy.data.groups.get(self.basemesh_name)
        if g:
            bpy.data.groups.remove(g)

    def update_socket(self, context):
        self.update()

    def free(self):
        self.remove_non_updated_objects(-1, self.basemesh_name)
        self.ungroup()


def register():
    bpy.utils.register_class(SvInstancerNode)
    bpy.utils.register_class(SvInstancerOp)


def unregister():
    bpy.utils.unregister_class(SvInstancerNode)
    bpy.utils.unregister_class(SvInstancerOp)
