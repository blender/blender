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

# MK2

import itertools

import bpy
from bpy.props import BoolProperty, StringProperty, BoolVectorProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect, fullList, updateNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.utils.sv_viewer_utils import (
    matrix_sanitizer,
    natural_plus_one,
    get_random_init,
    greek_alphabet
)


def default_mesh(name):
    verts, faces = [(1, 1, -1), (1, -1, -1), (-1, -1, -1)], [(0, 1, 2)]
    mesh_data = bpy.data.meshes.new(name)
    mesh_data.from_pydata(verts, [], faces)
    mesh_data.update()
    return mesh_data


def make_bmesh_geometry(node, idx, context, verts, *topology):
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    edges, faces, matrix = topology
    name = node.basemesh_name + "_" + str(idx)

    if name in objects:
        sv_object = objects[name]
    else:
        temp_mesh = default_mesh(name)
        sv_object = objects.new(name, temp_mesh)
        scene.objects.link(sv_object)

    # book-keeping via ID-props!? even this is can be broken by renames
    sv_object['idx'] = idx
    sv_object['madeby'] = node.name
    sv_object['basename'] = node.basemesh_name

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
        bm = bmesh_from_pydata(verts, edges, faces, normal_update=node.calc_normals)
        bm.to_mesh(sv_object.data)
        bm.free()

        sv_object.hide_select = False

    if matrix:
        matrix = matrix_sanitizer(matrix)
        if node.extended_matrix:
            sv_object.data.transform(matrix)
            sv_object.matrix_local = Matrix.Identity(4)
        else:
            sv_object.matrix_local = matrix
    else:
        sv_object.matrix_local = Matrix.Identity(4)


def make_bmesh_geometry_merged(node, idx, context, yielder_object):
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    name = node.basemesh_name + "_" + str(idx)

    if name in objects:
        sv_object = objects[name]
    else:
        temp_mesh = default_mesh(name)
        sv_object = objects.new(name, temp_mesh)
        scene.objects.link(sv_object)

    # book-keeping via ID-props!
    sv_object['idx'] = idx
    sv_object['madeby'] = node.name
    sv_object['basename'] = node.basemesh_name

    vert_count = 0
    big_verts = []
    big_edges = []
    big_faces = []

    for result in yielder_object:

        verts, topology = result
        edges, faces, matrix = topology

        if matrix:
            matrix = matrix_sanitizer(matrix)
            verts = [matrix * Vector(v) for v in verts]

        big_verts.extend(verts)
        big_edges.extend([[a + vert_count, b + vert_count] for a, b in edges])
        big_faces.extend([[j + vert_count for j in f] for f in faces])

        vert_count += len(verts)


    if node.fixed_verts and len(sv_object.data.vertices) == len(big_verts):
        mesh = sv_object.data
        f_v = list(itertools.chain.from_iterable(big_verts))
        mesh.vertices.foreach_set('co', f_v)
        mesh.update()
    else:
        ''' get bmesh, write bmesh to obj, free bmesh'''
        bm = bmesh_from_pydata(big_verts, big_edges, big_faces, normal_update=node.calc_normals)
        bm.to_mesh(sv_object.data)
        bm.free()

    sv_object.hide_select = False
    sv_object.matrix_local = Matrix.Identity(4)


class SvBmeshViewOp2(bpy.types.Operator):

    bl_idname = "node.sv_callback_bmesh_viewer"
    bl_label = "Sverchok bmesh general callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def hide_unhide(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        child = lambda obj: obj.type == "MESH" and obj.name.startswith(k)
        objs = list(filter(child, bpy.data.objects))

        if type_op in {'hide', 'hide_render', 'hide_select'}:
            op_value = getattr(n, type_op)
            for obj in objs:
                setattr(obj, type_op, op_value)
            setattr(n, type_op, not op_value)

        elif type_op == 'mesh_select':
            for obj in objs:
                obj.select = n.select_state_mesh
            n.select_state_mesh = not n.select_state_mesh

        elif type_op == 'random_mesh_name':
            n.basemesh_name = get_random_init()

        elif type_op == 'add_material':
            mat = bpy.data.materials.new('sv_material')
            mat.use_nodes = True
            mat.use_fake_user = True  # usually handy


            nodes = mat.node_tree.nodes
            n.material = mat.name
            
            if bpy.context.scene.render.engine == 'CYCLES':
                # add attr node to the left of diffuse BSDF + connect it
                diffuse_node = nodes['Diffuse BSDF']
                attr_node = nodes.new('ShaderNodeAttribute')
                attr_node.location = (-170, 300)
                attr_node.attribute_name = 'SvCol'

                links = mat.node_tree.links
                links.new(attr_node.outputs[0], diffuse_node.inputs[0])


    def execute(self, context):
        self.hide_unhide(context, self.fn_name)
        return {'FINISHED'}


class SvBmeshViewerNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    """ bmv Generate Live geom """

    bl_idname = 'SvBmeshViewerNodeMK2'
    bl_label = 'Viewer BMesh'
    bl_icon = 'OUTLINER_OB_MESH'

    # hints found at ba.org/forum/showthread.php?290106
    # - this will not allow objects on multiple layers, yet.
    def g(self):
        self['lp'] = self.get('lp', [False] * 20)
        return self['lp']

    def s(self, value):
        val = []
        for b in zip(self['lp'], value):
            val.append(b[0] != b[1])
        self['lp'] = val

    def layer_updateNode(self, context):
        '''will update in place without geometry updates'''
        for obj in self.get_children():
            obj.layers = self.layer_choice[:]

    material = StringProperty(default='', update=updateNode)
    grouping = BoolProperty(default=False)
    merge = BoolProperty(default=False, update=updateNode)

    hide = BoolProperty(default=True)
    hide_render = BoolProperty(default=True)
    hide_select = BoolProperty(default=True)

    select_state_mesh = BoolProperty(default=False)
    calc_normals = BoolProperty(default=False, update=updateNode)

    activate = BoolProperty(
        default=True,
        description='When enabled this will process incoming data',
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        update=updateNode,
        description="sets which base name the object will use, "
        "use N-panel to pick alternative random names")

    fixed_verts = BoolProperty(
        default=False,
        description="Use only with unchanging topology")

    autosmooth = BoolProperty(
        default=False,
        update=updateNode,
        description="This auto sets all faces to smooth shade")

    layer_choice = BoolVectorProperty(
        subtype='LAYER', size=20,
        update=layer_updateNode,
        description="This sets which layer objects are placed on",
        get=g, set=s)

    extended_matrix = BoolProperty(
        default=False,
        description='Allows mesh.transform(matrix) operation, quite fast!')

    def sv_init(self, context):
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edges', 'edges')
        self.inputs.new('StringsSocket', 'faces', 'faces')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

        self.outputs.new('SvObjectSocket', "Objects")

    def draw_buttons(self, context, layout):
        view_icon = 'BLENDER' if self.activate else 'ERROR'
        sh = 'node.sv_callback_bmesh_viewer'

        def icons(TYPE):
            NAMED_ICON = {
                'hide': 'RESTRICT_VIEW',
                'hide_render': 'RESTRICT_RENDER',
                'hide_select': 'RESTRICT_SELECT'}.get(TYPE)
            if not NAMED_ICON:
                return 'WARNING'
            return NAMED_ICON + ['_ON', '_OFF'][getattr(self, TYPE)]

        col = layout.column(align=True)
        row = col.row(align=True)
        row.column().prop(self, "activate", text="UPD", toggle=True, icon=view_icon)
        row.separator()
        row.operator(sh, text='', icon=icons('hide')).fn_name = 'hide'
        row.operator(sh, text='', icon=icons('hide_select')).fn_name = 'hide_select'
        row.operator(sh, text='', icon=icons('hide_render')).fn_name = 'hide_render'

        col = layout.column(align=True)
        if col:
            row = col.row(align=True)
            row.prop(self, "grouping", text="Group", toggle=True)
            row.prop(self, "merge", text="Merge", toggle=True)

            row = col.row(align=True)
            row.scale_y = 1
            row.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_MESH')

            row = col.row(align=True)
            row.scale_y = 1.62
            row.operator(sh, text='Select Toggle').fn_name = 'mesh_select'

            col.separator()
            row = col.row(align=True)
            row.scale_y = 1
            row.prop_search(
                self, 'material', bpy.data, 'materials', text='',
                icon='MATERIAL_DATA')
            row.operator(sh, text='', icon='ZOOMIN').fn_name = 'add_material'

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.separator()

        row = layout.row(align=True)
        sh = 'node.sv_callback_bmesh_viewer'
        row.operator(sh, text='Rnd Name').fn_name = 'random_mesh_name'
        row.separator()
        row.operator(sh, text='+Material').fn_name = 'add_material'

        col = layout.column(align=True)
        box = col.box()
        if box:
            box.label(text="Beta options")
            box.prop(self, "extended_matrix", text="Extended Matrix")
            box.prop(self, "fixed_verts", text="Fixed vert count")
            box.prop(self, 'autosmooth', text='smooth shade')
            box.prop(self, 'calc_normals', text='calculate normals')
            box.prop(self, 'layer_choice', text='layer')

    def get_geometry_from_sockets(self):

        def get(socket_name):
            data = self.inputs[socket_name].sv_get(default=[]) # , deepcopy=False) # can't because of fulllist.
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

        if not self.activate:
            return

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

        if self.merge:
            obj_index = 0

            def keep_yielding():
                # this will yield all in one go.
                for idx, Verts in enumerate(mverts):
                    if not Verts:
                        continue

                    data = get_edges_faces_matrices(idx)
                    yield (Verts, data)

            yielder_object = keep_yielding()
            make_bmesh_geometry_merged(self, obj_index, bpy.context, yielder_object)

        else:
            for obj_index, Verts in enumerate(mverts):
                if not Verts:
                    continue

                data = get_edges_faces_matrices(obj_index)
                make_bmesh_geometry(self, obj_index, bpy.context, Verts, *data)

        self.remove_non_updated_objects(obj_index)

        objs = self.get_children()

        if self.grouping:
            self.to_group(objs)

        # truthy if self.material is in .materials
        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials(objs)

        if self.autosmooth:
            self.set_autosmooth(objs)

        if self.outputs[0].is_linked:
            self.outputs[0].sv_set(objs)

    def get_children(self):
        objects = bpy.data.objects
        objs = [obj for obj in objects if obj.type == 'MESH']
        # critera, basename must be in object.keys and the value must be self.basemesh_name
        return [o for o in objs if o.get('basename') == self.basemesh_name]

    def remove_non_updated_objects(self, obj_index):
        objs = self.get_children()
        objs = [obj.name for obj in objs if obj['idx'] > obj_index]
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
            objects.remove(obj, do_unlink=True)

        # delete associated meshes
        for object_name in objs:
            meshes.remove(meshes[object_name])

    def to_group(self, objs):
        groups = bpy.data.groups
        named = self.basemesh_name

        # alias group, or generate new group and alias that
        group = groups.get(named)
        if not group:
            group = groups.new(named)

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
    bpy.utils.register_class(SvBmeshViewerNodeMK2)
    bpy.utils.register_class(SvBmeshViewOp2)


def unregister():
    bpy.utils.unregister_class(SvBmeshViewerNodeMK2)
    bpy.utils.unregister_class(SvBmeshViewOp2)

if __name__ == '__main__':
    register()
