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

from math import floor, ceil

import bpy
from bpy.props import (
    IntProperty, FloatProperty, StringProperty, BoolProperty)

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList

'''
by dealga mcardle sept 2014

Premise of the node

- make pixel copy of selected image using [:]
- outputs
    - [x, y, a]  [r, g, b]
    - polygons

this would allow one to combine x and y with r to show
a strength table of r over x,y

'''


def idx_to_co(idx, width):
    """ helps translate index of pixel into 2d coordinate """
    r = int(idx / width)
    c = idx % width
    return c, r


def generate_polygons(num_x, num_y):
    """
    taken from https://github.com/zeffii/BlenderSciViz/
    num_x = num verts on x side
    """
    faces = []
    faces_append = faces.append
    for i in range((num_x - 1) * (num_y - 1)):
        x = (i % (num_x - 1))
        y = floor(i / (num_x - 1))

        level = x + (y * num_x)
        idx1 = level
        idx2 = idx1 + 1
        idx4 = idx1 + num_x
        idx3 = idx4 + 1
        faces_append([idx1, idx2, idx3, idx4])

    return faces


class svImageImporterOp(bpy.types.Operator):

    bl_idname = "image.image_importer"
    bl_label = "sv Image Import Operator"

    filepath = StringProperty(
        name="File Path",
        description="Filepath used for importing the font file",
        maxlen=1024, default="", subtype='FILE_PATH')

    origin = StringProperty("")

    def execute(self, context):
        a = bpy.data.images.load(self.filepath)
        node_tree, node_name = self.origin.split('|><|')
        node = bpy.data.node_groups[node_tree].nodes[node_name]
        node.image_name = a.name
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class ImageComponentsOps(bpy.types.Operator):

    bl_idname = "node.image_comp_callback"
    bl_label = "Sverchok imagecomp callback"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def load_image(self, context):
        n = context.node
        if not hash(n) in n.node_dict:
            n.node_dict[hash(n)] = {}

        node_dict = n.node_dict[hash(n)]['node_image'] = {}

        img = bpy.data.images[n.image_name]
        img.use_fake_user = True  # maybe not?

        w = img_width = img.size[0]
        h = img_height = img.size[1]

        # work on copy only
        pxls = img.pixels[:]
        num_pixels = int(len(pxls) / 4)

        node_dict['image'] = {
            'xya': [], 'rgb': [], 'polygons': [], 'dimensions': [w, h]
        }

        image = node_dict['image']
        add_xya = image['xya'].append
        add_rgb = image['rgb'].append

        # generator expression
        gen_obj = (i for i in pxls)

        # x and y will be local when this function is called
        def add_pixeldata():
            r = next(gen_obj)
            g = next(gen_obj)
            b = next(gen_obj)
            a = next(gen_obj)
            if n.filter_mode:
                if not eval(n.filter_str, {}, vars()):
                    return
            add_xya((x, y, a))
            add_rgb((r, g, b))

        if n.skip == 0:
            for idx in range(num_pixels):
                x, y = idx_to_co(idx, w)
                add_pixeldata()
        else:
            xlookup = [ix for ix in range(w) if ix % (n.skip+1) == 0]
            ylookup = [iy for iy in range(h) if iy % (n.skip+1) == 0]

            for idx in range(num_pixels):
                x, y = idx_to_co(idx, w)
                if not ((x in xlookup) and (y in ylookup)):
                    [next(gen_obj) for i in range(4)]
                    continue
                add_pixeldata()

        n.loaded = True

    def unload_image(self, context):
        n = context.node
        # using f8 to reload python addons, forces us to add extra checks
        # because the n.dict becomes empty on reload.
        if hash(n) in n.node_dict:
            n.node_dict[hash(n)]['node_image']['image'] = {}
        n.loaded = False

    def execute(self, context):
        n = context.node
        fn_name = self.fn_name

        if not n.image_name:
            msg = 'no image picked, or imported'
            self.report({"WARNING"}, msg)
            return {'FINISHED'}

        if fn_name == 'load':
            self.load_image(context)
        elif fn_name == 'unload':
            self.unload_image(context)

        return {'FINISHED'}


