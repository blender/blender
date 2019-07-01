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

# TODO, use PREFERENCES_OT_* prefix for operators.

import bpy
from bpy.types import (
    Operator,
    OperatorFileListElement
)
from bpy.props import (
    BoolProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
    CollectionProperty,
)

from bpy.app.translations import pgettext_tip as tip_


def module_filesystem_remove(path_base, module_name):
    import os
    module_name = os.path.splitext(module_name)[0]
    for f in os.listdir(path_base):
        f_base = os.path.splitext(f)[0]
        if f_base == module_name:
            f_full = os.path.join(path_base, f)

            if os.path.isdir(f_full):
                os.rmdir(f_full)
            else:
                os.remove(f_full)


class PREFERENCES_OT_keyconfig_activate(Operator):
    bl_idname = "preferences.keyconfig_activate"
    bl_label = "Activate Keyconfig"

    filepath: StringProperty(
        subtype='FILE_PATH',
    )

    def execute(self, _context):
        if bpy.utils.keyconfig_set(self.filepath, report=self.report):
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class PREFERENCES_OT_copy_prev(Operator):
    """Copy settings from previous version"""
    bl_idname = "preferences.copy_prev"
    bl_label = "Copy Previous Settings"

    @staticmethod
    def previous_version():
        ver = bpy.app.version
        ver_old = ((ver[0] * 100) + ver[1]) - 1
        return ver_old // 100, ver_old % 100

    @staticmethod
    def _old_path():
        ver = bpy.app.version
        ver_old = ((ver[0] * 100) + ver[1]) - 1
        return bpy.utils.resource_path('USER', ver_old // 100, ver_old % 100)

    @staticmethod
    def _new_path():
        return bpy.utils.resource_path('USER')

    @classmethod
    def poll(cls, _context):
        import os

        old = cls._old_path()
        new = cls._new_path()

        # Disable operator in case config path is overriden with environment
        # variable. That case has no automatic per-version configuration.
        userconfig_path = os.path.normpath(bpy.utils.user_resource('CONFIG'))
        new_userconfig_path = os.path.normpath(os.path.join(new, "config"))
        if userconfig_path != new_userconfig_path:
            return False

        # Enable operator if new config path does not exist yet.
        if os.path.isdir(old) and not os.path.isdir(new):
            return True

        # Enable operator also if there are no new user preference yet.
        old_userpref = os.path.join(old, "config", "userpref.blend")
        new_userpref = os.path.join(new, "config", "userpref.blend")
        return os.path.isfile(old_userpref) and not os.path.isfile(new_userpref)

    def execute(self, _context):
        import shutil

        shutil.copytree(self._old_path(), self._new_path(), symlinks=True)

        # reload recent-files.txt
        bpy.ops.wm.read_history()

        # don't loose users work if they open the splash later.
        if bpy.data.is_saved is bpy.data.is_dirty is False:
            bpy.ops.wm.read_homefile()
        else:
            self.report({'INFO'}, "Reload Start-Up file to restore settings")

        return {'FINISHED'}


class PREFERENCES_OT_keyconfig_test(Operator):
    """Test key-config for conflicts"""
    bl_idname = "preferences.keyconfig_test"
    bl_label = "Test Key Configuration for Conflicts"

    def execute(self, context):
        from bpy_extras import keyconfig_utils

        wm = context.window_manager
        kc = wm.keyconfigs.default

        if keyconfig_utils.keyconfig_test(kc):
            print("CONFLICT")

        return {'FINISHED'}


class PREFERENCES_OT_keyconfig_import(Operator):
    """Import key configuration from a python script"""
    bl_idname = "preferences.keyconfig_import"
    bl_label = "Import Key Configuration..."

    filepath: StringProperty(
        subtype='FILE_PATH',
        default="keymap.py",
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )
    keep_original: BoolProperty(
        name="Keep original",
        description="Keep original file after copying to configuration folder",
        default=True,
    )

    def execute(self, _context):
        import os
        from os.path import basename
        import shutil

        if not self.filepath:
            self.report({'ERROR'}, "Filepath not set")
            return {'CANCELLED'}

        config_name = basename(self.filepath)

        path = bpy.utils.user_resource('SCRIPTS', os.path.join("presets", "keyconfig"), create=True)
        path = os.path.join(path, config_name)

        try:
            if self.keep_original:
                shutil.copy(self.filepath, path)
            else:
                shutil.move(self.filepath, path)
        except Exception as ex:
            self.report({'ERROR'}, "Installing keymap failed: %s" % ex)
            return {'CANCELLED'}

        # sneaky way to check we're actually running the code.
        if bpy.utils.keyconfig_set(path, report=self.report):
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

# This operator is also used by interaction presets saving - AddPresetBase


class PREFERENCES_OT_keyconfig_export(Operator):
    """Export key configuration to a python script"""
    bl_idname = "preferences.keyconfig_export"
    bl_label = "Export Key Configuration..."

    all: BoolProperty(
        name="All Keymaps",
        default=False,
        description="Write all keymaps (not just user modified)",
    )
    filepath: StringProperty(
        subtype='FILE_PATH',
        default="keymap.py",
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )

    def execute(self, context):
        from bl_keymap_utils.io import keyconfig_export_as_data

        if not self.filepath:
            raise Exception("Filepath not set")

        if not self.filepath.endswith(".py"):
            self.filepath += ".py"

        wm = context.window_manager

        keyconfig_export_as_data(
            wm,
            wm.keyconfigs.active,
            self.filepath,
            all_keymaps=self.all,
        )

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PREFERENCES_OT_keymap_restore(Operator):
    """Restore key map(s)"""
    bl_idname = "preferences.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all: BoolProperty(
        name="All Keymaps",
        description="Restore all keymaps to default",
    )

    def execute(self, context):
        wm = context.window_manager

        if self.all:
            for km in wm.keyconfigs.user.keymaps:
                km.restore_to_default()
        else:
            km = context.keymap
            km.restore_to_default()

        context.preferences.is_dirty = True
        return {'FINISHED'}


