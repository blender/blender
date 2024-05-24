# SPDX-FileCopyrightText: 2019-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Operator,
    OperatorFileListElement,
)
from bpy.props import (
    BoolProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
    CollectionProperty,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_tip as rpt_,
)


def _zipfile_root_namelist(file_to_extract):
    # Return a list of root paths from zipfile.ZipFile.namelist.
    import os
    root_paths = []
    for f in file_to_extract.namelist():
        # Python's `zipfile` API always adds a separate at the end of directories.
        # use `os.path.normpath` instead of `f.removesuffix(os.sep)`
        # since paths could be stored as `./paths/./`.
        #
        # Note that `..` prefixed paths can exist in ZIP files but they don't write to parent directory when extracting.
        # Nor do they pass the `os.sep not in f` test, this is important,
        # otherwise `shutil.rmtree` below could made to remove directories outside the installation directory.
        f = os.path.normpath(f)
        if os.sep not in f:
            root_paths.append(f)
    return root_paths


def _module_filesystem_remove(path_base, module_name):
    # Remove all Python modules with `module_name` in `base_path`.
    # The `module_name` is expected to be a result from `_zipfile_root_namelist`.
    import os
    import shutil
    module_name = os.path.splitext(module_name)[0]
    for f in os.listdir(path_base):
        f_base = os.path.splitext(f)[0]
        if f_base == module_name:
            f_full = os.path.join(path_base, f)
            if os.path.isdir(f_full):
                shutil.rmtree(f_full)
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

    @classmethod
    def _old_version_path(cls, version):
        return bpy.utils.resource_path('USER', major=version[0], minor=version[1])

    @classmethod
    def previous_version(cls):
        import os
        # Find config folder from previous version.
        #
        # Always allow to load startup data from any release from current major release cycle, and the previous one.

        # NOTE: This value may need to be updated when the release cycle system is modified.
        # Here could be `6` in theory (Blender 3.6 LTS), just give it a bit of extra room, such that it does not have to
        # be updated if there ever exist a 3.7 release e.g.
        MAX_MINOR_VERSION_FOR_PREVIOUS_MAJOR_LOOKUP = 10

        version_new = bpy.app.version[:2]
        version_old = [version_new[0], version_new[1] - 1]

        while True:
            while version_old[1] >= 0:
                if os.path.isdir(cls._old_version_path(version_old)):
                    return tuple(version_old)
                version_old[1] -= 1
            if version_new[0] == version_old[0]:
                # Retry with older major version.
                version_old[0] -= 1
                version_old[1] = MAX_MINOR_VERSION_FOR_PREVIOUS_MAJOR_LOOKUP
            else:
                break

        return None

    @classmethod
    def _old_path(cls):
        version_old = cls.previous_version()
        return cls._old_version_path(version_old) if version_old else None

    @classmethod
    def _new_path(cls):
        return bpy.utils.resource_path('USER')

    @classmethod
    def poll(cls, _context):
        import os

        old = cls._old_path()
        new = cls._new_path()
        if not old:
            return False

        # Disable operator in case config path is overridden with environment
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
        shutil.copytree(self._old_path(), self._new_path(), dirs_exist_ok=True, symlinks=True)

        # Reload preferences and `recent-files.txt`.
        bpy.ops.wm.read_userpref()
        bpy.ops.wm.read_history()
        # Fix operator presets that have unwanted filepath properties
        bpy.ops.wm.operator_presets_cleanup()

        # don't loose users work if they open the splash later.
        if bpy.data.is_saved is bpy.data.is_dirty is False:
            bpy.ops.wm.read_homefile()
        else:
            self.report({'INFO'}, "Reload Start-Up file to restore settings")

        return {'FINISHED'}


