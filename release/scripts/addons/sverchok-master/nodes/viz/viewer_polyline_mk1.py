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
from bpy.props import (BoolProperty, StringProperty, FloatProperty, IntProperty)

from mathutils import Matrix, Vector

from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.utils.geom import multiply_vectors
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect, fullList, updateNode

from sverchok.utils.sv_viewer_utils import (
    matrix_sanitizer, remove_non_updated_objects, get_children,
    natural_plus_one, get_random_init, greek_alphabet)


def get_obj_curve(obj_index, node):
    curves = bpy.data.curves
    objects = bpy.data.objects
    scene = bpy.context.scene

    curve_name = node.basemesh_name + '.' + str("%04d" % obj_index)

    # if curve data exists, pick it up else make new curve
    cu = curves.get(curve_name)
    if not cu:
        cu = curves.new(name=curve_name, type='CURVE')

    # if object reference exists, pick it up else make a new one
    obj = objects.get(curve_name)
    if not obj:
        obj = objects.new(curve_name, cu)
        obj['basename'] = node.basemesh_name
        obj['idx'] = obj_index
        scene.objects.link(obj)

    # break down existing splines entirely.
    if cu.splines:
        cu.splines.clear()

    return obj, cu


def set_bevel_object(node, cu, obj_index):
    # use bevel object if provided
    bevel_objs = node.inputs['bevel object'].sv_get(default=[])
    if bevel_objs:
        obj_ref = bevel_objs[obj_index] if obj_index < len(bevel_objs) else bevel_objs[-1]
        if obj_ref.type == 'CURVE':
            cu.bevel_object = obj_ref
            cu.use_fill_caps = node.caps
    else:
        cu.bevel_object = None
        cu.use_fill_caps = False    


# -- POLYLINE --
def live_curve(obj_index, node, verts, radii, twist):

    obj, cu = get_obj_curve(obj_index, node)

    obj.show_wire = node.show_wire
    cu.bevel_depth = node.depth
    cu.bevel_resolution = node.resolution
    cu.dimensions = node.curve_dimensions
    if cu.dimensions == '2D':
        cu.fill_mode = 'FRONT'
    else:
        cu.fill_mode = 'FULL'

    set_bevel_object(node, cu, obj_index)

    kind = ["POLY", "NURBS"][bool(node.bspline)]

    if node.selected_mode == 'Multi':
        verts = [verts]
        radii = [radii]
        twist = [twist]

    for idx, (VERTS, RADII, TWIST) in enumerate(zip(verts, radii, twist)):

        full_flat = []
        for v in VERTS:
            full_flat.extend([v[0], v[1], v[2], 1.0])

        polyline = cu.splines.new(kind)
        polyline.points.add(len(VERTS)-1)
        polyline.points.foreach_set('co', full_flat)

        if RADII:
            if len(VERTS) < len(RADII):
                RADII = RADII[:len(VERTS)]
            elif len(VERTS) > len(RADII):
                fullList(RADII, len(VERTS))
            polyline.points.foreach_set('radius', RADII)

        if TWIST:
            if len(VERTS) < len(TWIST):
                TWIST = TWIST[:len(VERTS)]
            elif len(VERTS) > len(TWIST):
                fullList(TWIST, len(VERTS))
            polyline.points.foreach_set('tilt', TWIST)
            
        if node.close:
            cu.splines[idx].use_cyclic_u = True

        if node.bspline:
            polyline.order_u = len(polyline.points)-1

        polyline.use_smooth = node.use_smooth

    return obj



def make_curve_geometry(obj_index, node, verts, matrix, radii, twist):
    sv_object = live_curve(obj_index, node, verts, radii, twist)
    sv_object.hide_select = False

    if matrix:
        matrix = matrix_sanitizer(matrix)
        sv_object.matrix_local = matrix
    else:
        sv_object.matrix_local = Matrix.Identity(4)

    return sv_object