class PREFERENCES_OT_keyitem_restore(Operator):
    """Restore key map item"""
    bl_idname = "preferences.keyitem_restore"
    bl_label = "Restore Key Map Item"

    item_id: IntProperty(
        name="Item Identifier",
        description="Identifier of the item to remove",
    )

    @classmethod
    def poll(cls, context):
        keymap = getattr(context, "keymap", None)
        return keymap

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)

        if (not kmi.is_user_defined) and kmi.is_user_modified:
            km.restore_item_to_default(kmi)

        return {'FINISHED'}


class PREFERENCES_OT_keyitem_add(Operator):
    """Add key map item"""
    bl_idname = "preferences.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        km = context.keymap

        if km.is_modal:
            km.keymap_items.new_modal("", 'A', 'PRESS')
        else:
            km.keymap_items.new("none", 'A', 'PRESS')

        # clear filter and expand keymap so we can see the newly added item
        if context.space_data.filter_text != "":
            context.space_data.filter_text = ""
            km.show_expanded_items = True
            km.show_expanded_children = True

        context.preferences.is_dirty = True
        return {'FINISHED'}


class PREFERENCES_OT_keyitem_remove(Operator):
    """Remove key map item"""
    bl_idname = "preferences.keyitem_remove"
    bl_label = "Remove Key Map Item"

    item_id: IntProperty(
        name="Item Identifier",
        description="Identifier of the item to remove",
    )

    @classmethod
    def poll(cls, context):
        return hasattr(context, "keymap")

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)
        km.keymap_items.remove(kmi)

        context.preferences.is_dirty = True
        return {'FINISHED'}


class PREFERENCES_OT_keyconfig_remove(Operator):
    """Remove key config"""
    bl_idname = "preferences.keyconfig_remove"
    bl_label = "Remove Key Config"

    @classmethod
    def poll(cls, context):
        wm = context.window_manager
        keyconf = wm.keyconfigs.active
        return keyconf and keyconf.is_user_defined

    def execute(self, context):
        wm = context.window_manager
        keyconfig = wm.keyconfigs.active
        wm.keyconfigs.remove(keyconfig)
        return {'FINISHED'}


# -----------------------------------------------------------------------------
# Add-on Operators

