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
from bpy.props import (
    BoolProperty,
    StringProperty,
    FloatProperty,
    IntProperty)

from mathutils import Matrix, Vector

from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (
    dataCorrect,
    fullList,
    updateNode)

from sverchok.utils.sv_viewer_utils import (
    matrix_sanitizer,
    natural_plus_one,
    get_random_init,
    greek_alphabet)

# -- POLYLINE --
def live_curve(node, curve_name, verts, close):
    curves = bpy.data.curves
    objects = bpy.data.objects
    scene = bpy.context.scene

    # if curve data exists, pick it up else make new curve
    cu = curves.get(curve_name)
    if not cu:
        cu = curves.new(name=curve_name, type='CURVE')

    # if object reference exists, pick it up else make a new one
    obj = objects.get(curve_name)
    if not obj:
        obj = objects.new(curve_name, cu)
        scene.objects.link(obj)

    # break down existing splines entirely.
    if cu.splines:
        cu.splines.clear()

    cu.bevel_depth = node.depth
    cu.bevel_resolution = node.resolution
    cu.dimensions = '3D'
    cu.fill_mode = 'FULL'

    # and rebuild
    # for edge in edges:
    # v0, v1 = m * Vector(verts[edge[0]]), m * Vector(verts[edge[1]])
    # full_flat = [v0[0], v0[1], v0[2], 0.0, v1[0], v1[1], v1[2], 0.0]
    full_flat = []
    for v in verts:
        full_flat.extend([v[0], v[1], v[2], 1.0])

    # each spline has a default first coordinate but we need two.
    kind = ["POLY","NURBS"][bool(node.bspline)]
    polyline = cu.splines.new(kind)
    polyline.points.add(len(verts)-1)
    polyline.points.foreach_set('co', full_flat)
        
    if close:
        cu.splines[0].use_cyclic_u = True
    # for idx, v in enumerate(verts):  
    #    polyline.points[idx].co = (v[0], v[1], v[2], 1.0)

    if node.bspline:
        polyline.order_u = len(polyline.points)-1

    return obj



def make_curve_geometry(node, context, name, verts, matrix, close):

    sv_object = live_curve(node, name, verts, close)
    sv_object.hide_select = False

    if matrix:
        matrix = matrix_sanitizer(matrix)
        sv_object.matrix_local = matrix
    else:
        sv_object.matrix_local = Matrix.Identity(4)


