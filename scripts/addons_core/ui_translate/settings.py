# SPDX-FileCopyrightText: 2013-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os

if "bpy" in locals():
    import importlib
    importlib.reload(settings_i18n)
else:
    import bpy
    from bpy.types import (
        Operator,
        AddonPreferences,
    )
    from bpy.props import (
        BoolProperty,
        StringProperty,
    )
    from _bl_i18n_utils import settings as settings_i18n


settings = settings_i18n.I18nSettings()


# Operators ###################################################################

class UI_OT_i18n_settings_load(Operator):
    """Load translations' settings from a persistent JSON file"""
    bl_idname = "ui.i18n_settings_load"
    bl_label = "I18n Load Settings"
    bl_option = {'REGISTER'}

    # Operator Arguments
    filepath: StringProperty(
        subtype='FILE_PATH',
        description="Path to the saved settings file",
    )

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'}
    )
    # /End Operator Arguments

    def invoke(self, context, event):
        if not self.properties.is_property_set("filepath"):
            context.window_manager.fileselect_add(self)
            return {'RUNNING_MODAL'}
        else:
            return self.execute(context)

    def execute(self, context):
        if not (self.filepath and settings):
            return {'CANCELLED'}
        settings.load(self.filepath, reset=True)
        return {'FINISHED'}


class UI_OT_i18n_settings_save(Operator):
    """Save translations' settings in a persistent JSON file"""
    bl_idname = "ui.i18n_settings_save"
    bl_label = "I18n Save Settings"
    bl_option = {'REGISTER'}

    # Operator Arguments
    filepath: StringProperty(
        description="Path to the saved settings file",
        subtype='FILE_PATH',
    )

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'},
    )
    # /End Operator Arguments

    def invoke(self, context, event):
        if not self.properties.is_property_set("filepath"):
            context.window_manager.fileselect_add(self)
            return {'RUNNING_MODAL'}
        else:
            return self.execute(context)

    def execute(self, context):
        if not (self.filepath and settings):
            return {'CANCELLED'}
        settings.save(self.filepath)
        return {'FINISHED'}


# Addon Preferences ###########################################################

def _setattr(self, name, val):
    print(self, name, val)
    setattr(self, name, val)


class UI_AP_i18n_settings(AddonPreferences):
    bl_idname = __name__.split(".")[0]  # We want "top" module name!
    bl_option = {'REGISTER'}

    _settings = settings

    WARN_MSGID_NOT_CAPITALIZED: BoolProperty(
        name="Warn Msgid Not Capitalized",
        description="Warn about messages not starting by a capitalized letter (with a few allowed exceptions!)",
        default=True,
        get=lambda self: self._settings.WARN_MSGID_NOT_CAPITALIZED,
        set=lambda self, val: _setattr(self._settings, "WARN_MSGID_NOT_CAPITALIZED", val),
    )

    FRIBIDI_LIB: StringProperty(
        name="Fribidi Library",
        description="The FriBidi C compiled library (.so under Linux, .dll under windows...), you’ll likely have "
                    "to edit it if you’re under Windows, e.g. using the one included in Blender libraries repository",
        subtype='FILE_PATH',
        default="libfribidi.so.0",
        get=lambda self: self._settings.FRIBIDI_LIB,
        set=lambda self, val: setattr(self._settings, "FRIBIDI_LIB", val),
    )

    SOURCE_DIR: StringProperty(
        name="Source Root",
        description="The Blender source root path",
        subtype='FILE_PATH',
        default="blender",
        get=lambda self: self._settings.SOURCE_DIR,
        set=lambda self, val: setattr(self._settings, "SOURCE_DIR", val),
    )

    I18N_DIR: StringProperty(
        name="Translation Root",
        description="The bf-translation repository",
        subtype='FILE_PATH',
        default="i18n",
        get=lambda self: self._settings.I18N_DIR,
        set=lambda self, val: setattr(self._settings, "I18N_DIR", val),
    )

    SPELL_CACHE: StringProperty(
        name="Spell Cache",
        description="A cache storing validated msgids, to avoid re-spellchecking them",
        subtype='FILE_PATH',
        default=os.path.join("/tmp", ".spell_cache"),
        get=lambda self: self._settings.SPELL_CACHE,
        set=lambda self, val: setattr(self._settings, "SPELL_CACHE", val),
    )

    PY_SYS_PATHS: StringProperty(
        name="Import Paths",
        description="Additional paths to add to sys.path (';' separated)",
        default="",
        get=lambda self: self._settings.PY_SYS_PATHS,
        set=lambda self, val: setattr(self._settings, "PY_SYS_PATHS", val),
    )

    persistent_data_path: StringProperty(
        name="Persistent Data Path",
        description="The name of a json file storing those settings (unfortunately, Blender's system "
                    "does not work here)",
        subtype='FILE_PATH',
        default=os.path.join("ui_translate_settings.json"),
    )
    _is_init = False

    def draw(self, context):
        layout = self.layout
        layout.label(text="WARNING: preferences are lost when add-on is disabled, be sure to use \"Save Persistent\" "
                          "if you want to keep your settings!")
        layout.prop(self, "WARN_MSGID_NOT_CAPITALIZED")
        layout.prop(self, "FRIBIDI_LIB")
        layout.prop(self, "SOURCE_DIR")
        layout.prop(self, "I18N_DIR")
        layout.prop(self, "SPELL_CACHE")
        layout.prop(self, "PY_SYS_PATHS")

        layout.separator()
        split = layout.split(factor=0.75)
        col = split.column()
        col.prop(self, "persistent_data_path")
        row = col.row()
        row.operator("ui.i18n_settings_save", text="Save").filepath = self.persistent_data_path
        row.operator("ui.i18n_settings_load", text="Load").filepath = self.persistent_data_path
        col = split.column()
        col.operator("ui.i18n_settings_save", text="Save Persistent To...")
        col.operator("ui.i18n_settings_load", text="Load Persistent From...")


classes = (
    UI_OT_i18n_settings_load,
    UI_OT_i18n_settings_save,
    UI_AP_i18n_settings,
)