class PREFERENCES_OT_addon_enable(Operator):
    """Enable an add-on"""
    bl_idname = "preferences.addon_enable"
    bl_label = "Enable Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to enable",
    )

    def execute(self, _context):
        import addon_utils

        err_str = ""

        def err_cb(ex):
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        mod = addon_utils.enable(self.module, default_set=True, handle_error=err_cb)

        if mod:
            info = addon_utils.module_bl_info(mod)

            info_ver = info.get("blender", (0, 0, 0))

            if info_ver > bpy.app.version:
                self.report(
                    {'WARNING'},
                    "This script was written Blender "
                    "version %d.%d.%d and might not "
                    "function (correctly), "
                    "though it is enabled" %
                    info_ver
                )
            return {'FINISHED'}
        else:

            if err_str:
                self.report({'ERROR'}, err_str)

            return {'CANCELLED'}


class PREFERENCES_OT_addon_disable(Operator):
    """Disable an add-on"""
    bl_idname = "preferences.addon_disable"
    bl_label = "Disable Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to disable",
    )

    def execute(self, _context):
        import addon_utils

        err_str = ""

        def err_cb(ex):
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        addon_utils.disable(self.module, default_set=True, handle_error=err_cb)

        if err_str:
            self.report({'ERROR'}, err_str)

        return {'FINISHED'}


class PREFERENCES_OT_theme_install(Operator):
    """Load and apply a Blender XML theme file"""
    bl_idname = "preferences.theme_install"
    bl_label = "Install Theme..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing theme file if exists",
        default=True,
    )
    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.xml",
        options={'HIDDEN'},
    )

    def execute(self, _context):
        import os
        import shutil
        import traceback

        xmlfile = self.filepath

        path_themes = bpy.utils.user_resource('SCRIPTS', "presets/interface_theme", create=True)

        if not path_themes:
            self.report({'ERROR'}, "Failed to get themes path")
            return {'CANCELLED'}

        path_dest = os.path.join(path_themes, os.path.basename(xmlfile))

        if not self.overwrite:
            if os.path.exists(path_dest):
                self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                return {'CANCELLED'}

        try:
            shutil.copyfile(xmlfile, path_dest)
            bpy.ops.script.execute_preset(
                filepath=path_dest,
                menu_idname="USERPREF_MT_interface_theme_presets",
            )

        except:
            traceback.print_exc()
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PREFERENCES_OT_addon_refresh(Operator):
    """Scan add-on directories for new modules"""
    bl_idname = "preferences.addon_refresh"
    bl_label = "Refresh"

    def execute(self, _context):
        import addon_utils

        addon_utils.modules_refresh()

        return {'FINISHED'}


