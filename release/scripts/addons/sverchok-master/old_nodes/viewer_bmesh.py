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

import itertools

import bpy
from bpy.props import BoolProperty, StringProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import (
    SverchCustomTreeNode, VerticesSocket, MatrixSocket, StringsSocket)
from sverchok.data_structure import dataCorrect, fullList, updateNode, SvGetSocketAnyType
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.utils.sv_viewer_utils import (
    matrix_sanitizer,
    natural_plus_one,
    get_random_init
)


def default_mesh(name):
    verts = [(1, 1, -1), (1, -1, -1), (-1, -1, -1)]
    faces = [(0, 1, 2)]

    mesh_data = bpy.data.meshes.new(name)
    mesh_data.from_pydata(verts, [], faces)
    mesh_data.update()
    return mesh_data


def make_bmesh_geometry(node, context, name, verts, *topology):
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    edges, faces, matrix = topology

    if name in objects:
        sv_object = objects[name]
    else:
        temp_mesh = default_mesh(name)
        sv_object = objects.new(name, temp_mesh)
        scene.objects.link(sv_object)

    ''' There is overalapping code here for testing! '''

    mesh = sv_object.data
    current_count = len(mesh.vertices)
    propose_count = len(verts)
    difference = (propose_count - current_count)

    ''' With this mode you make a massive assumption about the
        constant state of geometry. Assumes the count of verts
        edges/faces stays the same, and only updates the locations

        node.fixed_verts is not suitable for initial object creation
        but if over time you find that the only change is going to be
        vertices, this mode can be switched to to increase efficiency
    '''
    if node.fixed_verts and difference == 0:
        f_v = list(itertools.chain.from_iterable(verts))
        mesh.vertices.foreach_set('co', f_v)
        mesh.update()
    else:

        ''' get bmesh, write bmesh to obj, free bmesh'''
        bm = bmesh_from_pydata(verts, edges, faces)
        bm.to_mesh(sv_object.data)
        bm.free()
        sv_object.hide_select = False

    if matrix:
        matrix = matrix_sanitizer(matrix)
        sv_object.matrix_local = matrix
    else:
        sv_object.matrix_local = Matrix.Identity(4)


