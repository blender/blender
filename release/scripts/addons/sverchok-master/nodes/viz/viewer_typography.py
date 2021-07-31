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
    BoolVectorProperty,
    FloatProperty,
    IntProperty
)
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import dataCorrect, fullList, updateNode
from sverchok.utils.sv_viewer_utils import (
    matrix_sanitizer, natural_plus_one, get_random_init, greek_alphabet
)


def make_text_object(node, idx, context, data):
    scene = context.scene
    curves = bpy.data.curves
    objects = bpy.data.objects

    txt, matrix = data

    name = node.basemesh_name + "_" + str(idx)

    # CURVES
    if not (name in curves):
        f = curves.new(name, 'FONT')
    else:
        f = curves[name]

    # CONTAINER OBJECTS
    if name in objects:
        sv_object = objects[name]
    else:
        sv_object = objects.new(name, f)
        scene.objects.link(sv_object)

    default = bpy.data.fonts.get('Bfont')

    f.body = txt

    # misc
    f.size = node.fsize
    f.font = bpy.data.fonts.get(node.fontname, default)

    # space
    f.space_character = node.space_character
    f.space_word = node.space_word
    f.space_line = node.space_line

    f.offset_x = node.xoffset
    f.offset_y = node.yoffset

    # modifications
    f.offset = node.offset
    f.extrude = node.extrude

    # bevel
    f.bevel_depth = node.bevel_depth
    f.bevel_resolution = node.bevel_resolution

    # alignment, now expanded! 
    f.align_x = node.align_x
    if hasattr(node, "align_y"):
        f.align_y = node.align_y

    sv_object['idx'] = idx
    sv_object['madeby'] = node.name
    sv_object['basename'] = node.basemesh_name
    sv_object.hide_select = False

    if matrix:
        matrix = matrix_sanitizer(matrix)
        sv_object.matrix_local = matrix
    else:
        sv_object.matrix_local = Matrix.Identity(4)


class SvFontFileImporterOp(bpy.types.Operator):

    bl_idname = "node.sv_fontfile_importer"
    bl_label = "sv FontFile Importer"

    filepath = StringProperty(
        name="File Path",
        description="Filepath used for importing the font file",
        maxlen=1024, default="", subtype='FILE_PATH')

    def execute(self, context):
        n = self.node
        t = bpy.data.fonts.load(self.filepath)
        n.fontname = t.name
        return {'FINISHED'}

    def invoke(self, context, event):
        self.node = context.node
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class SvTypeViewOp2(bpy.types.Operator):

    bl_idname = "node.sv_callback_type_viewer"
    bl_label = "Sverchok Type general callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def hide_unhide(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        child = lambda obj: obj.type == "FONT" and obj.name.startswith(k)
        objs = list(filter(child, bpy.data.objects))

        if type_op in {'hide', 'hide_render', 'hide_select'}:
            op_value = getattr(n, type_op)
            for obj in objs:
                setattr(obj, type_op, op_value)
            setattr(n, type_op, not op_value)

        elif type_op == 'typography_select':
            for obj in objs:
                obj.select = n.select_state_mesh
            n.select_state_mesh = not n.select_state_mesh

        elif type_op == 'random_mesh_name':
            n.basemesh_name = get_random_init()

        elif type_op == 'add_material':
            mat = bpy.data.materials.new('sv_material')
            mat.use_nodes = True
            mat.use_fake_user = True  # usually handy
            n.material = mat.name

    def execute(self, context):
        self.hide_unhide(context, self.fn_name)
        return {'FINISHED'}