# Note: shares some logic with PREFERENCES_OT_app_template_install
# but not enough to de-duplicate. Fixed here may apply to both.
class PREFERENCES_OT_addon_install(Operator):
    """Install an add-on"""
    bl_idname = "preferences.addon_install"
    bl_label = "Install Add-on from File..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing add-ons with the same ID",
        default=True,
    )
    target: EnumProperty(
        name="Target Path",
        items=(
            ('DEFAULT', "Default", ""),
            ('PREFS', "User Prefs", ""),
        ),
    )

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.py;*.zip",
        options={'HIDDEN'},
    )

    def execute(self, context):
        import addon_utils
        import traceback
        import zipfile
        import shutil
        import os

        pyfile = self.filepath

        if self.target == 'DEFAULT':
            # don't use bpy.utils.script_paths("addons") because we may not be able to write to it.
            path_addons = bpy.utils.user_resource('SCRIPTS', "addons", create=True)
        else:
            path_addons = context.preferences.filepaths.script_directory
            if path_addons:
                path_addons = os.path.join(path_addons, "addons")

        if not path_addons:
            self.report({'ERROR'}, "Failed to get add-ons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_addons):
            try:
                os.makedirs(path_addons, exist_ok=True)
            except:
                traceback.print_exc()

        # Check if we are installing from a target path,
        # doing so causes 2+ addons of same name or when the same from/to
        # location is used, removal of the file!
        addon_path = ""
        pyfile_dir = os.path.dirname(pyfile)
        for addon_path in addon_utils.paths():
            if os.path.samefile(pyfile_dir, addon_path):
                self.report({'ERROR'}, "Source file is in the add-on search path: %r" % addon_path)
                return {'CANCELLED'}
        del addon_path
        del pyfile_dir
        # done checking for exceptional case

        addons_old = {mod.__name__ for mod in addon_utils.modules()}

        # check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(pyfile):
            try:
                file_to_extract = zipfile.ZipFile(pyfile, 'r')
            except:
                traceback.print_exc()
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract.namelist():
                    module_filesystem_remove(path_addons, f)
            else:
                for f in file_to_extract.namelist():
                    path_dest = os.path.join(path_addons, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                        return {'CANCELLED'}

            try:  # extract the file to "addons"
                file_to_extract.extractall(path_addons)
            except:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            path_dest = os.path.join(path_addons, os.path.basename(pyfile))

            if self.overwrite:
                module_filesystem_remove(path_addons, os.path.basename(pyfile))
            elif os.path.exists(path_dest):
                self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                return {'CANCELLED'}

            # if not compressed file just copy into the addon path
            try:
                shutil.copyfile(pyfile, path_dest)
            except:
                traceback.print_exc()
                return {'CANCELLED'}

        addons_new = {mod.__name__ for mod in addon_utils.modules()} - addons_old
        addons_new.discard("modules")

        # disable any addons we may have enabled previously and removed.
        # this is unlikely but do just in case. bug [#23978]
        for new_addon in addons_new:
            addon_utils.disable(new_addon, default_set=True)

        # possible the zip contains multiple addons, we could disallow this
        # but for now just use the first
        for mod in addon_utils.modules(refresh=False):
            if mod.__name__ in addons_new:
                info = addon_utils.module_bl_info(mod)

                # show the newly installed addon.
                context.window_manager.addon_filter = 'All'
                context.window_manager.addon_search = info["name"]
                break

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = (
            tip_("Modules Installed (%s) from %r into %r") %
            (", ".join(sorted(addons_new)), pyfile, path_addons)
        )
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PREFERENCES_OT_addon_remove(Operator):
    """Delete the add-on from the file system"""
    bl_idname = "preferences.addon_remove"
    bl_label = "Remove Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to remove",
    )

    @staticmethod
    def path_from_addon(module):
        import os
        import addon_utils

        for mod in addon_utils.modules():
            if mod.__name__ == module:
                filepath = mod.__file__
                if os.path.exists(filepath):
                    if os.path.splitext(os.path.basename(filepath))[0] == "__init__":
                        return os.path.dirname(filepath), True
                    else:
                        return filepath, False
        return None, False

    def execute(self, context):
        import addon_utils
        import os

        path, isdir = PREFERENCES_OT_addon_remove.path_from_addon(self.module)
        if path is None:
            self.report({'WARNING'}, "Add-on path %r could not be found" % path)
            return {'CANCELLED'}

        # in case its enabled
        addon_utils.disable(self.module, default_set=True)

        import shutil
        if isdir and (not os.path.islink(path)):
            shutil.rmtree(path)
        else:
            os.remove(path)

        addon_utils.modules_refresh()

        context.area.tag_redraw()
        return {'FINISHED'}

    # lame confirmation check
    def draw(self, _context):
        self.layout.label(text="Remove Add-on: %r?" % self.module)
        path, _isdir = PREFERENCES_OT_addon_remove.path_from_addon(self.module)
        self.layout.label(text="Path: %r" % path)

    def invoke(self, context, _event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


class PREFERENCES_OT_addon_expand(Operator):
    """Display information and preferences for this add-on"""
    bl_idname = "preferences.addon_expand"
    bl_label = ""
    bl_options = {'INTERNAL'}

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to expand",
    )

    def execute(self, _context):
        import addon_utils

        module_name = self.module

        mod = addon_utils.addons_fake_modules.get(module_name)
        if mod is not None:
            info = addon_utils.module_bl_info(mod)
            info["show_expanded"] = not info["show_expanded"]

        return {'FINISHED'}


class PREFERENCES_OT_addon_show(Operator):
    """Show add-on preferences"""
    bl_idname = "preferences.addon_show"
    bl_label = ""
    bl_options = {'INTERNAL'}

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to expand",
    )

    def execute(self, context):
        import addon_utils

        module_name = self.module

        _modules = addon_utils.modules(refresh=False)
        mod = addon_utils.addons_fake_modules.get(module_name)
        if mod is not None:
            info = addon_utils.module_bl_info(mod)
            info["show_expanded"] = True

            context.preferences.active_section = 'ADDONS'
            context.window_manager.addon_filter = 'All'
            context.window_manager.addon_search = info["name"]
            bpy.ops.screen.userpref_show('INVOKE_DEFAULT')

        return {'FINISHED'}


