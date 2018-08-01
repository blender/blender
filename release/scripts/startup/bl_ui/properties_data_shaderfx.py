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

# <pep8 compliant>
import bpy
from bpy.types import Panel
from bpy.app.translations import pgettext_iface as iface_


class ShaderFxButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "shaderfx"
    bl_options = {'HIDE_HEADER'}


class DATA_PT_shader_fx(ShaderFxButtonsPanel, Panel):
    bl_label = "Effects"

    @classmethod
    def poll(cls, context):
        return True
        ob = context.object
        return ob and ob.type == 'GPENCIL'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object

        layout.operator_menu_enum("object.shaderfx_add", "type")

        for fx in ob.shader_effects:
            box = layout.template_shaderfx(fx)
            if box:
                # match enum type to our functions, avoids a lookup table.
                getattr(self, fx.type)(box, fx)

    # the mt.type enum is (ab)used for a lookup on function names
    # ...to avoid lengthy if statements
    # so each type must have a function here.

    def FX_BLUR(self, layout, fx):

        layout.prop(fx, "factor", text="Factor")
        layout.prop(fx, "samples", text="Samples")

        layout.separator()
        layout.prop(fx, "use_dof_mode")
        if fx.use_dof_mode:
            layout.prop(fx, "coc")

    def FX_COLORIZE(self, layout, fx):
        layout.prop(fx, "mode", text="Mode")

        if fx.mode == 'BITONE':
            layout.prop(fx, "low_color", text="Low Color")
        if fx.mode == 'CUSTOM':
            layout.prop(fx, "low_color", text="Color")

        if fx.mode == 'BITONE':
            layout.prop(fx, "high_color", text="High Color")

        if fx.mode in {'BITONE', 'CUSTOM', 'TRANSPARENT'}:
            layout.prop(fx, "factor")

    def FX_WAVE(self, layout, fx):
        layout.prop(fx, "orientation", expand=True)

        layout.separator()
        layout.prop(fx, "amplitude")
        layout.prop(fx, "period")
        layout.prop(fx, "phase")

    def FX_PIXEL(self, layout, fx):
        layout.prop(fx, "size", text="Size")

        layout.prop(fx, "use_lines", text="Display Lines")

        col = layout.column()
        col.enabled = fx.use_lines
        col.prop(fx, "color")

    def FX_RIM(self, layout, fx):
        layout.prop(fx, "offset", text="Offset")

        layout.prop(fx, "rim_color")
        layout.prop(fx, "mask_color")
        layout.prop(fx, "mode")
        layout.prop(fx, "blur")
        layout.prop(fx, "samples")

    def FX_SWIRL(self, layout, fx):
        layout.prop(fx, "object", text="Object")

        layout.prop(fx, "radius")
        layout.prop(fx, "angle")

        layout.prop(fx, "transparent")

    def FX_FLIP(self, layout, fx):
        layout.prop(fx, "flip_horizontal")
        layout.prop(fx, "flip_vertical")

    def FX_LIGHT(self, layout, fx):
        layout.prop(fx, "object", text="Object")

        layout.prop(fx, "energy")
        layout.prop(fx, "ambient")


classes = (
    DATA_PT_shader_fx,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
