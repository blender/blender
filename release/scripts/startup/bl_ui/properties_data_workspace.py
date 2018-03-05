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


class WORKSPACE_PT_owner_ids(WorkSpaceButtonsPanel, Panel):
    bl_label = "Workspace Add-ons"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        workspace = context.workspace
        self.layout.prop(workspace, "use_filter_by_owner", text="")

    def draw(self, context):
        layout = self.layout
        # align just to pack more tightly
        col = layout.box().column(align=True)

        workspace = context.workspace
        userpref = context.user_preferences

        col.active = workspace.use_filter_by_owner

        import addon_utils
        addon_map = {mod.__name__: mod for mod in addon_utils.modules()}
        owner_ids = {owner_id.name  for owner_id in workspace.owner_ids}

        for addon in userpref.addons:
            module_name = addon.module
            info = addon_utils.module_bl_info(addon_map[module_name])
            if not info["use_owner"]:
                continue
            is_enabled = module_name in owner_ids
            row = col.row()
            row.operator(
                "wm.owner_disable" if is_enabled else "wm.owner_enable",
                icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT',
                text="",
                emboss=False,
            ).owner_id = module_name
            row.label("%s: %s" % (info["category"], info["name"]))
            if is_enabled:
                owner_ids.remove(module_name)

        # Detect unused
        if owner_ids:
            layout.label(text="Unknown add-ons", icon='ERROR')
            col = layout.box().column(align=True)
            for module_name in sorted(owner_ids):
                row = col.row()
                row.operator(
                    "wm.owner_disable",
                    icon='CHECKBOX_HLT',
                    text="",
                    emboss=False,
                ).owner_id = module_name
                row.label(module_name)



class WORKSPACE_PT_custom_props(WorkSpaceButtonsPanel, PropertyPanel, Panel):
    _context_path = "workspace"
    _property_type = bpy.types.WorkSpace


classes = (
    WORKSPACE_PT_context,
    WORKSPACE_PT_workspace,
    WORKSPACE_PT_owner_ids,
    WORKSPACE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

