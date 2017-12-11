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
from bpy.types import (
        Panel,
        )

from rna_prop_ui import PropertyPanel


class WorkSpaceButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "workspace"


class WORKSPACE_PT_context(WorkSpaceButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        workspace = context.workspace
        layout.prop(workspace, "use_scene_settings", icon='SCENE')


class WORKSPACE_PT_workspace(WorkSpaceButtonsPanel, Panel):
    bl_label = "Workspace"

    def draw(self, context):
        layout = self.layout

        window = context.window
        workspace = context.workspace
        scene = context.scene
        view_render = workspace.view_render

        layout.enabled = not workspace.use_scene_settings

        layout.template_search(window, "view_layer", scene, "view_layers")

        if view_render.has_multiple_engines:
            layout.prop(view_render, "engine", text="")


class WORKSPACE_PT_custom_props(WorkSpaceButtonsPanel, PropertyPanel, Panel):
    _context_path = "workspace"
    _property_type = bpy.types.WorkSpace


classes = (
    WORKSPACE_PT_context,
    WORKSPACE_PT_workspace,
    WORKSPACE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

