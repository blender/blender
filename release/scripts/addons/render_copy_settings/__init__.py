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

bl_info = {
    "name": "Copy Settings",
    "author": "Bastien Montagne",
    "version": (0, 1, 6),
    "blender": (2, 65, 9),
    "location": "Render buttons (Properties window)",
    "description": "Allows to copy a selection of render settings "
                   "from current scene to others.",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Render/Copy Settings",
    "category": "Render",
}


if "bpy" in locals():
    import importlib
    importlib.reload(operator)
    importlib.reload(panel)
    importlib.reload(translations)

else:
    from . import (
            operator,
            panel,
            translations,
            )


import bpy
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        CollectionProperty,
        PointerProperty,
        )

########################################################################################################################
# Global properties for the script, for UI (as there’s no way to let them in the operator…).
########################################################################################################################

class RenderCopySettingsDataScene(bpy.types.PropertyGroup):
    allowed = BoolProperty(default=True)


class RenderCopySettingsDataSetting(bpy.types.PropertyGroup):
    strid = StringProperty(default="")
    copy = BoolProperty(default=False)


class RenderCopySettingsData(bpy.types.PropertyGroup):
    # XXX: The consistency of this collection is delegated to the UI code.
    #      It should only contain one element for each render setting.
    affected_settings = CollectionProperty(type=RenderCopySettingsDataSetting,
                                           name="Affected Settings",
                                           description="The list of all available render settings")
    # XXX Unused, but needed for template_list…
    affected_settings_idx = IntProperty()

    # XXX: The consistency of this collection is delegated to the UI code.
    #      It should only contain one element for each scene.
    allowed_scenes = CollectionProperty(type=RenderCopySettingsDataScene,
                                        name="Allowed Scenes",
                                        description="The list all scenes in the file")
    # XXX Unused, but needed for template_list…
    allowed_scenes_idx = IntProperty()

    filter_scene = StringProperty(name="Filter Scene",
                                  description="Regex to only affect scenes which name matches it",
                                  default="")


classes = (
    RenderCopySettingsDataScene,
    RenderCopySettingsDataSetting,
    RenderCopySettingsData,
) + operator.classes + panel.classes


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Scene.render_copy_settings = PointerProperty(type=RenderCopySettingsData)

    bpy.app.translations.register(__name__, translations.translations_dict)


def unregister():
    bpy.app.translations.unregister(__name__)

    del bpy.types.Scene.render_copy_settings
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
