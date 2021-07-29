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
from mathutils import Vector
from bpy.props import (BoolProperty, FloatVectorProperty, StringProperty, FloatProperty)

from sverchok.ui import index_viewer_draw as IV
from sverchok.node_tree import SverchCustomTreeNode, MatrixSocket, VerticesSocket, StringsSocket
from sverchok.data_structure import (
    dataCorrect,
    node_id,
    updateNode,
    fullList,
    Vector_generate,
    Matrix_generate)



# status colors
FAIL_COLOR = (0.1, 0.05, 0)
READY_COLOR = (1, 0.3, 0)


class SvBakeText (bpy.types.Operator):
    """3Dtext baking"""

    bl_idname = "node.sv_text_baking"
    bl_label = "bake text"
    bl_options = {'REGISTER', 'UNDO'}

    idname = StringProperty(name='idname', description='name of parent node',
                            default='')
    idtree = StringProperty(name='idtree', description='name of parent tree',
                            default='')

    @property
    def node(self):
        return bpy.data.node_groups[self.idtree].nodes[self.idname]

    def execute(self, context):
        self.collect_text_to_bake()
        return {'FINISHED'}

    def collect_text_to_bake(self):
        node = self.node
        inputs = node.inputs

        def has_good_link(name, TypeSocket):
            if inputs[name].links:
                socket = inputs[name].links[0].from_socket
                return isinstance(socket, TypeSocket)

        def get_socket_type(name):
            if name in {'edges', 'faces', 'text'}:
                return StringsSocket
            elif name == 'matrix':
                return MatrixSocket
            else:
                return VerticesSocket

        def get_data(name, fallback=[]):
            TypeSocket = get_socket_type(name)
            if has_good_link(name, TypeSocket):
                d = dataCorrect(inputs[name].sv_get())
                if name == 'matrix':
                    d = Matrix_generate(d) if d else []
                elif name == 'vertices':
                    d = Vector_generate(d) if d else []
                return d
            return fallback

        data_vector = get_data('vertices')
        if not data_vector:
            return

        data_edges = get_data('edges')
        data_faces = get_data('faces')
        data_matrix = get_data('matrix')
        data_text = get_data('text', '')

        for obj_index, verts in enumerate(data_vector):
            final_verts = verts

            if data_text:
                text_obj = data_text[obj_index]
            else:
                text_obj = ''

            if data_matrix:
                matrix = data_matrix[obj_index]
                final_verts = [matrix * v for v in verts]

            if node.display_vert_index:
                for idx, v in enumerate(final_verts):
                    self.bake(idx, v, text_obj)

            if data_edges and node.display_edge_index:
                for edge_index, (idx1, idx2) in enumerate(data_edges[obj_index]):
                    v1 = Vector(final_verts[idx1])
                    v2 = Vector(final_verts[idx2])
                    loc = v1 + ((v2 - v1) / 2)
                    self.bake(edge_index, loc, text_obj)

            if data_faces and node.display_face_index:
                for face_index, f in enumerate(data_faces[obj_index]):
                    verts = [Vector(final_verts[idx]) for idx in f]
                    median = self.calc_median(verts)
                    self.bake(face_index, median, text_obj)

    def bake(self, index, origin, text_obj):
        node = self.node

        text_ = '' if (text_obj == '') else text_obj[index]
        text = str(text_[0] if text_ else index)

        # Create and name TextCurve object
        name = 'sv_text_' + text
        tcu = bpy.data.curves.new(name=name, type='FONT')
        obj = bpy.data.objects.new(name, tcu)
        obj.location = origin
        bpy.context.scene.objects.link(obj)

        # TextCurve attributes
        file_font = bpy.data.fonts.get(node.fonts)
        if file_font:
            # else blender defaults to using bfont,
            # and bfont is added to data.fonts
            tcu.font = file_font

        tcu.body = text
        tcu.offset_x = 0
        tcu.offset_y = 0
        tcu.resolution_u = 2
        tcu.shear = 0
        tcu.size = node.font_size
        tcu.space_character = 1
        tcu.space_word = 1
        tcu.align_x = 'CENTER'
        tcu.align_y = 'CENTER'
        tcu.extrude = 0.0
        tcu.fill_mode = 'NONE'

    def calc_median(self, vlist):
        a = Vector((0, 0, 0))
        for v in vlist:
            a += v
        return a / len(vlist)