class SvBmeshViewOp(bpy.types.Operator):

    bl_idname = "node.showhide_bmesh"
    bl_label = "Sverchok bmesh showhide"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def hide_unhide(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        # maybe do hash+(obj_name + treename)
        child = lambda obj: obj.type == "MESH" and obj.name.startswith(k)
        objs = list(filter(child, bpy.data.objects))

        if type_op == 'hide_view':
            for obj in objs:
                obj.hide = n.state_view
            n.state_view = not n.state_view

        elif type_op == 'hide_render':
            for obj in objs:
                obj.hide_render = n.state_render
            n.state_render = not n.state_render

        elif type_op == 'hide_select':
            for obj in objs:
                obj.hide_select = n.state_select
            n.state_select = not n.state_select

        elif type_op == 'mesh_select':
            for obj in objs:
                obj.select = n.select_state_mesh
            n.select_state_mesh = not n.select_state_mesh

        elif type_op == 'random_mesh_name':
            n.basemesh_name = get_random_init()

    def execute(self, context):
        self.hide_unhide(context, self.fn_name)
        return {'FINISHED'}


class BmeshViewerNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'BmeshViewerNode'
    bl_label = 'Bmesh Viewer Draw'
    bl_icon = 'OUTLINER_OB_EMPTY'

    activate = BoolProperty(
        name='Show',
        description='When enabled this will process incoming data',
        default=True,
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        update=updateNode,
        description='sets which base name the object will use, \
        use N-panel to pick alternative random names')

    material = StringProperty(default='', update=updateNode)
    grouping = BoolProperty(default=False)
    state_view = BoolProperty(default=True)
    state_render = BoolProperty(default=True)
    state_select = BoolProperty(default=True)
    select_state_mesh = BoolProperty(default=False)

    fixed_verts = BoolProperty(
        default=False,
        name="Fixed vertices",
        description="Use only with unchanging topology")
    autosmooth = BoolProperty(
        default=False,
        update=updateNode,
        description="This auto sets all faces to smooth shade")

    def sv_init(self, context):
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edges', 'edges')
        self.inputs.new('StringsSocket', 'faces', 'faces')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def draw_buttons(self, context, layout):
        view_icon = 'RESTRICT_VIEW_' + ('OFF' if self.activate else 'ON')
        sh = 'node.showhide_bmesh'

        def icons(button_type):
            icon = 'WARNING'
            if button_type == 'v':
                icon = 'RESTRICT_VIEW_' + ['ON', 'OFF'][self.state_view]
            elif button_type == 'r':
                icon = 'RESTRICT_RENDER_' + ['ON', 'OFF'][self.state_render]
            elif button_type == 's':
                icon = 'RESTRICT_SELECT_' + ['ON', 'OFF'][self.state_select]
            return icon

        col = layout.column(align=True)
        row = col.row(align=True)
        row.column().prop(self, "activate", text="UPD", toggle=True, icon=view_icon)

        row.operator(sh, text='', icon=icons('v')).fn_name = 'hide_view'
        row.operator(sh, text='', icon=icons('s')).fn_name = 'hide_select'
        row.operator(sh, text='', icon=icons('r')).fn_name = 'hide_render'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "grouping", text="Group", toggle=True)

        row = col.row(align=True)
        row.scale_y = 1
        row.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_MESH')

        row = col.row(align=True)
        row.scale_y = 2
        row.operator(sh, text='Select / Deselect').fn_name = 'mesh_select'
        row = col.row(align=True)
        row.scale_y = 1

        row.prop_search(
            self, 'material', bpy.data, 'materials', text='',
            icon='MATERIAL_DATA')

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.separator()

        row = layout.row(align=True)
        sh = 'node.showhide_bmesh'
        row.operator(sh, text='Random Name').fn_name = 'random_mesh_name'

        col = layout.column(align=True)
        box = col.box()
        if box:
            box.label(text="Beta options")
            box.prop(self, "fixed_verts", text="Fixed vert count")
            box.prop(self, 'autosmooth', text='smooth shade')

    def get_geometry_from_sockets(self):

        def get(socket_name):
            data = self.inputs[socket_name].sv_get(default=[])
            return dataCorrect(data)

        mverts = get('vertices')
        medges = get('edges')
        mfaces = get('faces')
        mmtrix = get('matrix')
        return mverts, medges, mfaces, mmtrix

    def get_structure(self, stype, sindex):
        if not stype:
            return []

        try:
            j = stype[sindex]
        except IndexError:
            j = []
        finally:
            return j

    def process(self):
        # perhaps if any of mverts is [] this should already fail.
        mverts, *mrest = self.get_geometry_from_sockets()

        def get_edges_faces_matrices(obj_index):
            for geom in mrest:
                yield self.get_structure(geom, obj_index)

        # extend all non empty lists to longest of mverts or *mrest
        maxlen = max(len(mverts), *(map(len, mrest)))
        fullList(mverts, maxlen)
        for idx in range(3):
            if mrest[idx]:
                fullList(mrest[idx], maxlen)

        for obj_index, Verts in enumerate(mverts):
            if not Verts:
                continue

            data = get_edges_faces_matrices(obj_index)
            mesh_name = self.basemesh_name + "_" + str(obj_index)
            make_bmesh_geometry(self, bpy.context, mesh_name, Verts, *data)

        self.remove_non_updated_objects(obj_index)

        objs = self.get_children()

        if self.grouping:
            self.to_group(objs)

        # truthy if self.material is in .materials
        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials(objs)

        if self.autosmooth:
            self.set_autosmooth(objs)

    def get_children(self):
        objects = bpy.data.objects
        objs = [obj for obj in objects if obj.type == 'MESH']
        return [o for o in objs if o.name.startswith(self.basemesh_name + "_")]

    def remove_non_updated_objects(self, obj_index):
        objs = self.get_children()
        objs = [obj.name for obj in objs if int(obj.name.split("_")[-1]) > obj_index]
        if not objs:
            return

        meshes = bpy.data.meshes
        objects = bpy.data.objects
        scene = bpy.context.scene

        # remove excess objects
        for object_name in objs:
            obj = objects[object_name]
            obj.hide_select = False
            scene.objects.unlink(obj)
            objects.remove(obj)

        # delete associated meshes
        for object_name in objs:
            meshes.remove(meshes[object_name])

    def to_group(self, objs):
        groups = bpy.data.groups
        named = self.basemesh_name

        # alias group, or generate new group and alias that
        group = groups.get(named, groups.new(named))

        for obj in objs:
            if obj.name not in group.objects:
                group.objects.link(obj)

    def set_corresponding_materials(self, objs):
        for obj in objs:
            obj.active_material = bpy.data.materials[self.material]

    def set_autosmooth(self, objs):
        for obj in objs:
            mesh = obj.data
            smooth_states = [True] * len(mesh.polygons)
            mesh.polygons.foreach_set('use_smooth', smooth_states)
            mesh.update()

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(BmeshViewerNode)
    bpy.utils.register_class(SvBmeshViewOp)


def unregister():
    bpy.utils.unregister_class(BmeshViewerNode)
    bpy.utils.unregister_class(SvBmeshViewOp)

if __name__ == '__main__':
    register()
