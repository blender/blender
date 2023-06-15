# SPDX-FileCopyrightText: 2018-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Panel


class ShaderFxButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "shaderfx"


class DATA_PT_shader_fx(ShaderFxButtonsPanel, Panel):
    bl_label = "Effects"
    bl_options = {'HIDE_HEADER'}

    # Unused: always show for now.

    # @classmethod
    # def poll(cls, context):
    #     ob = context.object
    #     return ob and ob.type == 'GPENCIL'

    def draw(self, _context):
        layout = self.layout
        layout.operator_menu_enum("object.shaderfx_add", "type")
        layout.template_shaderfx()


classes = (
    DATA_PT_shader_fx,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