# could be imported from bmeshviewr directly, it's almost identical
class SvPolylineViewOp(bpy.types.Operator):

    bl_idname = "node.sv_callback_polyline_viewer"
    bl_label = "Sverchok polyline showhide"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def dispatch(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        # maybe do hash+(obj_name + treename)
        child = lambda obj: obj.type == "CURVE" and obj.name.startswith(k)
        objs = list(filter(child, bpy.data.objects))

        # find a simpler way to do this :)
        if type_op in {'hide', 'hide_render', 'hide_select', 'select'}:
            for obj in objs:
                setattr(obj, type_op, getattr(n, type_op))
            setattr(n, type_op, not getattr(n, type_op))

        elif type_op == 'random_mesh_name':
            n.basemesh_name = get_random_init()

        elif type_op == 'add_material':
            mat = bpy.data.materials.new('sv_material')
            mat.use_nodes = True
            n.material = mat.name
            print(mat.name)

    def execute(self, context):
        self.dispatch(context, self.fn_name)
        return {'FINISHED'}


# should inherit from bmeshviewer, many of these methods are largely identical.
class SvPolylineViewerNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvPolylineViewerNode'
    bl_label = 'Polyline Viewer'
    bl_icon = 'MOD_CURVE'

    activate = BoolProperty(
        name='Show',
        description='When enabled this will process incoming data',
        default=True,
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        description="which base name the object will use",
        update=updateNode
    )

    material = StringProperty(default='', update=updateNode)

    hide = BoolProperty(default=True)
    hide_render = BoolProperty(default=True)
    select = BoolProperty(default=True)
    hide_select = BoolProperty(default=False)

    depth = FloatProperty(min=0.0, default=0.2, update=updateNode)
    resolution = IntProperty(min=0, default=3, update=updateNode)
    bspline = BoolProperty(default=False, update=updateNode)
    close = BoolProperty(default=False, update=updateNode)

    def sv_init(self, context):
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def icons(self, button_type):

        icon = 'WARNING'
        if button_type == 'v':
            icon = 'RESTRICT_VIEW_' + ['ON', 'OFF'][self.hide]
        elif button_type == 'r':
            icon = 'RESTRICT_RENDER_' + ['ON', 'OFF'][self.hide_render]
        elif button_type == 's':
            icon = 'RESTRICT_SELECT_' + ['ON', 'OFF'][self.select]
        return icon

    def draw_buttons(self, context, layout):
        view_icon = 'RESTRICT_VIEW_' + ('OFF' if self.activate else 'ON')
        sh = 'node.sv_callback_polyline_viewer'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.column().prop(self, "activate", text="UPD", toggle=True, icon=view_icon)

        row.operator(sh, text='', icon=self.icons('v')).fn_name = 'hide'
        row.operator(sh, text='', icon=self.icons('s')).fn_name = 'hide_select'
        row.operator(sh, text='', icon=self.icons('r')).fn_name = 'hide_render'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.scale_y = 1
        row.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_MESH')

        row = col.row(align=True)
        row.scale_y = 2
        row.operator(sh, text='Select / Deselect').fn_name = 'select'
        row = col.row(align=True)
        row.scale_y = 1

        row.prop_search(
            self, 'material', bpy.data, 'materials', text='',
            icon='MATERIAL_DATA')

        col = layout.column()
        col.prop(self, 'depth', text='depth radius')
        col.prop(self, 'resolution', text='surface resolution')
        row = col.row(align=True)
        row.prop(self, 'bspline', text='bspline')
        row.prop(self, 'close', text='close')


    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.separator()

        row = layout.row(align=True)
        sh = 'node.sv_callback_curve_viewer'
        row.operator(sh, text='Rnd Name').fn_name = 'random_mesh_name'
        row.operator(sh, text='+Material').fn_name = 'add_material'

    def get_geometry_from_sockets(self):

        def get(socket_name):
            data = self.inputs[socket_name].sv_get(default=[])
            return dataCorrect(data)

        mverts = get('vertices')
        mmtrix = get('matrix')
        return mverts, mmtrix

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
        if not (self.inputs['vertices'].is_linked):
            return

        # perhaps if any of mverts is [] this should already fail.
        has_matrices = self.inputs['matrix'].is_linked
        mverts, mmatrices = self.get_geometry_from_sockets()

        # extend all non empty lists to longest of mverts or *mrest
        maxlen = max(len(mverts), len(mmatrices))
        if has_matrices:
            fullList(mmatrices, maxlen)

        for obj_index, Verts in enumerate(mverts):
            if not Verts:
                continue

            curve_name = self.basemesh_name + "_" + str(obj_index)
            if has_matrices:
                matrix = mmatrices[obj_index]
            else:
                matrix = []

            make_curve_geometry(self, bpy.context, curve_name, Verts, matrix, self.close)

        self.remove_non_updated_objects(obj_index)
        objs = self.get_children()

        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials(objs)

    def get_children(self):
        objects = bpy.data.objects
        objs = [obj for obj in objects if obj.type == 'CURVE']
        return [o for o in objs if o.name.startswith(self.basemesh_name + "_")]

    def remove_non_updated_objects(self, obj_index):
        objs = self.get_children()
        objs = [obj.name for obj in objs if int(obj.name.split("_")[-1]) > obj_index]
        if not objs:
            return

        curves = bpy.data.curves
        objects = bpy.data.objects
        scene = bpy.context.scene

        # remove excess objects
        for object_name in objs:
            obj = objects[object_name]
            obj.hide_select = False
            scene.objects.unlink(obj)
            objects.remove(obj)

        for object_name in objs:
            curves.remove(curves[object_name])


    def set_corresponding_materials(self, objs):
        for obj in objs:
            obj.active_material = bpy.data.materials[self.material]


def register():
    bpy.utils.register_class(SvPolylineViewerNode)
    bpy.utils.register_class(SvPolylineViewOp)


def unregister():
    bpy.utils.unregister_class(SvPolylineViewerNode)
    bpy.utils.unregister_class(SvPolylineViewOp)
