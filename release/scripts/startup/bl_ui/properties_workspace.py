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
    # bl_space_type = 'PROPERTIES'
    # bl_region_type = 'WINDOW'
    # bl_context = ".workspace"

    # Developer note: this is displayed in tool settings as well as the 3D view.
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Tool"


class WORKSPACE_PT_main(WorkSpaceButtonsPanel, Panel):
    bl_label = "Workspace"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        workspace = context.workspace

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.prop(workspace, "object_mode", text="Mode")


class WORKSPACE_PT_addons(WorkSpaceButtonsPanel, Panel):
    bl_label = "Filter Add-ons"
    bl_parent_id = "WORKSPACE_PT_main"

    def draw_header(self, context):
        workspace = context.workspace
        self.layout.prop(workspace, "use_filter_by_owner", text="")

    def draw(self, context):
        layout = self.layout
        # align just to pack more tightly
        col = layout.box().column(align=True)

        workspace = context.workspace
        prefs = context.preferences

        col.active = workspace.use_filter_by_owner

        import addon_utils
        addon_map = {mod.__name__: mod for mod in addon_utils.modules()}
        owner_ids = {owner_id.name for owner_id in workspace.owner_ids}

        for addon in prefs.addons:
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
            row.label(text="%s: %s" % (info["category"], info["name"]))
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
                row.label(text=module_name)


class WORKSPACE_PT_custom_props(WorkSpaceButtonsPanel, PropertyPanel, Panel):
    bl_parent_id = "WORKSPACE_PT_main"

    _context_path = "workspace"
    _property_type = bpy.types.WorkSpace


classes = (
    WORKSPACE_PT_main,
    WORKSPACE_PT_addons,
    WORKSPACE_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
