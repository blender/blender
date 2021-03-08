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

# note, properties_animviz is a helper module only.

# support reloading sub-modules
if "bpy" in locals():
    from importlib import reload
    _modules_loaded[:] = [reload(val) for val in _modules_loaded]
    del reload

_modules = [
    "properties_animviz",
    "properties_constraint",
    "properties_data_armature",
    "properties_data_bone",
    "properties_data_camera",
    "properties_data_curve",
    "properties_data_empty",
    "properties_data_gpencil",
    "properties_data_hair",
    "properties_data_light",
    "properties_data_lattice",
    "properties_data_mesh",
    "properties_data_metaball",
    "properties_data_modifier",
    "properties_data_pointcloud",
    "properties_data_shaderfx",
    "properties_data_lightprobe",
    "properties_data_speaker",
    "properties_data_volume",
    "properties_mask_common",
    "properties_material",
    "properties_material_gpencil",
    "properties_object",
    "properties_paint_common",
    "properties_grease_pencil_common",
    "properties_particle",
    "properties_physics_cloth",
    "properties_physics_common",
    "properties_physics_dynamicpaint",
    "properties_physics_field",
    "properties_physics_rigidbody",
    "properties_physics_rigidbody_constraint",
    "properties_physics_fluid",
    "properties_physics_softbody",
    "properties_render",
    "properties_output",
    "properties_view_layer",
    "properties_scene",
    "properties_texture",
    "properties_world",

    # Generic Space Modules
    #
    # Depends on DNA_WORKSPACE_TOOL (C define).
    "space_toolsystem_common",
    "space_toolsystem_toolbar",

    "space_clip",
    "space_console",
    "space_dopesheet",
    "space_filebrowser",
    "space_graph",
    "space_image",
    "space_info",
    "space_nla",
    "space_node",
    "space_outliner",
    "space_properties",
    "space_sequencer",
    "space_spreadsheet",
    "space_statusbar",
    "space_text",
    "space_time",
    "space_topbar",
    "space_userpref",
    "space_view3d",
    "space_view3d_toolbar",

    # XXX, keep last so panels show after all other tool options.
    "properties_workspace",
]

import bpy

if bpy.app.build_options.freestyle:
    _modules.append("properties_freestyle")

__import__(name=__name__, fromlist=_modules)
_namespace = globals()
_modules_loaded = [_namespace[name] for name in _modules]
del _namespace


def register():
    from bpy.utils import register_class
    for mod in _modules_loaded:
        for cls in mod.classes:
            register_class(cls)

    # space_userprefs.py
    from bpy.props import (
        EnumProperty,
        StringProperty,
    )
    from bpy.types import WindowManager

    def addon_filter_items(_self, _context):
        import addon_utils

        items = [
            ('All', "All", "All Add-ons"),
            ('User', "User", "All Add-ons Installed by User"),
        ]

        items_unique = set()

        for mod in addon_utils.modules(refresh=False):
            info = addon_utils.module_bl_info(mod)
            items_unique.add(info["category"])

        items.extend([(cat, cat, "") for cat in sorted(items_unique)])
        return items

    WindowManager.addon_search = StringProperty(
        name="Search",
        description="Search within the selected filter",
        options={'TEXTEDIT_UPDATE'},
    )
    WindowManager.addon_filter = EnumProperty(
        items=addon_filter_items,
        name="Category",
        description="Filter add-ons by category",
    )

    WindowManager.addon_support = EnumProperty(
        items=[
            ('OFFICIAL', "Official", "Officially supported"),
            ('COMMUNITY', "Community", "Maintained by community developers"),
            ('TESTING', "Testing", "Newly contributed scripts (excluded from release builds)")
        ],
        name="Support",
        description="Display support level",
        default={'OFFICIAL', 'COMMUNITY'},
        options={'ENUM_FLAG'},
    )
    # done...


def unregister():
    from bpy.utils import unregister_class
    for mod in reversed(_modules_loaded):
        for cls in reversed(mod.classes):
            if cls.is_registered:
                unregister_class(cls)

# Define a default UIList, when a list does not need any custom drawing...
# Keep in sync with its #defined name in UI_interface.h


class UI_UL_list(bpy.types.UIList):
    # These are common filtering or ordering operations (same as the default C ones!).
    @staticmethod
    def filter_items_by_name(pattern, bitflag, items, propname="name", flags=None, reverse=False):
        """
        Set FILTER_ITEM for items which name matches filter_name one (case-insensitive).
        pattern is the filtering pattern.
        propname is the name of the string property to use for filtering.
        flags must be a list of integers the same length as items, or None!
        return a list of flags (based on given flags if not None),
        or an empty list if no flags were given and no filtering has been done.
        """
        import fnmatch

        if not pattern or not items:  # Empty pattern or list = no filtering!
            return flags or []

        if flags is None:
            flags = [0] * len(items)

        # Implicitly add heading/trailing wildcards.
        pattern = "*" + pattern + "*"

        for i, item in enumerate(items):
            name = getattr(item, propname, None)
            # This is similar to a logical xor
            if bool(name and fnmatch.fnmatch(name, pattern)) is not bool(reverse):
                flags[i] |= bitflag
        return flags

    @staticmethod
    def sort_items_helper(sort_data, key, reverse=False):
        """
        Common sorting utility. Returns a neworder list mapping org_idx -> new_idx.
        sort_data must be an (unordered) list of tuples [(org_idx, ...), (org_idx, ...), ...].
        key must be the same kind of callable you would use for sorted() builtin function.
        reverse will reverse the sorting!
        """
        sort_data.sort(key=key, reverse=reverse)
        neworder = [None] * len(sort_data)
        for newidx, (orgidx, *_) in enumerate(sort_data):
            neworder[orgidx] = newidx
        return neworder

    @classmethod
    def sort_items_by_name(cls, items, propname="name"):
        """
        Re-order items using their names (case-insensitive).
        propname is the name of the string property to use for sorting.
        return a list mapping org_idx -> new_idx,
               or an empty list if no sorting has been done.
        """
        _sort = [(idx, getattr(it, propname, "")) for idx, it in enumerate(items)]
        return cls.sort_items_helper(_sort, lambda e: e[1].lower())


bpy.utils.register_class(UI_UL_list)