class PREFERENCES_OT_keyconfig_test(Operator):
    """Test key configuration for conflicts"""
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
    """Import key configuration from a Python script"""
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
        name="Filter Python",
        default=True,
        options={'HIDDEN'},
    )
    keep_original: BoolProperty(
        name="Keep Original",
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

        path = bpy.utils.user_resource(
            'SCRIPTS',
            path=os.path.join("presets", "keyconfig"),
            create=True,
        )
        path = os.path.join(path, config_name)

        try:
            if self.keep_original:
                shutil.copy(self.filepath, path)
            else:
                shutil.move(self.filepath, path)
        except BaseException as ex:
            self.report({'ERROR'}, rpt_("Installing keymap failed: {:s}").format(str(ex)))
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
    """Export key configuration to a Python script"""
    bl_idname = "preferences.keyconfig_export"
    bl_label = "Export Key Configuration..."

    all: BoolProperty(
        name="All Keymaps",
        default=False,
        description="Write all keymaps (not just user modified)",
    )
    filepath: StringProperty(
        subtype='FILE_PATH',
        default="",
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
        name="Filter Python",
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
        import os
        wm = context.window_manager
        if not self.filepath:
            self.filepath = os.path.join(
                os.path.expanduser("~"),
                bpy.path.display_name_to_filepath(wm.keyconfigs.active.name) + ".py",
            )
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
        description="Identifier of the item to restore",
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
    """Turn on this extension"""
    bl_idname = "preferences.addon_enable"
    bl_label = "Enable Extension"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to enable",
    )

    def execute(self, _context):
        import addon_utils

        err_str = ""

        def err_cb(ex):
            import traceback
            traceback.print_exc()

            # The full trace-back in the UI is unwieldy and associated with unhandled exceptions.
            # Only show a single exception instead of the full trace-back,
            # developers can debug using information printed in the console.
            nonlocal err_str
            err_str = str(ex)

        mod = addon_utils.enable(self.module, default_set=True, handle_error=err_cb)

        if mod:
            bl_info = addon_utils.module_bl_info(mod)

            info_ver = bl_info.get("blender", (0, 0, 0))

            if info_ver > bpy.app.version:
                self.report(
                    {'WARNING'},
                    rpt_(
                        "This script was written Blender "
                        "version {:d}.{:d}.{:d} and might not "
                        "function (correctly), "
                        "though it is enabled"
                    ).format(info_ver)
                )
            return {'FINISHED'}
        else:

            if err_str:
                self.report({'ERROR'}, err_str)

            return {'CANCELLED'}


class PREFERENCES_OT_addon_disable(Operator):
    """Turn off this extension"""
    bl_idname = "preferences.addon_disable"
    bl_label = "Disable Extension"

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

        path_themes = bpy.utils.user_resource(
            'SCRIPTS',
            path=os.path.join("presets", "interface_theme"),
            create=True,
        )

        if not path_themes:
            self.report({'ERROR'}, "Failed to get themes path")
            return {'CANCELLED'}

        path_dest = os.path.join(path_themes, os.path.basename(xmlfile))

        if not self.overwrite:
            if os.path.exists(path_dest):
                self.report({'WARNING'}, rpt_("File already installed to {!r}").format(path_dest))
                return {'CANCELLED'}

        try:
            shutil.copyfile(xmlfile, path_dest)
            bpy.ops.script.execute_preset(
                filepath=path_dest,
                menu_idname="USERPREF_MT_interface_theme_presets",
            )
        except BaseException:
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
    bl_label = "Install Add-on"

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing add-ons with the same ID",
        default=True,
    )

    def _target_path_items(_self, context):
        default_item = ('DEFAULT', "Default", "")
        if context is None:
            return (
                default_item,
            )

        paths = context.preferences.filepaths
        return (
            default_item,
            None,
            *[(item.name, item.name, "") for index, item in enumerate(paths.script_directories) if item.directory],
        )

    target: EnumProperty(
        name="Target Path",
        items=_target_path_items,
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
        name="Filter Python",
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
            # Don't use `bpy.utils.script_paths(path="addons")` because we may not be able to write to it.
            path_addons = bpy.utils.user_resource('SCRIPTS', path="addons", create=True)
        else:
            paths = context.preferences.filepaths
            for script_directory in paths.script_directories:
                if script_directory.name == self.target:
                    path_addons = os.path.join(script_directory.directory, "addons")
                    break

        if not path_addons:
            self.report({'ERROR'}, "Failed to get add-ons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_addons):
            try:
                os.makedirs(path_addons, exist_ok=True)
            except BaseException:
                traceback.print_exc()

        # Check if we are installing from a target path,
        # doing so causes 2+ addons of same name or when the same from/to
        # location is used, removal of the file!
        addon_path = ""
        pyfile_dir = os.path.dirname(pyfile)
        for addon_path in addon_utils.paths():
            if os.path.samefile(pyfile_dir, addon_path):
                self.report({'ERROR'}, rpt_("Source file is in the add-on search path: {!r}").format(addon_path))
                return {'CANCELLED'}
        del addon_path
        del pyfile_dir
        # done checking for exceptional case

        addons_old = {mod.__name__ for mod in addon_utils.modules()}

        # check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(pyfile):
            try:
                file_to_extract = zipfile.ZipFile(pyfile, "r")
            except BaseException:
                traceback.print_exc()
                return {'CANCELLED'}

            file_to_extract_root = _zipfile_root_namelist(file_to_extract)

            if "__init__.py" in file_to_extract_root:
                self.report({'ERROR'}, rpt_(
                    "ZIP packaged incorrectly; __init__.py should be in a directory, not at top-level"
                ))
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract_root:
                    _module_filesystem_remove(path_addons, f)
            else:
                for f in file_to_extract_root:
                    path_dest = os.path.join(path_addons, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, rpt_("File already installed to {!r}").format(path_dest))
                        return {'CANCELLED'}

            try:  # extract the file to "addons"
                file_to_extract.extractall(path_addons)
            except BaseException:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            path_dest = os.path.join(path_addons, os.path.basename(pyfile))

            if self.overwrite:
                _module_filesystem_remove(path_addons, os.path.basename(pyfile))
            elif os.path.exists(path_dest):
                self.report({'WARNING'}, rpt_("File already installed to {!r}").format(path_dest))
                return {'CANCELLED'}

            # if not compressed file just copy into the addon path
            try:
                shutil.copyfile(pyfile, path_dest)
            except BaseException:
                traceback.print_exc()
                return {'CANCELLED'}

        addons_new = {mod.__name__ for mod in addon_utils.modules()} - addons_old
        addons_new.discard("modules")

        # disable any addons we may have enabled previously and removed.
        # this is unlikely but do just in case. bug #23978.
        for new_addon in addons_new:
            addon_utils.disable(new_addon, default_set=True)

        # possible the zip contains multiple addons, we could disallow this
        # but for now just use the first
        for mod in addon_utils.modules(refresh=False):
            if mod.__name__ in addons_new:
                bl_info = addon_utils.module_bl_info(mod)

                # show the newly installed addon.
                context.preferences.view.show_addons_enabled_only = False
                context.window_manager.addon_filter = 'All'
                context.window_manager.addon_search = bl_info["name"]
                break

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = rpt_("Modules Installed ({:s}) from {!r} into {!r}").format(
            ", ".join(sorted(addons_new)), pyfile, path_addons,
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
            self.report({'WARNING'}, rpt_("Add-on path {!r} could not be found").format(path))
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
        self.layout.label(text=iface_("Remove Add-on: {!r}?").format(self.module), translate=False)
        path, _isdir = PREFERENCES_OT_addon_remove.path_from_addon(self.module)
        self.layout.label(text=iface_("Path: {!r}").format(path), translate=False)

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

        addon_module_name = self.module

        mod = addon_utils.addons_fake_modules.get(addon_module_name)
        if mod is not None:
            bl_info = addon_utils.module_bl_info(mod)
            bl_info["show_expanded"] = not bl_info["show_expanded"]

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

        addon_module_name = self.module

        _modules = addon_utils.modules(refresh=False)
        mod = addon_utils.addons_fake_modules.get(addon_module_name)
        if mod is not None:
            bl_info = addon_utils.module_bl_info(mod)
            bl_info["show_expanded"] = True

            context.preferences.active_section = 'EXTENSIONS'
            context.preferences.view.show_addons_enabled_only = False
            context.window_manager.addon_filter = 'All'
            context.window_manager.addon_search = bl_info["name"]
            bpy.ops.screen.userpref_show('INVOKE_DEFAULT')

        return {'FINISHED'}


# Note: shares some logic with PREFERENCES_OT_addon_install
# but not enough to de-duplicate. Fixes here may apply to both.
class PREFERENCES_OT_app_template_install(Operator):
    """Install an application template"""
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
            'SCRIPTS',
            path=os.path.join("startup", "bl_app_templates_user"),
            create=True,
        )

        if not path_app_templates:
            self.report({'ERROR'}, "Failed to get add-ons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_app_templates):
            try:
                os.makedirs(path_app_templates, exist_ok=True)
            except BaseException:
                traceback.print_exc()

        app_templates_old = set(os.listdir(path_app_templates))

        # check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(filepath):
            try:
                file_to_extract = zipfile.ZipFile(filepath, "r")
            except BaseException:
                traceback.print_exc()
                return {'CANCELLED'}

            file_to_extract_root = _zipfile_root_namelist(file_to_extract)
            if self.overwrite:
                for f in file_to_extract_root:
                    _module_filesystem_remove(path_app_templates, f)
            else:
                for f in file_to_extract_root:
                    path_dest = os.path.join(path_app_templates, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, rpt_("File already installed to {!r}").format(path_dest))
                        return {'CANCELLED'}

            try:  # extract the file to "bl_app_templates_user"
                file_to_extract.extractall(path_app_templates)
            except BaseException:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            # Only support installing zip-files.
            self.report({'WARNING'}, rpt_("Expected a zip-file {!r}").format(filepath))
            return {'CANCELLED'}

        app_templates_new = set(os.listdir(path_app_templates)) - app_templates_old

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = rpt_("Template Installed ({:s}) from {!r} into {!r}").format(
            ", ".join(sorted(app_templates_new)),
            filepath,
            path_app_templates,
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
    """Install a user defined light"""
    bl_idname = "preferences.studiolight_install"
    bl_label = "Install Light"

    files: CollectionProperty(
        name="File Path",
        type=OperatorFileListElement,
    )
    directory: StringProperty(
        subtype='DIR_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter Folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.png;*.jpg;*.hdr;*.exr",
        options={'HIDDEN'},
    )
    type: EnumProperty(
        name="Type",
        items=(
            ('MATCAP', "MatCap", "Install custom MatCaps"),
            ('WORLD', "World", "Install custom HDRIs"),
            ('STUDIO', "Studio", "Install custom Studio Lights"),
        ),
    )

    def execute(self, context):
        import os
        import shutil
        prefs = context.preferences

        path_studiolights = os.path.join("studiolights", self.type.lower())
        path_studiolights = bpy.utils.user_resource('DATAFILES', path=path_studiolights, create=True)
        if not path_studiolights:
            self.report({'ERROR'}, "Failed to create Studio Light path")
            return {'CANCELLED'}

        for e in self.files:
            shutil.copy(os.path.join(self.directory, e.name), path_studiolights)
            prefs.studio_lights.load(os.path.join(path_studiolights, e.name), self.type)

        # print message
        msg = rpt_("StudioLight Installed {!r} into {!r}").format(
            ", ".join(e.name for e in self.files),
            path_studiolights
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

    ask_override = False

    def execute(self, context):
        import os
        prefs = context.preferences
        wm = context.window_manager
        filename = bpy.path.ensure_ext(self.filename, ".sl")

        path_studiolights = bpy.utils.user_resource(
            'DATAFILES',
            path=os.path.join("studiolights", "studio"),
            create=True,
        )
        if not path_studiolights:
            self.report({'ERROR'}, "Failed to get Studio Light path")
            return {'CANCELLED'}

        filepath_final = os.path.join(path_studiolights, filename)
        if os.path.isfile(filepath_final):
            if not self.ask_override:
                self.ask_override = True
                return wm.invoke_props_dialog(self, width=320)
            else:
                for studio_light in prefs.studio_lights:
                    if studio_light.name == filename:
                        bpy.ops.preferences.studiolight_uninstall(index=studio_light.index)

        prefs.studio_lights.new(path=filepath_final)

        # print message
        msg = rpt_("StudioLight Installed {!r} into {!r}").format(self.filename, str(path_studiolights))
        print(msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}

    def draw(self, _context):
        layout = self.layout
        if self.ask_override:
            layout.label(text="Warning, file already exists. Overwrite existing file?")
        else:
            layout.prop(self, "filename")

    def invoke(self, context, _event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=320)


class PREFERENCES_OT_studiolight_uninstall(Operator):
    """Delete Studio Light"""
    bl_idname = "preferences.studiolight_uninstall"
    bl_label = "Uninstall Studio Light"
    index: IntProperty()

    def execute(self, context):
        import os
        prefs = context.preferences
        for studio_light in prefs.studio_lights:
            if studio_light.index == self.index:
                filepath = studio_light.path
                if filepath and os.path.exists(filepath):
                    os.unlink(filepath)
                prefs.studio_lights.remove(studio_light)
                return {'FINISHED'}
        return {'CANCELLED'}


class PREFERENCES_OT_studiolight_copy_settings(Operator):
    """Copy Studio Light settings to the Studio Light editor"""
    bl_idname = "preferences.studiolight_copy_settings"
    bl_label = "Copy Studio Light Settings"
    index: IntProperty()

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


class PREFERENCES_OT_script_directory_new(Operator):
    bl_idname = "preferences.script_directory_add"
    bl_label = "Add Python Script Directory"

    directory: StringProperty(
        subtype='DIR_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter Folders",
        default=True,
        options={'HIDDEN'},
    )

    def execute(self, context):
        import os

        script_directories = context.preferences.filepaths.script_directories

        new_dir = script_directories.new()
        # Assign path selected via file browser.
        new_dir.directory = self.directory
        new_dir.name = os.path.basename(self.directory.rstrip(os.sep))

        assert context.preferences.is_dirty is True

        return {'FINISHED'}

    def invoke(self, context, _event):
        wm = context.window_manager

        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PREFERENCES_OT_script_directory_remove(Operator):
    bl_idname = "preferences.script_directory_remove"
    bl_label = "Remove Python Script Directory"

    index: IntProperty(
        name="Index",
        description="Index of the script directory to remove",
    )

    def execute(self, context):
        script_directories = context.preferences.filepaths.script_directories
        for search_index, script_directory in enumerate(script_directories):
            if search_index == self.index:
                script_directories.remove(script_directory)
                break

        assert context.preferences.is_dirty is True

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
    PREFERENCES_OT_script_directory_new,
    PREFERENCES_OT_script_directory_remove,
)