class IndexViewerNode(bpy.types.Node, SverchCustomTreeNode):

    ''' IDX ViewerNode '''
    bl_idname = 'IndexViewerNode'
    bl_label = 'Viewer Index'
    bl_icon = 'OUTLINER_OB_EMPTY'

    # node id
    n_id = StringProperty(default='', options={'SKIP_SAVE'})

    activate = BoolProperty(
        name='Show', description='Activate node?',
        default=True,
        update=updateNode)

    bakebuttonshow = BoolProperty(
        name='bakebuttonshow', description='show bake button on node',
        default=False,
        update=updateNode)

    draw_bg = BoolProperty(
        name='draw_bg', description='draw background poly?',
        default=False,
        update=updateNode)

    display_vert_index = BoolProperty(
        name="Vertices", description="Display vertex indices",
        default=True,
        update=updateNode)
    display_edge_index = BoolProperty(
        name="Edges", description="Display edge indices",
        update=updateNode)
    display_face_index = BoolProperty(
        name="Faces", description="Display face indices",
        update=updateNode)

    fonts = StringProperty(name='fonts', default='', update=updateNode)

    font_size = FloatProperty(
        name="font_size", description='',
        min=0.01, default=0.1,
        update=updateNode)

    def make_color_prop(name, col):
        return FloatVectorProperty(
            name=name, description='', size=4, min=0.0, max=1.0,
            default=col, subtype='COLOR', update=updateNode)

    bg_edges_col = make_color_prop("bg_edges", (.2, .2, .2, 1.0))
    bg_faces_col = make_color_prop("bg_faces", (.2, .2, .2, 1.0))
    bg_verts_col = make_color_prop("bg_verts", (.2, .2, .2, 1.0))
    numid_edges_col = make_color_prop("numid_edges", (1.0, 1.0, 0.1, 1.0))
    numid_faces_col = make_color_prop("numid_faces", (1.0, .8, .8, 1.0))
    numid_verts_col = make_color_prop("numid_verts", (1, 1, 1, 1.0))

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edges', 'edges')
        self.inputs.new('StringsSocket', 'faces', 'faces')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')
        self.inputs.new('StringsSocket', 'text', 'text')

    # reset n_id on copy
    def copy(self, node):
        self.n_id = ''

    def draw_buttons(self, context, layout):
        view_icon = 'RESTRICT_VIEW_' + ('OFF' if self.activate else 'ON')

        column_all = layout.column()

        row = column_all.row(align=True)
        split = row.split()
        r = split.column()
        r.prop(self, "activate", text="Show", toggle=True, icon=view_icon)
        row.prop(self, "draw_bg", text="Background", toggle=True)

        col = column_all.column(align=True)
        row = col.row(align=True)
        row.prop(self, "display_vert_index", toggle=True, icon='VERTEXSEL', text='')
        row.prop(self, "numid_verts_col", text="")
        if self.draw_bg:
            row.prop(self, "bg_verts_col", text="")

        row = col.row(align=True)
        row.prop(self, "display_edge_index", toggle=True, icon='EDGESEL', text='')
        row.prop(self, "numid_edges_col", text="")
        if self.draw_bg:
            row.prop(self, "bg_edges_col", text="")

        row = col.row(align=True)
        row.prop(self, "display_face_index", toggle=True, icon='FACESEL', text='')
        row.prop(self, "numid_faces_col", text="")
        if self.draw_bg:
            row.prop(self, "bg_faces_col", text="")

        if self.bakebuttonshow:
            col = column_all.column(align=True)
            row = col.row(align=True)
            row.scale_y = 3
            baker = row.operator('node.sv_text_baking', text='B A K E')
            baker.idname = self.name
            baker.idtree = self.id_data.name

            row = col.row(align=True)
            row.prop(self, "font_size")

            row = col.row(align=True)
            row.prop_search(self, 'fonts', bpy.data, 'fonts', text='', icon='FONT_DATA')

    def get_settings(self):
        '''Produce a dict of settings for the callback'''
        # A copy is needed, we can't have reference to the
        # node in a callback, it will crash blender on undo
        return {
            'bg_edges_col': self.bg_edges_col[:],
            'bg_faces_col': self.bg_faces_col[:],
            'bg_verts_col': self.bg_verts_col[:],
            'numid_edges_col': self.numid_edges_col[:],
            'numid_faces_col': self.numid_faces_col[:],
            'numid_verts_col': self.numid_verts_col[:],
            'display_vert_index': self.display_vert_index,
            'display_edge_index': self.display_edge_index,
            'display_face_index': self.display_face_index
        }.copy()

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        box = layout.box()
        little_width = 0.135

        # heading - wide column for descriptors
        col = box.column(align=True)
        row = col.row(align=True)
        row.label(text='Colors')  # IDX pallete

        # heading - remaining column space divided by
        # little_width factor. shows icons only
        col1 = row.column(align=True)
        col1.scale_x = little_width
        col1.label(icon='VERTEXSEL', text=' ')

        col2 = row.column(align=True)
        col2.scale_x = little_width
        col2.label(icon='EDGESEL', text=' ')

        col3 = row.column(align=True)
        col3.scale_x = little_width
        col3.label(icon='FACESEL', text=' ')

        # 'table info'
        colprops = [
            ['Numbers :', [
                'numid_verts_col', 'numid_edges_col', 'numid_faces_col']],
            ['Backgrnd :', [
                'bg_verts_col', 'bg_edges_col', 'bg_faces_col']]
        ]

        # each first draws the table row heading, 'label'
        # then for each geometry type will draw the color property
        # with the same spacing as col1, col2, col3 above
        for label, geometry_types in colprops:
            row = col.row(align=True)
            row.label(text=label)
            for colprop in geometry_types:
                col4 = row.column(align=True)
                col4.scale_x = little_width
                col4.prop(self, colprop, text="")

        layout.prop(self, 'bakebuttonshow', text='show bake UI')

    def update(self):
        # used because this node should disable itself in certain scenarios
        # : namely , no inputs.
        n_id = node_id(self)
        IV.callback_disable(n_id)

    def process(self):
        inputs = self.inputs
        n_id = node_id(self)
        IV.callback_disable(n_id)

        # end if tree status is set to not show
        if not self.id_data.sv_show:
            return

        self.use_custom_color = True

        if not (self.activate and inputs['vertices'].is_linked):
            return

        self.generate_callback(n_id, IV)

    def generate_callback(self, n_id, IV):
        inputs = self.inputs

        verts, matrices = [], []
        text = ''

        # gather vertices from input
        propv = inputs['vertices'].sv_get()
        verts = dataCorrect(propv)

        # end early, no point doing anything else.
        if not verts:
            return

        # draw text on locations instead of indices.
        text_so = inputs['text'].sv_get(default=[])
        text = dataCorrect(text_so)
        if text:
            fullList(text, len(verts))
            for i, t in enumerate(text):
                fullList(text[i], len(verts[i]))

        # read non vertex inputs in a loop and assign to data_collected
        data_collected = []
        for socket in ['edges', 'faces', 'matrix']:
            propm = inputs[socket].sv_get(default=[])
            input_stream = dataCorrect(propm)
            data_collected.append(input_stream)

        edges, faces, matrices = data_collected

        bg = self.draw_bg
        settings = self.get_settings()
        IV.callback_enable(
            n_id, verts, edges, faces, matrices, bg, settings, text)

    def free(self):
        IV.callback_disable(node_id(self))

    def bake(self):
        if self.activate and self.inputs['vertices'].links:
            textbake = bpy.ops.node.sv_text_baking
            textbake(idname=self.name, idtree=self.id_data.name)


def register():
    bpy.utils.register_class(IndexViewerNode)
    bpy.utils.register_class(SvBakeText)


def unregister():
    bpy.utils.unregister_class(SvBakeText)
    bpy.utils.unregister_class(IndexViewerNode)
