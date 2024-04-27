# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Panel,
    UIList,
)
from bpy.app.translations import pgettext_iface as iface_

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

        layout.prop(workspace, "use_pin_scene")
        layout.prop(workspace, "object_mode", text="Mode")


class WORKSPACE_PT_addons(WorkSpaceButtonsPanel, Panel):
    bl_label = "Filter Add-ons"
    bl_parent_id = "WORKSPACE_PT_main"
    addon_map = {}
    owner_ids = set()

    def draw_header(self, context):
        workspace = context.workspace
        self.layout.prop(workspace, "use_filter_by_owner", text="")

    def draw(self, context):
        layout = self.layout

        workspace = context.workspace
        prefs = context.preferences

        import addon_utils
        WORKSPACE_PT_addons.addon_map = {mod.__name__: mod for mod in addon_utils.modules()}
        WORKSPACE_PT_addons.owner_ids = {owner_id.name for owner_id in workspace.owner_ids}
        known_addons = set()
        for addon in prefs.addons:
            if addon.module in WORKSPACE_PT_addons.owner_ids:
                known_addons.add(addon.module)
        unknown_addons = WORKSPACE_PT_addons.owner_ids.difference(known_addons)
        layout.template_list(
            "WORKSPACE_UL_addons_items",
            "",
            context.preferences,
            "addons",
            context.workspace,
            "active_addon",
            rows=8,
        )
        # Detect unused
        if unknown_addons:
            layout.label(text="Unknown add-ons", icon='ERROR')
            col = layout.box().column(align=True)
            for addon_module_name in sorted(unknown_addons):
                row = col.row()
                row.alignment = 'LEFT'
                row.operator(
                    "wm.owner_disable",
                    icon='CHECKBOX_HLT',
                    text=addon_module_name,
                    emboss=False,
                ).owner_id = addon_module_name


class WORKSPACE_UL_addons_items(UIList):

    @staticmethod
    def _ui_label_from_addon(addon):
        # Return: `Category: Addon Name` when the add-on is known, otherwise it's module name.
        import addon_utils
        module = WORKSPACE_PT_addons.addon_map.get(addon.module)
        if not module:
            return addon.module
        bl_info = addon_utils.module_bl_info(module)
        return "{!s}: {!s}".format(iface_(bl_info["category"]), iface_(bl_info["name"]))

    @staticmethod
    def _filter_addons_by_category_name(pattern, bitflag, addons, reverse=False):
        # Set FILTER_ITEM for addons which category and name matches filter_name one (case-insensitive).
        # pattern is the filtering pattern.
        # return a list of flags based on given bit flag, or an empty list if no pattern is given
        # or list addons is empty.

        if not pattern or not addons:  # Empty pattern or list = no filtering!
            return []

        import fnmatch
        import re

        # Implicitly add heading/trailing wildcards.
        pattern_regex = re.compile(fnmatch.translate("*" + pattern + "*"), re.IGNORECASE)

        flags = [0] * len(addons)

        for i, addon in enumerate(addons):
            name = WORKSPACE_UL_addons_items._ui_label_from_addon(addon)
            # This is similar to a logical XOR.
            if bool(name and pattern_regex.match(name)) is not reverse:
                flags[i] |= bitflag
        return flags

    @staticmethod
    def _sort_addons_by_category_name(addons):
        # Re-order addons using their categories and names (case-insensitive).
        # return a list mapping org_idx -> new_idx, or an empty list if no sorting has been done.
        _sort = [(idx, WORKSPACE_UL_addons_items._ui_label_from_addon(addon)) for idx, addon in enumerate(addons)]
        return bpy.types.UI_UL_list.sort_items_helper(_sort, lambda e: e[1].lower())

    def filter_items(self, _context, data, property):
        addons = getattr(data, property)
        flags = []
        indices = []

        # Filtering by category and name
        if self.filter_name:
            flags = self._filter_addons_by_category_name(
                self.filter_name, self.bitflag_filter_item, addons, reverse=self.use_filter_invert)
        if not flags:
            flags = [self.bitflag_filter_item] * len(addons)
        # Filer addons without registered modules
        for idx, addon in enumerate(addons):
            if not WORKSPACE_PT_addons.addon_map.get(addon.module):
                flags[idx] = 0
        if self.use_filter_sort_alpha:
            indices = self._sort_addons_by_category_name(addons)
        return flags, indices

    def draw_item(self, context, layout, _data, addon, icon, _active_data, _active_propname, _index):
        row = layout.row()
        row.active = context.workspace.use_filter_by_owner
        row.emboss = 'NONE'
        row.label(text=WORKSPACE_UL_addons_items._ui_label_from_addon(addon))
        row = row.row()
        row.alignment = 'RIGHT'
        is_enabled = addon.module in WORKSPACE_PT_addons.owner_ids
        row.operator(
            "wm.owner_disable" if is_enabled else "wm.owner_enable",
            icon='CHECKBOX_HLT' if is_enabled else 'CHECKBOX_DEHLT',
            text="",
        ).owner_id = addon.module


class WORKSPACE_PT_custom_props(WorkSpaceButtonsPanel, PropertyPanel, Panel):
    bl_parent_id = "WORKSPACE_PT_main"

    _context_path = "workspace"
    _property_type = bpy.types.WorkSpace


classes = (
    WORKSPACE_UL_addons_items,

    WORKSPACE_PT_main,
    WORKSPACE_PT_addons,
    WORKSPACE_PT_custom_props,
)


bpy.types.WorkSpace.active_addon = bpy.props.IntProperty(
    name="Active Add-on", description="Active Add-on in the Workspace Add-ons filter")


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