class SvImageComponentsNode(bpy.types.Node, SverchCustomTreeNode):

    ''' Image Components'''
    bl_idname = 'SvImageComponentsNode'
    bl_label = 'Image Decompose'
    bl_icon = 'GROUP_VCOL'

    # node storage, reference by the hash of self.
    node_dict = {}

    image_name = StringProperty(
        default="",
        name='image_name',
        description='name of image to CC',
        update=updateNode)

    loaded = BoolProperty(
        default=0,
        name='loaded',
        description='confirms the state of image loading',
        update=updateNode)

    xy_spread = FloatProperty(
        default=0.01,
        step=0.001,
        name='xy_spread',
        update=updateNode)

    z_spread = FloatProperty(
        default=1.00,
        step=0.001,
        name='z_spread',
        update=updateNode)

    skip = IntProperty(
        default=3,
        step=1, min=0)

    filter_mode = BoolProperty(
        default=0,
        name='filter_mode',
        description='Allows arbitary filter logic, to spit out verts (polygons are dropped)',
        update=updateNode)

    filter_str = StringProperty(
        default='',
        name='filter_str',
        description='will safe eval this string',
        update=updateNode)

    def sv_init(self, context):
        self.node_dict[hash(self)] = {}
        self.node_dict[hash(self)]['node_image'] = {}

        xy, z, p = 'xy_spread', 'z_spread', 'polygons'
        socket = 'StringsSocket'
        vsocket = 'VerticesSocket'

        new_in = self.inputs.new
        new_in(socket, xy).prop_name = xy
        new_in(socket, z).prop_name = z

        new_out = self.outputs.new
        new_out(vsocket, 'xya')
        new_out(vsocket, 'rgb')
        new_out(socket, p)

    def draw_buttons(self, context, layout):
        col = layout.column()

        if not self.loaded:
            col.prop_search(
                self, 'image_name', bpy.data, 'images',
                text='', icon='FILE_IMAGE')
            operator_type = 'load'
            operator_icon = 'IMPORT'
        else:
            col.label('loaded: ' + self.image_name)
            operator_type = 'unload'
            operator_icon = 'FILE'

        col.operator(
            'node.image_comp_callback',
            text=operator_type,
            icon=operator_icon).fn_name = operator_type

        col.prop(self, 'filter_mode', text='Filter?', toggle=True)
        if self.filter_mode:
            col.prop(self, 'filter_str', text='', icon='GROUP_VCOL')

        col.prop(self, 'skip', text='Skip n pixels')

        if self.loaded:
            if not (hash(self) in self.node_dict):
                pass
            else:
                image_dict = self.node_dict[hash(self)].get('node_image')
                if image_dict:
                    w, h = image_dict['image']['dimensions']
                    col.label('dims h={h}, w={w}'.format(w=w, h=h))

    def draw_buttons_ext(self, context, layout):

        # poor mans origin tracker
        node_tree = self.id_data.name
        node_name = self.name
        origin = '|><|'.join([node_tree, node_name])

        col = layout.column()
        image_op = col.operator(
            'image.image_importer',
            text='img from disk',
            icon="FILE_IMAGE").origin = origin


    def process(self):
        outputs = self.outputs
        
        if not self.loaded:
            return
        if not outputs['xya'].is_linked:
            return

        # upload reload, this avoids errors - still not perfect
        if self.loaded and not (hash(self) in self.node_dict):
            self.node_dict[hash(self)] = {}
            self.loaded = 0

        node_image = self.node_dict[hash(self)]['node_image']
        dict_data = node_image['image']
        xya = 'xya'
        rgb = 'rgb'
        m1 = self.xy_spread
        m2 = self.z_spread

        if outputs[xya].is_linked:
            tmul = lambda v: (v[0]*m1, v[1]*m1, v[2])
            data = [tmul(v) for v in dict_data[xya]]
            outputs[xya].sv_set([data])

        if outputs[rgb].is_linked:
            if -0.001 <= m2 <= 0.001:
                data = dict_data[rgb]
            else:
                tmul = lambda v: (v[0]*m2, v[1]*m2, v[2]*m2)
                data = [tmul(v) for v in dict_data[rgb]]

            outputs[rgb].sv_set([data])

        if self.filter_mode:
            '''
            possible to generate polygon from pixel.. later..for now return
            '''
            return

        polygons = 'polygons'
        if not outputs[polygons].is_linked:
            return

        polygon_data = dict_data[polygons]
        if not polygon_data:

            w, h = dict_data['dimensions']
            if not self.skip == 0:
                w = ceil(w/(self.skip+1))
                h = ceil(h/(self.skip+1))

            dict_data[polygons] = generate_polygons(w, h)

        outputs[polygons].sv_set([polygon_data])

    def update_socket(self, context):
        self.update()


def register():
    bpy.utils.register_class(svImageImporterOp)
    bpy.utils.register_class(ImageComponentsOps)
    bpy.utils.register_class(SvImageComponentsNode)


def unregister():
    bpy.utils.unregister_class(ImageComponentsOps)
    bpy.utils.unregister_class(SvImageComponentsNode)
    bpy.utils.unregister_class(svImageImporterOp)
