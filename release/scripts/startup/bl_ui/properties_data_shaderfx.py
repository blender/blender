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
from bpy.types import Panel


class ShaderFxButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "shaderfx"
    bl_options = {'HIDE_HEADER'}


class DATA_PT_shader_fx(ShaderFxButtonsPanel, Panel):
    bl_label = "Effects"

    # Unused: always show for now.

    # @classmethod
    # def poll(cls, context):
    #     ob = context.object
    #     return ob and ob.type == 'GPENCIL'

    def draw(self, context):
        layout = self.layout

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

        layout.prop(fx, "use_dof_mode", text="Use as Depth Of Field")
        layout.separator()

        col = layout.column()
        col.enabled = not fx.use_dof_mode
        col.prop(fx, "size", text="Size")
        col.separator()
        col.prop(fx, "rotation")

        layout.prop(fx, "samples", text="Samples")


    def FX_COLORIZE(self, layout, fx):
        layout.prop(fx, "mode", text="Mode")

        if fx.mode == 'DUOTONE':
            layout.prop(fx, "low_color", text="Low Color")
        if fx.mode == 'CUSTOM':
            layout.prop(fx, "low_color", text="Color")

        if fx.mode == 'DUOTONE':
            layout.prop(fx, "high_color", text="High Color")

        layout.prop(fx, "factor")

    def FX_WAVE(self, layout, fx):
        row = layout.row(align=True)
        row.prop(fx, "orientation", expand=True)

        layout.separator()
        layout.prop(fx, "amplitude")
        layout.prop(fx, "period")
        layout.prop(fx, "phase")

    def FX_PIXEL(self, layout, fx):
        layout.prop(fx, "size", text="Size")

    def FX_RIM(self, layout, fx):
        layout.prop(fx, "offset", text="Offset")

        layout.prop(fx, "rim_color")
        layout.prop(fx, "mask_color")
        layout.prop(fx, "mode")
        layout.prop(fx, "blur")
        layout.prop(fx, "samples")

    def FX_SHADOW(self, layout, fx):
        layout.prop(fx, "offset", text="Offset")

        layout.prop(fx, "shadow_color")
        layout.prop(fx, "scale")
        layout.prop(fx, "rotation")

        layout.separator()
        layout.prop(fx, "blur")
        layout.prop(fx, "samples")

        layout.separator()
        layout.prop(fx, "use_object", text="Use Object As Pivot")
        if fx.use_object:
            row = layout.row()
            row.prop(fx, "object", text="Object")

        layout.separator()
        layout.prop(fx, "use_wave", text="Use Wave Effect")
        if fx.use_wave is True:
            row = layout.row(align=True)
            row.prop(fx, "orientation", expand=True)
            layout.prop(fx, "amplitude")
            layout.prop(fx, "period")
            layout.prop(fx, "phase")

    def FX_GLOW(self, layout, fx):
        layout.prop(fx, "mode")
        if fx.mode == 'LUMINANCE':
            layout.prop(fx, "threshold")
        else:
            layout.prop(fx, "select_color")

        layout.prop(fx, "glow_color")
        layout.separator()
        layout.prop(fx, "blend_mode", text="Blend")
        layout.prop(fx, "opacity")

        layout.prop(fx, "size")
        layout.prop(fx, "rotation")
        layout.prop(fx, "samples")

        layout.prop(fx, "use_glow_under", text="Glow Under")

    def FX_SWIRL(self, layout, fx):
        layout.prop(fx, "object", text="Object")

        layout.prop(fx, "radius")
        layout.prop(fx, "angle")

    def FX_FLIP(self, layout, fx):
        layout.prop(fx, "flip_horizontal")
        layout.prop(fx, "flip_vertical")


classes = (
    DATA_PT_shader_fx,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
