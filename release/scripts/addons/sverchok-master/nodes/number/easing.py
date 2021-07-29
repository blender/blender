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

from mathutils import Vector
import bpy
from bpy.props import FloatProperty, EnumProperty, StringProperty, BoolProperty

import blf
import bgl

from sverchok.data_structure import updateNode, node_id
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.ui import nodeview_bgl_viewer_draw_mk2 as nvBGL2

# star imports easing_dict and all easing functions.
from sverchok.utils.sv_easing_functions import *

easing_list = []
for k in sorted(easing_dict.keys()):
    fname = easing_dict[k].__name__
    easing_list.append(tuple([str(k), fname, "", k]))


palette_dict = {
    "default": (
        (0.243299, 0.590403, 0.836084, 1.00),  # back_color 
        (0.390805, 0.754022, 1.000000, 1.00),  # grid_color
        (1.000000, 0.330010, 0.107140, 1.00)   # line_color
    ),
    "scope": (
        (0.274677, 0.366253, 0.386430, 1.00),  # back_color 
        (0.423268, 0.558340, 0.584078, 1.00),  # grid_color
        (0.304762, 1.000000, 0.062827, 1.00)   # line_color
    )

}


def simple_grid_xy(x, y, args):
    func = args[0]
    back_color, grid_color, line_color = args[1]

    def draw_rect(x=0, y=0, w=30, h=10, color=(0.0, 0.0, 0.0, 1.0)):

        bgl.glColor4f(*color)       
        bgl.glBegin(bgl.GL_POLYGON)

        for coord in [(x, y), (x+w, y), (w+x, y-h), (x, y-h)]:
            bgl.glVertex2f(*coord)
        bgl.glEnd()


    # draw bg fill
    draw_rect(x=x, y=y, w=140, h=140, color=back_color)

    # draw grid
    bgl.glColor4f(*grid_color)
    num_divs = 8
    offset = 140/num_divs
    line_parts_x = []
    line_parts_y = []
    for i in range(num_divs+1):
        xpos1 = x + (i*offset)
        ypos1 = y
        ypos2 = y - 140
        line_parts_x.extend([[xpos1, ypos1], [xpos1, ypos2]])

        ypos = y - (i*offset)
        line_parts_y.extend([[x, ypos], [x+140, ypos]])

    bgl.glLineWidth(0.8)
    bgl.glBegin(bgl.GL_LINES)
    for coord in line_parts_x + line_parts_y:
        bgl.glVertex2f(*coord)
    bgl.glEnd()        

    # draw graph-line
    bgl.glColor4f(*line_color)
    bgl.glLineWidth(2.0)
    bgl.glBegin(bgl.GL_LINE_STRIP)
    num_points = 100
    seg_diff = 1 / num_points
    for i in range(num_points+1):
        _px = x + ((i * seg_diff) * 140)
        _py = y - (1 - func(i * seg_diff) * 140) - 140
        bgl.glVertex2f(_px, _py)
    bgl.glEnd()        




class SvEasingNode(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvEasingNode'
    bl_label = 'Easing 0..1'

    n_id = StringProperty(default='')
    activate = BoolProperty(
        name='Show', description='Activate drawing',
        default=True,
        update=updateNode)

    selected_mode = EnumProperty(
        items=easing_list,
        description="offers easing choice",
        default="0",
        update=updateNode
    )

    in_float = FloatProperty(
        min=0.0, max=1.0, default=0.0, name='Float Input',
        description='input to the easy function', update=updateNode
    )

    theme_mode_options = [(m, m, '', idx) for idx, m in enumerate(["default", "scope"])]
    selected_theme_mode = EnumProperty(
        items=theme_mode_options, default="default", update=updateNode
    )

    def draw_buttons(self, context, l):
        c = l.column()
        c.label(text="set easing function")
        c.prop(self, "selected_mode", text="")
        c.prop(self, 'activate')

    def draw_buttons_ext(self, context, l):
        l.prop(self, "selected_theme_mode")

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Float").prop_name = 'in_float'
        self.outputs.new('StringsSocket', "Float")

    def process(self):
        p = self.inputs['Float'].sv_get()
        n_id = node_id(self)

        # end early
        nvBGL2.callback_disable(n_id)

        float_out = self.outputs['Float']
        easing_func = easing_dict.get(int(self.selected_mode))
        if float_out.is_linked:
            out = []
            for obj in p:
                r = []
                for i in obj:
                    r.append(easing_func(i))
                out.append(r)
            float_out.sv_set(out)
        else:
            float_out.sv_set([[None]])

        if self.activate:

            palette = palette_dict.get(self.selected_theme_mode)[:]
            x, y = [int(j) for j in (self.location + Vector((self.width + 20, 0)))[:]]
            
            draw_data = {
                'tree_name': self.id_data.name[:],
                'mode': 'custom_function', 
                'custom_function': simple_grid_xy,
                'loc': (x, y),
                'args': (easing_func, palette)
            }
            nvBGL2.callback_enable(n_id, draw_data)

    def free(self):
        nvBGL2.callback_disable(node_id(self))            

    # reset n_id on copy
    def copy(self, node):
        self.n_id = ''


def register():
    bpy.utils.register_class(SvEasingNode)


def unregister():
    bpy.utils.unregister_class(SvEasingNode)
