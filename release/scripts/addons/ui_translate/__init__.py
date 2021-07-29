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
    "name": "Manage UI translations",
    "author": "Bastien Montagne",
    "version": (1, 1, 4),
    "blender": (2, 79, 0),
    "location": "Main \"File\" menu, text editor, any UI control",
    "description": "Allow to manage UI translations directly from Blender "
        "(update main po files, update scripts' translations, etc.)",
    "warning": "Still in development, not all features are fully implemented yet!",
    "wiki_url": "http://wiki.blender.org/index.php/Dev:Doc/How_to/Translate_Blender",
    "support": 'OFFICIAL',
    "category": "System"}


if "bpy" in locals():
    import importlib
    importlib.reload(settings)
    importlib.reload(edit_translation)
    importlib.reload(update_svn)
    importlib.reload(update_addon)
    importlib.reload(update_ui)
else:
    import bpy
    from . import (
            settings,
            edit_translation,
            update_svn,
            update_addon,
            update_ui,
            )


import os


classes = settings.classes + edit_translation.classes + update_svn.classes + update_addon.classes + update_ui.classes


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.WindowManager.i18n_update_svn_settings = \
                    bpy.props.PointerProperty(type=update_ui.I18nUpdateTranslationSettings)

    # Init addon's preferences (unfortunately, as we are using an external storage for the properties,
    # the load/save user preferences process has no effect on them :( ).
    if __name__ in bpy.context.user_preferences.addons:
        pref = bpy.context.user_preferences.addons[__name__].preferences
        if os.path.isfile(pref.persistent_data_path):
            pref._settings.load(pref.persistent_data_path, reset=True)


def unregister():
    del bpy.types.WindowManager.i18n_update_svn_settings
    for cls in classes:
        bpy.utils.unregister_class(cls)