# could be imported from bmeshviewr directly, it's almost identical
class SvPolylineViewOpMK1(bpy.types.Operator):

    bl_idname = "node.sv_callback_polyline_viewer_mk1"
    bl_label = "Sverchok polyline showhide"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def dispatch(self, context, type_op):
        n = context.node
        objs = get_children(n, kind='CURVE')

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
class SvPolylineViewerNodeMK1(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvPolylineViewerNodeMK1'
    bl_label = 'Polyline Viewer MK1'
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

    mode_options = [(k, k, '', i) for i, k in enumerate(["Multi", "Single"])]
    selected_mode = bpy.props.EnumProperty(
        items=mode_options,
        description="offers joined of unique curves",
        default="Multi", update=updateNode
    )

    dimension_modes = [(k, k, '', i) for i, k in enumerate(["3D", "2D"])]
    
    curve_dimensions = bpy.props.EnumProperty(
        items=dimension_modes, update=updateNode,
        description="2D or 3D curves", default="3D"
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

    radii = FloatProperty(min=0, default=0.2, update=updateNode)
    twist = FloatProperty(default=0.0, update=updateNode)
    caps = BoolProperty(update=updateNode)
    show_wire = BoolProperty(update=updateNode)
    use_smooth = BoolProperty(default=True, update=updateNode)


    def copy(self, other):
        self.basemesh_name = get_random_init()

    def sv_init(self, context):
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('MatrixSocket', 'matrix')
        self.inputs.new('StringsSocket', 'radii').prop_name = 'radii'
        self.inputs.new('StringsSocket', 'twist').prop_name = 'twist'
        self.inputs.new('SvObjectSocket', 'bevel object')
        self.outputs.new('SvObjectSocket', "object")

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
        sh = 'node.sv_callback_polyline_viewer_mk1'

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

        layout.row().prop(self, 'curve_dimensions', expand=True)

        col = layout.column()
        if self.curve_dimensions == '3D':
            r1 = col.row(align=True)
            r1.prop(self, 'depth', text='radius')
            r1.prop(self, 'resolution', text='subdiv')
        row = col.row(align=True)
        row.prop(self, 'bspline', text='bspline', toggle=True)
        row.prop(self, 'close', text='close', toggle=True)
        if self.inputs['bevel object'].sv_get(default=[]):
            row.prop(self, 'caps', text='caps', toggle=True)
        row = col.row(align=True)
        row.prop(self, 'show_wire', text='wire', toggle=True)
        row.prop(self, 'use_smooth', text='smooth', toggle=True)
        row.separator()
        row.prop(self, 'selected_mode', expand=True)


    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.separator()

        row = layout.row(align=True)
        sh = 'node.sv_callback_polyline_viewer_mk1'
        row.operator(sh, text='Rnd Name').fn_name = 'random_mesh_name'
        row.operator(sh, text='+Material').fn_name = 'add_material'


    def get_geometry_from_sockets(self, has_matrices):
        
        def get(socket_name):
            data = self.inputs[socket_name].sv_get(default=[])
            return dataCorrect(data)

        mverts = get('vertices')
        mradii = self.inputs['radii'].sv_get(deepcopy=False)
        mtwist = self.inputs['twist'].sv_get(deepcopy=False)
        mmtrix = get('matrix')

        # extend all non empty lists to longest of these
        maxlen = max(len(mverts), len(mmtrix))
        if has_matrices:
            fullList(mverts, maxlen)
            fullList(mmtrix, maxlen)

        if mradii:
            fullList(mradii, maxlen)
        if mtwist:
            fullList(mtwist, maxlen)

        return mverts, mradii, mtwist, mmtrix


    def process(self):
        if not self.activate:
            return

        if not (self.inputs['vertices'].is_linked):
            return

        has_matrices = self.inputs['matrix'].is_linked
        mverts, mradii, mtwist, mmatrices = self.get_geometry_from_sockets(has_matrices)

        out_objects = []
        for obj_index, Verts in enumerate(mverts):
            if not Verts:
                continue

            matrix = mmatrices[obj_index] if has_matrices else []

            if self.selected_mode == 'Multi':
                curve_args = obj_index, self, Verts, matrix, mradii[obj_index], mtwist[obj_index]
                new_obj = make_curve_geometry(*curve_args)
                out_objects.append(new_obj)
            else:
                if matrix:
                    mverts = [multiply_vectors(*mv) for mv in zip(mmatrices, mverts)]
                new_obj = make_curve_geometry(0, self, mverts, [], mradii, mtwist)
                out_objects.append(new_obj)
                break

        # warning: uses possibly undefined index..
        remove_non_updated_objects(self, obj_index, kind='CURVE')
        self.set_corresponding_materials()

        self.outputs['object'].sv_set(out_objects)


    def set_corresponding_materials(self):
        if bpy.data.materials.get(self.material):
            for obj in get_children(self, kind='CURVE'):
                obj.active_material = bpy.data.materials[self.material]


def register():
    bpy.utils.register_class(SvPolylineViewerNodeMK1)
    bpy.utils.register_class(SvPolylineViewOpMK1)


def unregister():
    bpy.utils.unregister_class(SvPolylineViewerNodeMK1)
    bpy.utils.unregister_class(SvPolylineViewOpMK1)
