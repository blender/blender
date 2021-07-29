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
from bpy.props import StringProperty, BoolProperty, FloatProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import SvGetSocketAnyType, node_id, Matrix_generate, match_long_repeat, updateNode
from sverchok.utils.sv_viewer_utils import greek_alphabet

class SvMetaballOperator(bpy.types.Operator):

    bl_idname = "node.sv_callback_metaball"
    bl_label = "Sverchok metaball general callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def hide_unhide(self, context, type_op):
        node = context.node
        metaball = node.find_metaball()

        if type_op in {'hide', 'hide_render', 'hide_select'}:
            op_value = getattr(node, type_op)
            setattr(metaball, type_op, op_value)
            setattr(node, type_op, not op_value)

        elif type_op == 'mesh_select':
            metaball.select = node.select_state
            node.select_state = not node.select_state

    def execute(self, context):
        self.hide_unhide(context, self.fn_name)
        return {'FINISHED'}


class SvMetaballOutNode(bpy.types.Node, SverchCustomTreeNode):
    '''Create Blender's metaball object'''
    bl_idname = 'SvMetaballOutNode'
    bl_label = 'Metaball'
    bl_icon = 'META_BALL'

    def rename_metaball(self, context):
        meta = self.find_metaball()
        if meta:
            meta.name = self.metaball_ref_name
            self.label = meta.name

    n_id = StringProperty(default='')
    metaball_ref_name = StringProperty(default='')

    activate = BoolProperty(
        name='Activate',
        default=True,
        description='When enabled this will process incoming data',
        update=updateNode)

    hide = BoolProperty(default=True)
    hide_render = BoolProperty(default=True)
    hide_select = BoolProperty(default=True)
    select_state = BoolProperty(default=False)

    meta_name = StringProperty(default='SvMetaBall', name="Base name",
                                description="Base name of metaball",
                                update=rename_metaball)

    meta_types = [
            ("BALL", "Ball", "Ball", "META_BALL", 1),
            ("CAPSULE", "Capsule", "Capsule", "META_CAPSULE", 2),
            ("PLANE", "Plane", "Plane", "META_PLANE", 3),
            ("ELLIPSOID", "Ellipsoid", "Ellipsoid", "META_ELLIPSOID", 4),
            ("CUBE", "Cube", "Cube", "META_CUBE", 5)
        ]

    meta_type_by_id = dict((item[4], item[0]) for item in meta_types)

    meta_type = EnumProperty(name='Meta type',
            description = "Meta object type",
            items = meta_types, update=updateNode)

    material = StringProperty(name="Material", default='', update=updateNode)

    radius = FloatProperty(
        name='Radius',
        description='Metaball radius',
        default=1.0, min=0.0, update=updateNode)

    stiffness = FloatProperty(
        name='Stiffness',
        description='Metaball stiffness',
        default=2.0, min=0.0, update=updateNode)

    view_resolution = FloatProperty(
        name='Resolution (viewport)',
        description='Resolution for viewport',
        default=0.2, min=0.0, max=1.0, update=updateNode)

    render_resolution = FloatProperty(
        name='Resolution (render)',
        description='Resolution for rendering',
        default=0.1, min=0.0, max=1.0, update=updateNode)

    threshold = FloatProperty(
        name='Threshold',
        description='Influence of meta elements',
        default=0.6, min=0.0, max=5.0, update=updateNode)

    def create_metaball(self):
        n_id = node_id(self)
        scene = bpy.context.scene
        objects = bpy.data.objects
        metaball_data = bpy.data.metaballs.new("MetaBall")
        metaball_object = bpy.data.objects.new(self.meta_name, metaball_data)
        scene.objects.link(metaball_object)
        scene.update()

        metaball_object["SVERCHOK_REF"] = n_id
        self.metaball_ref_name = metaball_object.name

        return metaball_object

    def find_metaball(self):
        n_id = node_id(self)

        def check_metaball(obj):
            """ Check that it is the correct metaball """
            if obj.type == 'META':
                return "SVERCHOK_REF" in obj and obj["SVERCHOK_REF"] == n_id
            return False

        objects = bpy.data.objects
        if self.metaball_ref_name in objects:
            obj = objects[self.metaball_ref_name]
            if check_metaball(obj):
                return obj
        for obj in objects:
            if check_metaball(obj):
                self.metaball_ref_name = obj.name
                return obj
        return None

    def get_next_name(self):
        gai = bpy.context.scene.SvGreekAlphabet_index
        bpy.context.scene.SvGreekAlphabet_index += 1
        return 'Meta_' + greek_alphabet[gai]

    def sv_init(self, context):
        self.meta_name = self.get_next_name()

        self.create_metaball()
        self.inputs.new('StringsSocket', 'Types').prop_name = "meta_type"
        self.inputs.new('MatrixSocket', 'Origins')
        self.inputs.new('StringsSocket', "Radius").prop_name = "radius"
        self.inputs.new('StringsSocket', "Stiffness").prop_name = "stiffness"
        self.inputs.new('StringsSocket', 'Negation')
        self.outputs.new('SvObjectSocket', "Objects")

    def draw_buttons(self, context, layout):
        view_icon = 'BLENDER' if self.activate else 'ERROR'
        show_hide = 'node.sv_callback_metaball'

        def icons(TYPE):
            NAMED_ICON = {
                'hide': 'RESTRICT_VIEW',
                'hide_render': 'RESTRICT_RENDER',
                'hide_select': 'RESTRICT_SELECT'}.get(TYPE)
            if not NAMED_ICON:
                return 'WARNING'
            return NAMED_ICON + ['_ON', '_OFF'][getattr(self, TYPE)]

        row = layout.row(align=True)
        row.column().prop(self, "activate", text="UPD", toggle=True, icon=view_icon)
        row.separator()

        row.operator(show_hide, text='', icon=icons('hide')).fn_name = 'hide'
        row.operator(show_hide, text='', icon=icons('hide_select')).fn_name = 'hide_select'
        row.operator(show_hide, text='', icon=icons('hide_render')).fn_name = 'hide_render'

        col = layout.column(align=True)
        col.prop(self, "meta_name", text='', icon='OUTLINER_OB_META')
        col.operator(show_hide, text='Select Toggle').fn_name = 'mesh_select'

        layout.prop_search(
                self, 'material', bpy.data, 'materials',
                text='',
                icon='MATERIAL_DATA')
        layout.prop(self, "threshold")

    def draw_buttons_ext(self, context, layout):
        self.draw_buttons(context, layout)
        layout.prop(self, "view_resolution")
        layout.prop(self, "render_resolution")

    def copy(self, node):
        self.n_id = ''
        self.meta_name = self.get_next_name()
        meta = self.create_metaball()
        self.label = meta.name

    def update_material(self):
        if bpy.data.materials.get(self.material):
            metaball_object = self.find_metaball()
            if metaball_object:
                metaball_object.active_material = bpy.data.materials[self.material]

    def process(self):
        if not self.activate:
            return

        if not self.inputs['Origins'].is_linked:
            return

        metaball_object = self.find_metaball()
        if not metaball_object:
            metaball_object = self.create_metaball()
            self.debug("Created new metaball")

        self.update_material()

        metaball_object.data.resolution = self.view_resolution
        metaball_object.data.render_resolution = self.render_resolution
        metaball_object.data.threshold = self.threshold

        self.label = metaball_object.name

        origins = self.inputs['Origins'].sv_get()
        origins = Matrix_generate(origins)
        radiuses = self.inputs['Radius'].sv_get()[0]
        stiffnesses = self.inputs['Stiffness'].sv_get()[0]
        negation = self.inputs['Negation'].sv_get(default=[[0]])[0]
        types = self.inputs['Types'].sv_get()[0]

        items = match_long_repeat([origins, radiuses, stiffnesses, negation, types])
        items = list(zip(*items))

        def setup_element(element, item):
            (origin, radius, stiffness, negate, meta_type) = item
            center, rotation, scale = origin.decompose()
            element.co = center[:3]
            if isinstance(meta_type, int):
                if meta_type not in self.meta_type_by_id:
                    raise Exception("`Types' input expects an integer number from 1 to 5")
                meta_type = self.meta_type_by_id[meta_type]
            element.type = meta_type
            element.radius = radius
            element.stiffness = stiffness
            element.rotation = rotation
            element.size_x = scale[0]
            element.size_y = scale[1]
            element.size_z = scale[2]
            element.use_negative = bool(negate)

        if len(items) == len(metaball_object.data.elements):
            #self.debug('Updating existing metaball data')

            for (item, element) in zip(items, metaball_object.data.elements):
                setup_element(element, item)
        else:
            #self.debug('Recreating metaball data')
            metaball_object.data.elements.clear()

            for item in items:
                element = metaball_object.data.elements.new()
                setup_element(element, item)

        if 'Objects' in self.outputs:
            self.outputs['Objects'].sv_set([metaball_object])


def register():
    bpy.utils.register_class(SvMetaballOutNode)
    bpy.utils.register_class(SvMetaballOperator)


def unregister():
    bpy.utils.unregister_class(SvMetaballOperator)
    bpy.utils.unregister_class(SvMetaballOutNode)

