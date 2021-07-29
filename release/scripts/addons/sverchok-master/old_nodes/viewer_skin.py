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
import random
import re

import bpy
from bpy.props import BoolProperty, StringProperty, FloatProperty, IntProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.utils.sv_viewer_utils import greek_alphabet

def matrix_sanitizer(matrix):
    #  reduces all values below threshold (+ or -) to 0.0, to avoid meaningless
    #  wandering floats.
    coord_strip = lambda c: 0.0 if (-1.6e-5 <= c <= 1.6e-5) else c
    san = lambda v: Vector((coord_strip(c) for c in v[:]))
    return Matrix([san(v) for v in matrix])


def default_mesh(name):
    verts = [(1, 1, -1), (1, -1, -1), (-1, -1, -1)]
    faces = [(0, 1, 2)]

    mesh_data = bpy.data.meshes.new(name)
    mesh_data.from_pydata(verts, [], faces)
    mesh_data.update()
    return mesh_data


def assign_empty_mesh():
    meshes = bpy.data.meshes
    mt_name = 'empty_skin_mesh_sv'
    if mt_name in meshes:
        return meshes[mt_name]
    else:
        return meshes.new(mt_name)


def force_pydata(mesh, verts, edges):
    mesh.vertices.add(len(verts))
    f_v = list(itertools.chain.from_iterable(verts))
    mesh.vertices.foreach_set('co', f_v)
    mesh.update()

    mesh.edges.add(len(edges))
    f_e = list(itertools.chain.from_iterable(edges))
    mesh.edges.foreach_set('vertices', f_e)
    mesh.update(calc_edges=True)


def make_bmesh_geometry(node, context, geometry):
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    verts, edges, matrix = geometry
    name = node.basemesh_name

    # remove object
    if name in objects:
        obj = objects[name]
        # assign the object an empty mesh, this allows the current mesh
        # to be uncoupled and removed from bpy.data.meshes
        obj.data = assign_empty_mesh()

        # remove uncoupled mesh, and add it straight back.
        if name in meshes:
            meshes.remove(meshes[name])
        mesh = meshes.new(name)
        obj.data = mesh
    else:
        # this is only executed once, upon the first run.
        mesh = meshes.new(name)
        obj = objects.new(name, mesh)
        scene.objects.link(obj)

    # at this point the mesh is always fresh and empty
    force_pydata(obj.data, verts, edges)
    obj.update_tag(refresh={'OBJECT', 'DATA'})
    context.scene.update()

    if node.live_updates:
        # if modifier present, remove
        if 'sv_skin' in obj.modifiers:
            sk = obj.modifiers['sv_skin']
            obj.modifiers.remove(sk)

        if 'sv_subsurf' in obj.modifiers:
            sd = obj.modifiers['sv_subsurf']
            obj.modifiers.remove(sd)

        a = obj.modifiers.new(type='SKIN', name='sv_skin')
        b = obj.modifiers.new(type='SUBSURF', name='sv_subsurf')
        b.levels = node.levels
        b.render_levels = node.render_levels

    if matrix:
        matrix = matrix_sanitizer(matrix)
        obj.matrix_local = matrix
    else:
        obj.matrix_local = Matrix.Identity(4)

class SvSkinmodViewOp(bpy.types.Operator):

    bl_idname = "node.sv_callback_skinmod_viewer"
    bl_label = "Sverchok skinmod general callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def skin_ops(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        if type_op == 'add_material':
            mat = bpy.data.materials.new('sv_material')
            mat.use_nodes = True
            mat.use_fake_user = True  # usually handy
            n.material = mat.name

    def execute(self, context):
        self.skin_ops(context, self.fn_name)
        return {'FINISHED'}




class SkinViewerNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SkinViewerNode'
    bl_label = 'Skin Mesher'
    bl_icon = 'OUTLINER_OB_EMPTY'

    activate = BoolProperty(
        name='Show',
        description='When enabled this will process incoming data',
        default=True,
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        update=updateNode,
        description="sets which base name the object will use, "
        "use N-panel to pick alternative random names")

    live_updates = BoolProperty(
        default=0,
        update=updateNode,
        description="This auto updates the modifier (by removing and adding it)")

    general_radius = FloatProperty(
        name='general_radius',
        default=0.25,
        description='value used to uniformly set the radii of skin vertices',
        min=0.0001, step=0.05,
        update=updateNode)

    levels = IntProperty(min=0, default=1, max=3, update=updateNode)
    render_levels = IntProperty(min=0, default=1, max=3, update=updateNode)

    remove_doubles = BoolProperty(
        default=0,
        name='remove doubles',
        description="removes coinciding verts, also aims to remove double radii data",
        update=updateNode)

    material = StringProperty(default='', update=updateNode)

    # https://github.com/nortikin/sverchok/blob/master/nodes/basic_view/viewer_bmesh_mk2.py

    def sv_init(self, context):
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'edges')
        self.inputs.new('MatrixSocket', 'matrix')
        self.inputs.new('StringsSocket', 'radii').prop_name = "general_radius"

    def draw_buttons(self, context, layout):
        view_icon = 'MOD_ARMATURE' if self.activate else 'ARMATURE_DATA'

        r = layout.row(align=True)
        r.prop(self, "activate", text="", toggle=True, icon=view_icon)
        r.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_MESH')

        r2 = layout.row(align=True)
        r2.prop(self, "live_updates", text="Live Modifier", toggle=True)

        layout.label('SubSurf')
        r3 = layout.row(align=True)
        r3.prop(self, 'levels', text="View")
        r3.prop(self, 'render_levels', text="Render")

        layout.label('extras')
        r4 = layout.row(align=True)
        r4.prop(self, 'remove_doubles', text='rm doubles')

        sh = "node.sv_callback_skinmod_viewer"
        r5 = layout.row(align=True)
        r5.prop_search(
            self, 'material', bpy.data, 'materials', text='',
            icon='MATERIAL_DATA')
        r5.operator(sh, text='', icon='ZOOMIN').fn_name = 'add_material'

    def get_geometry_from_sockets(self):
        i = self.inputs
        mverts = i['vertices'].sv_get(default=[])[0]
        medges = i['edges'].sv_get(default=[])[0]
        mmtrix = i['matrix'].sv_get(default=[[]])[0]
        return mverts, medges, mmtrix

    def process(self):
        if not self.activate:
            return

        # only interested in the first
        geometry = self.get_geometry_from_sockets()
        make_bmesh_geometry(self, bpy.context, geometry)

        if not self.live_updates:
            return

        # assign radii after creation
        obj = bpy.data.objects[self.basemesh_name]
        i = self.inputs
        ntimes = len(geometry[0])
        if i['radii'].is_linked:
            radii = i['radii'].sv_get()[0]
            radii, delete = match_long_repeat([radii,geometry[0]])
            # perhaps extend to fullList if given list length doesn't match.
            # maybe also indicate this failure somehow in the UI?
        else:
            radii = list(itertools.repeat(self.general_radius, ntimes))

        # for now don't update unless
        if len(radii) == len(geometry[0]):
            f_r = list(itertools.chain(*zip(radii, radii)))
            obj.data.skin_vertices[0].data.foreach_set('radius', f_r)

            # set all to root
            all_yes = list(itertools.repeat(True, ntimes))
            obj.data.skin_vertices[0].data.foreach_set('use_root', all_yes)

        # truthy if self.material is in .materials
        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials([obj])

    def set_corresponding_materials(self, objs):
        for obj in objs:
            obj.active_material = bpy.data.materials[self.material]

def register():
    bpy.utils.register_class(SvSkinmodViewOp)
    bpy.utils.register_class(SkinViewerNode)


def unregister():
    bpy.utils.unregister_class(SkinViewerNode)
    bpy.utils.unregister_class(SvSkinmodViewOp)