class SvTypeViewerNode(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvTypeViewerNode'
    bl_label = 'Typography Viewer'
    bl_icon = 'OUTLINER_OB_EMPTY'

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

    hide = BoolProperty(default=True)
    hide_render = BoolProperty(default=True)
    hide_select = BoolProperty(default=True)

    select_state_mesh = BoolProperty(default=False)

    activate = BoolProperty(
        default=True,
        description='When enabled this will process incoming data',
        update=updateNode)

    basemesh_name = StringProperty(
        default='Alpha',
        update=updateNode,
        description="sets which base name the object will use, "
        "use N-panel to pick alternative random names")

    layer_choice = BoolVectorProperty(
        subtype='LAYER', size=20,
        update=layer_updateNode,
        description="This sets which layer objects are placed on",
        get=g, set=s)

    show_options = BoolProperty(default=0)
    fontname = StringProperty(default='', update=updateNode)
    fsize = FloatProperty(default=1.0, update=updateNode)

    # space
    space_character = FloatProperty(default=1.0, update=updateNode)
    space_word = FloatProperty(default=1.0, update=updateNode)
    space_line = FloatProperty(default=1.0, update=updateNode)
    yoffset = FloatProperty(default=0.0, update=updateNode)
    xoffset = FloatProperty(default=0.0, update=updateNode)

    # modifications
    offset = FloatProperty(default=0.0, update=updateNode)
    extrude = FloatProperty(default=0.0, update=updateNode)

    # bevel
    bevel_depth = FloatProperty(default=0.0, update=updateNode)
    bevel_resolution = IntProperty(default=0, update=updateNode)

    # orientation x | y 
    mode_options = [(_item, _item, "", idx) for idx, _item in enumerate(['LEFT', 'CENTER', 'RIGHT', 'JUSTIFY', 'FLUSH'])]
    align_x = bpy.props.EnumProperty(
        items=mode_options, description="Horizontal Alignment", default="LEFT", update=updateNode
    )

    mode_options_y = [(_item, _item, "", idx) for idx, _item in enumerate(['TOP_BASELINE', 'TOP', 'CENTER', 'BOTTOM'])]
    align_y = bpy.props.EnumProperty(
        items=mode_options_y, description="Vertical Alignment", default="TOP_BASELINE", update=updateNode
    )

    parent_to_empty = BoolProperty(default=False, update=updateNode)
    parent_name = StringProperty()  # calling updateNode would recurse.

    def sv_init(self, context):
        # self['lp'] = [True] + [False] * 19
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('StringsSocket', 'text', 'text')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def draw_buttons(self, context, layout):
        view_icon = 'BLENDER' if self.activate else 'ERROR'

        sh = 'node.sv_callback_type_viewer'

        def icons(TYPE):
            ICON = {
                'hide': 'RESTRICT_VIEW',
                'hide_render': 'RESTRICT_RENDER',
                'hide_select': 'RESTRICT_SELECT'}.get(TYPE)
            return 'WARNING' if not ICON else ICON + ['_ON', '_OFF'][getattr(self, TYPE)]

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
            row.operator(sh, text='Select').fn_name = 'typography_select'

            row = col.row(align=True)
            row.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_CURVE')

            col = layout.column(align=True)
            col.prop(self, 'fsize')
            col.prop(self, 'show_options', toggle=True)
            if self.show_options:
                col.label('position')
                row = col.row(align=True)
                if row:
                    row.prop(self, 'xoffset', text='XOFF')
                    row.prop(self, 'yoffset', text='YOFF')
                split = col.split()
                col1 = split.column()
                col1.prop(self, 'space_character', text='CH')
                col1.prop(self, 'space_word', text='W')
                col1.prop(self, 'space_line', text='L')

                col.label('modifications')
                col.prop(self, 'offset')
                col.prop(self, 'extrude')
                col.label('bevel')
                col.prop(self, 'bevel_depth')
                col.prop(self, 'bevel_resolution')

                col.label("alignment")
                row = col.row(align=True)
                row.prop(self, 'align_x', text="")
                row.prop(self, 'align_y', text="")
                col.separator()

            row = col.row(align=True)
            row.prop_search(self, 'material', bpy.data, 'materials', text='', icon='MATERIAL_DATA')
            row.operator(sh, text='', icon='ZOOMIN').fn_name = 'add_material'

    def draw_buttons_ext(self, context, layout):
        sh = 'node.sv_callback_type_viewer'
        shf = 'node.sv_fontfile_importer'

        self.draw_buttons(context, layout)

        layout.separator()
        row = layout.row(align=True)
        row.operator(sh, text='Rnd Name').fn_name = 'random_mesh_name'

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop_search(self, 'fontname', bpy.data, 'fonts', text='', icon='FONT_DATA')
        row.operator(shf, text='', icon='ZOOMIN')

        box = col.box()
        if box:
            box.label(text="Beta options")
            box.prop(self, 'layer_choice', text='layer')

        row = layout.row()
        row.prop(self, 'parent_to_empty', text='parented')
        if self.parent_to_empty:
            row.label(self.parent_name)

    def process(self):

        if (not self.activate) or (not self.inputs['text'].is_linked):
            return

        # no autorepeat yet.
        text = self.inputs['text'].sv_get(default=[['sv_text']])[0]
        matrices = self.inputs['matrix'].sv_get(default=[[]])

        if self.parent_to_empty:
            mtname = 'Empty_' + self.basemesh_name
            self.parent_name = mtname
            scene = bpy.context.scene
            if not (mtname in bpy.data.objects):
                empty = bpy.data.objects.new(mtname, None)
                scene.objects.link(empty)
                scene.update()

        for obj_index, txt_content in enumerate(text):
            matrix = matrices[obj_index]
            if isinstance(txt_content, list) and (len(txt_content) == 1):
                txt_content = txt_content[0]
            else:
                txt_content = str(txt_content)

            make_text_object(self, obj_index, bpy.context, (txt_content, matrix))

        self.remove_non_updated_objects(obj_index)
        objs = self.get_children()

        if self.grouping:
            self.to_group(objs)

        # truthy if self.material is in .materials
        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials(objs)

        for obj in objs:
            if self.parent_to_empty:
                obj.parent = bpy.data.objects[mtname]
            elif obj.parent:
                obj.parent = None


    def get_children(self):
        objs = [obj for obj in bpy.data.objects if obj.type == 'FONT']
        return [o for o in objs if o.get('basename') == self.basemesh_name]

    def remove_non_updated_objects(self, obj_index):
        objs = self.get_children()
        objs = [obj.name for obj in objs if obj['idx'] > obj_index]
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

        # delete associated meshes
        for object_name in objs:
            curves.remove(curves[object_name])

    def to_group(self, objs):
        groups = bpy.data.groups
        named = self.basemesh_name

        # alias group, or generate new group and alias that
        group = groups.get(named, groups.new(named))

        for obj in objs:
            if not (obj.name in group.objects):
                group.objects.link(obj)

    def set_corresponding_materials(self, objs):
        for obj in objs:
            obj.active_material = bpy.data.materials[self.material]

    def copy(new_node, node):
        new_node.basemesh_name = get_random_init()


def register():
    bpy.utils.register_class(SvTypeViewerNode)
    bpy.utils.register_class(SvTypeViewOp2)
    bpy.utils.register_class(SvFontFileImporterOp)


def unregister():
    bpy.utils.unregister_class(SvFontFileImporterOp)
    bpy.utils.unregister_class(SvTypeViewerNode)
    bpy.utils.unregister_class(SvTypeViewOp2)