# Note: shares some logic with PREFERENCES_OT_addon_install
# but not enough to de-duplicate. Fixes here may apply to both.
class PREFERENCES_OT_app_template_install(Operator):
    """Install an application-template"""
    bl_idname = "preferences.app_template_install"
    bl_label = "Install Template from File..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing template with the same ID",
        default=True,
    )

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.zip",
        options={'HIDDEN'},
    )

    def execute(self, _context):
        import traceback
        import zipfile
        import os

        filepath = self.filepath

        path_app_templates = bpy.utils.user_resource(
            'SCRIPTS', os.path.join("startup", "bl_app_templates_user"),
            create=True,
        )

        if not path_app_templates:
            self.report({'ERROR'}, "Failed to get add-ons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_app_templates):
            try:
                os.makedirs(path_app_templates, exist_ok=True)
            except:
                traceback.print_exc()

        app_templates_old = set(os.listdir(path_app_templates))

        # check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(filepath):
            try:
                file_to_extract = zipfile.ZipFile(filepath, 'r')
            except:
                traceback.print_exc()
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract.namelist():
                    module_filesystem_remove(path_app_templates, f)
            else:
                for f in file_to_extract.namelist():
                    path_dest = os.path.join(path_app_templates, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                        return {'CANCELLED'}

            try:  # extract the file to "bl_app_templates_user"
                file_to_extract.extractall(path_app_templates)
            except:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            # Only support installing zipfiles
            self.report({'WARNING'}, "Expected a zip-file %r\n" % filepath)
            return {'CANCELLED'}

        app_templates_new = set(os.listdir(path_app_templates)) - app_templates_old

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = (
            tip_("Template Installed (%s) from %r into %r") %
            (", ".join(sorted(app_templates_new)), filepath, path_app_templates)
        )
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


# -----------------------------------------------------------------------------
# Studio Light Operations

class PREFERENCES_OT_studiolight_install(Operator):
    """Install a user defined studio light"""
    bl_idname = "preferences.studiolight_install"
    bl_label = "Install Custom Studio Light"

    files: CollectionProperty(
        name="File Path",
        type=OperatorFileListElement,
    )
    directory: StringProperty(
        subtype='DIR_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.png;*.jpg;*.hdr;*.exr",
        options={'HIDDEN'},
    )
    type: EnumProperty(
        items=(
            ('MATCAP', "MatCap", ""),
            ('WORLD', "World", ""),
            ('STUDIO', "Studio", ""),
        )
    )

    def execute(self, context):
        import os
        import shutil
        prefs = context.preferences

        path_studiolights = os.path.join("studiolights", self.type.lower())
        path_studiolights = bpy.utils.user_resource('DATAFILES', path_studiolights, create=True)
        if not path_studiolights:
            self.report({'ERROR'}, "Failed to create Studio Light path")
            return {'CANCELLED'}

        for e in self.files:
            shutil.copy(os.path.join(self.directory, e.name), path_studiolights)
            prefs.studio_lights.load(os.path.join(path_studiolights, e.name), self.type)

        # print message
        msg = (
            tip_("StudioLight Installed %r into %r") %
            (", ".join(e.name for e in self.files), path_studiolights)
        )
        print(msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager

        if self.type == 'STUDIO':
            self.filter_glob = "*.sl"

        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PREFERENCES_OT_studiolight_new(Operator):
    """Save custom studio light from the studio light editor settings"""
    bl_idname = "preferences.studiolight_new"
    bl_label = "Save Custom Studio Light"

    filename: StringProperty(
        name="Name",
        default="StudioLight",
    )

    ask_overide = False

    def execute(self, context):
        import os
        prefs = context.preferences
        wm = context.window_manager
        filename = bpy.path.ensure_ext(self.filename, ".sl")

        path_studiolights = bpy.utils.user_resource('DATAFILES', os.path.join("studiolights", "studio"), create=True)
        if not path_studiolights:
            self.report({'ERROR'}, "Failed to get Studio Light path")
            return {'CANCELLED'}

        filepath_final = os.path.join(path_studiolights, filename)
        if os.path.isfile(filepath_final):
            if not self.ask_overide:
                self.ask_overide = True
                return wm.invoke_props_dialog(self, width=600)
            else:
                for studio_light in prefs.studio_lights:
                    if studio_light.name == filename:
                        bpy.ops.preferences.studiolight_uninstall(index=studio_light.index)

        prefs.studio_lights.new(path=filepath_final)

        # print message
        msg = (
            tip_("StudioLight Installed %r into %r") %
            (self.filename, str(path_studiolights))
        )
        print(msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}

    def draw(self, _context):
        layout = self.layout
        if self.ask_overide:
            layout.label(text="Warning, file already exists. Overwrite existing file?")
        else:
            layout.prop(self, "filename")

    def invoke(self, context, _event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


class PREFERENCES_OT_studiolight_uninstall(Operator):
    """Delete Studio Light"""
    bl_idname = "preferences.studiolight_uninstall"
    bl_label = "Uninstall Studio Light"
    index: bpy.props.IntProperty()

    def execute(self, context):
        import os
        prefs = context.preferences
        for studio_light in prefs.studio_lights:
            if studio_light.index == self.index:
                for filepath in (
                        studio_light.path,
                        studio_light.path_irr_cache,
                        studio_light.path_sh_cache,
                ):
                    if filepath and os.path.exists(filepath):
                        os.unlink(filepath)
                prefs.studio_lights.remove(studio_light)
                return {'FINISHED'}
        return {'CANCELLED'}


class PREFERENCES_OT_studiolight_copy_settings(Operator):
    """Copy Studio Light settings to the Studio light editor"""
    bl_idname = "preferences.studiolight_copy_settings"
    bl_label = "Copy Studio Light settings"
    index: bpy.props.IntProperty()

    def execute(self, context):
        prefs = context.preferences
        system = prefs.system
        for studio_light in prefs.studio_lights:
            if studio_light.index == self.index:
                system.light_ambient = studio_light.light_ambient
                for sys_light, light in zip(system.solid_lights, studio_light.solid_lights):
                    sys_light.use = light.use
                    sys_light.diffuse_color = light.diffuse_color
                    sys_light.specular_color = light.specular_color
                    sys_light.smooth = light.smooth
                    sys_light.direction = light.direction
                return {'FINISHED'}
        return {'CANCELLED'}


class PREFERENCES_OT_studiolight_show(Operator):
    """Show light preferences"""
    bl_idname = "preferences.studiolight_show"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, context):
        context.preferences.active_section = 'LIGHTS'
        bpy.ops.screen.userpref_show('INVOKE_DEFAULT')
        return {'FINISHED'}


classes = (
    PREFERENCES_OT_addon_disable,
    PREFERENCES_OT_addon_enable,
    PREFERENCES_OT_addon_expand,
    PREFERENCES_OT_addon_install,
    PREFERENCES_OT_addon_refresh,
    PREFERENCES_OT_addon_remove,
    PREFERENCES_OT_addon_show,
    PREFERENCES_OT_app_template_install,
    PREFERENCES_OT_copy_prev,
    PREFERENCES_OT_keyconfig_activate,
    PREFERENCES_OT_keyconfig_export,
    PREFERENCES_OT_keyconfig_import,
    PREFERENCES_OT_keyconfig_remove,
    PREFERENCES_OT_keyconfig_test,
    PREFERENCES_OT_keyitem_add,
    PREFERENCES_OT_keyitem_remove,
    PREFERENCES_OT_keyitem_restore,
    PREFERENCES_OT_keymap_restore,
    PREFERENCES_OT_theme_install,
    PREFERENCES_OT_studiolight_install,
    PREFERENCES_OT_studiolight_new,
    PREFERENCES_OT_studiolight_uninstall,
    PREFERENCES_OT_studiolight_copy_settings,
    PREFERENCES_OT_studiolight_show,
)
