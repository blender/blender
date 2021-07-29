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

import os
import gzip

import bpy
import blf
from bpy.props import BoolProperty, FloatVectorProperty, StringProperty, IntProperty
from mathutils import Vector

from sverchok.utils.context_managers import sv_preferences
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import node_id, updateNode
from sverchok.ui import nodeview_bgl_viewer_draw_mk2 as nvBGL


# status colors
FAIL_COLOR = (0.1, 0.05, 0)
READY_COLOR = (1, 0.3, 0)


def high_contrast_color(c):
    g = 2.2  # gamma
    L = 0.2126 * (c.r**g) + 0.7152 * (c.g**g) + 0.0722 * (c.b**g)
    return [(.1, .1, .1), (.95, .95, .95)][int(L < 0.5)]

def adjust_location(_x, _y, location_theta):
    return _x * location_theta, _y * location_theta


class SvStethoscopeNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    bl_idname = 'SvStethoscopeNodeMK2'
    bl_label = 'Stethoscope MK2'
    bl_icon = 'OUTLINER_OB_EMPTY'

    n_id = StringProperty(default='')
    font_id = IntProperty(default=0, update=updateNode)

    text_color = FloatVectorProperty(
        name="Color", description='Text color',
        size=3, min=0.0, max=1.0,
        default=(.1, .1, .1), subtype='COLOR',
        update=updateNode)

    activate = BoolProperty(
        name='Show', description='Activate node?',
        default=True,
        update=updateNode)

    mode_options = [(i, i, '', idx) for idx, i in enumerate(["text-based", "graphical"])]
    selected_mode = bpy.props.EnumProperty(
        items=mode_options,
        description="offers....",
        default="text-based", update=updateNode
    )

    view_by_element = BoolProperty(update=updateNode)
    num_elements = IntProperty(default=0)
    element_index = IntProperty(default=0, update=updateNode)
    rounding = IntProperty(min=1, max=5, default=3, update=updateNode)
    line_width = IntProperty(default=60, min=20, update=updateNode, name='Line Width (chars)')
    compact = BoolProperty(default=False, update=updateNode)
    depth = IntProperty(default=5, min=0, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', 'Data')
        try:
            current_theme = bpy.context.user_preferences.themes.items()[0][0]
            editor = bpy.context.user_preferences.themes[current_theme].node_editor
            self.text_color = high_contrast_color(editor.space.back)
        except:
            print('-', end='')

    # reset n_id on copy
    def copy(self, node):
        self.n_id = ''

    def draw_buttons(self, context, layout):
        row = layout.row()
        icon = 'RESTRICT_VIEW_OFF' if self.activate else 'RESTRICT_VIEW_ON'
        row.separator()
        row.prop(self, "activate", icon=icon, text='')

        layout.prop(self, 'selected_mode', expand=True)
        if self.selected_mode == 'text-based':

            row.prop(self, "text_color", text='')
            row1 = layout.row(align=True)
            row1.prop(self, "rounding")
            row1.prop(self, "compact", toggle=True)
            row2 = layout.row(align=True)
            row2.prop(self, "line_width")
            row2.prop(self, "depth")
            # layout.prop(self, "socket_name")
            layout.label('input has {0} elements'.format(self.num_elements))
            layout.prop(self, 'view_by_element', toggle=True)
            if self.num_elements > 0 and self.view_by_element:
                layout.prop(self, 'element_index', text='get index')

        else:
            pass

    def draw_buttons_ext(self, context, layout):
        layout.prop(self, 'font_id')

    def process(self):
        inputs = self.inputs
        n_id = node_id(self)

        # end early
        nvBGL.callback_disable(n_id)

        if self.activate and inputs[0].is_linked:

            try:
                with sv_preferences() as prefs:
                    scale = prefs.stethoscope_view_scale
                    location_theta = prefs.stethoscope_view_xy_multiplier
            except:
                # print('did not find preferences - you need to save user preferences')
                scale = 1.0
                location_theta = 1.0

            # gather vertices from input
            data = inputs[0].sv_get(deepcopy=False)
            self.num_elements = len(data)


            if self.selected_mode == 'text-based':
                props = lambda: None
                props.line_width = self.line_width
                props.compact = self.compact
                props.depth = self.depth or None

                processed_data = nvBGL.parse_socket(
                    inputs[0],
                    self.rounding,
                    self.element_index,
                    self.view_by_element,
                    props
                )
            else:
                #                # implement another nvBGL parses for gfx
                processed_data = data

            # adjust the location of the printed bgl text, depending on hidden state
            node_width = (self.width_hidden + 30.0) if self.hide else self.width
            _x, _y = (self.location + Vector((node_width + 20, 0)))
            location = adjust_location(_x, _y, location_theta)

            draw_data = {
                'tree_name': self.id_data.name[:],
                'content': processed_data,
                'location': location,
                'color': self.text_color[:],
                'scale' : float(scale),
                'mode': self.selected_mode[:],
                'font_id': int(self.font_id)
            }
            nvBGL.callback_enable(n_id, draw_data)

    def free(self):
        nvBGL.callback_disable(node_id(self))


    def update(self):
        if not ("Data" in self.inputs):
            return
        try:
            if not self.inputs[0].other:
                nvBGL.callback_disable(node_id(self))        
        except:
            print('stethoscope update holdout (not a problem)')


def register():
    bpy.utils.register_class(SvStethoscopeNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvStethoscopeNodeMK2)
