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

if "bpy" in locals():
    from imp import reload as _reload
    for val in _modules_loaded.values():
        _reload(val)
_modules = (
    "properties_animviz",
    "properties_data_armature",
    "properties_data_bone",
    "properties_data_camera",
    "properties_data_curve",
    "properties_data_empty",
    "properties_data_lamp",
    "properties_data_lattice",
    "properties_data_mesh",
    "properties_data_metaball",
    "properties_data_modifier",
    "properties_data_speaker",
    "properties_game",
    "properties_mask_common",
    "properties_material",
    "properties_object_constraint",
    "properties_object",
    "properties_particle",
    "properties_physics_cloth",
    "properties_physics_common",
    "properties_physics_dynamicpaint",
    "properties_physics_field",
    "properties_physics_fluid",
    "properties_physics_smoke",
    "properties_physics_softbody",
    "properties_render",
    "properties_scene",
    "properties_texture",
    "properties_world",
    "space_clip",
    "space_console",
    "space_dopesheet",
    "space_filebrowser",
    "space_graph",
    "space_image",
    "space_info",
    "space_logic",
    "space_nla",
    "space_node",
    "space_outliner",
    "space_sequencer",
    "space_text",
    "space_time",
    "space_userpref_keymap",
    "space_userpref",
    "space_view3d",
    "space_view3d_toolbar",
)
__import__(name=__name__, fromlist=_modules)
_namespace = globals()
_modules_loaded = {name: _namespace[name] for name in _modules}
del _namespace


import bpy


def register():
    bpy.utils.register_module(__name__)

    # space_userprefs.py
    from bpy.props import StringProperty, EnumProperty
    from bpy.types import WindowManager

    def addon_filter_items(self, context):
        import addon_utils

        items = [('All', "All", ""),
                 ('Enabled', "Enabled", ""),
                 ('Disabled', "Disabled", ""),
                ]

        items_unique = set()

        for mod in addon_utils.modules(addon_utils.addons_fake_modules):
            info = addon_utils.module_bl_info(mod)
            items_unique.add(info["category"])

        items.extend([(cat, cat, "") for cat in sorted(items_unique)])
        return items

    WindowManager.addon_search = StringProperty(
            name="Search",
            description="Search within the selected filter",
            )
    WindowManager.addon_filter = EnumProperty(
            items=addon_filter_items,
            name="Category",
            description="Filter addons by category",
            )

    WindowManager.addon_support = EnumProperty(
            items=[('OFFICIAL', "Official", "Officially supported"),
                   ('COMMUNITY', "Community", "Maintained by community developers"),
                   ('TESTING', "Testing", "Newly contributed scripts (excluded from release builds)"),
                  ],
            name="Support",
            description="Display support level",
            default={'OFFICIAL', 'COMMUNITY'},
            options={'ENUM_FLAG'},
            )
    # done...


def unregister():
    bpy.utils.unregister_module(__name__)
