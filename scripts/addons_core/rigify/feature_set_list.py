# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from typing import TYPE_CHECKING, List, Sequence, Optional

import bpy
from bpy.props import StringProperty
import os
import re
import importlib
import traceback
from zipfile import ZipFile
from shutil import rmtree

from . import feature_sets

if TYPE_CHECKING:
    from . import RigifyFeatureSets


DEFAULT_NAME = 'rigify'

# noinspection PyProtectedMember
INSTALL_PATH = feature_sets._install_path()
NAME_PREFIX = feature_sets.__name__.split('.')

# noinspection SpellCheckingInspection
PROMOTED_FEATURE_SETS = [
    {
        "name": "CloudRig",
        "author": "Demeter Dzadik",
        "description": "Feature set developed by the Blender Animation Studio",
        "doc_url": "https://gitlab.com/blender/CloudRig/-/wikis/",
        "link": "https://gitlab.com/blender/CloudRig/",
    },
    {
        "name": "Experimental Rigs by Alexander Gavrilov",
        "author": "Alexander Gavrilov",
        "description":
            "Experimental and/or niche rigs made by a Rigify maintainer.\n"
            "Includes a BlenRig-like spine, Body IK (knee & elbow IK), jiggles, skin transforms, etc.",
        "link": "https://github.com/angavrilov/angavrilov-rigs",
    },
    {
        "name": "Cessen's Rigify Extensions",
        "author": "Nathan Vegdahl",
        "description": "Collection of original legacy Rigify rigs minimally ported to the modern Rigify",
        "warning": "This feature set is maintained at the bare minimal level",
        "link": "https://github.com/cessen/cessen_rigify_ext",
    }
]


def get_install_path(*, create=False):
    if not os.path.exists(INSTALL_PATH):
        if create:
            os.makedirs(INSTALL_PATH, exist_ok=True)
        else:
            return None

    return INSTALL_PATH


def get_installed_modules_names() -> List[str]:
    """Return a list of module names of all feature sets in the file system."""
    features_path = get_install_path()
    if not features_path:
        return []

    sets = []

    for fs in os.listdir(features_path):
        if fs and fs[0] != '.' and fs != DEFAULT_NAME:
            fs_path = os.path.join(features_path, fs)
            if os.path.isdir(fs_path):
                sets.append(fs)

    return sets


def get_prefs_feature_sets() -> Sequence['RigifyFeatureSets']:
    from . import RigifyPreferences
    return RigifyPreferences.get_instance().rigify_feature_sets


def get_enabled_modules_names() -> List[str]:
    """Return a list of module names of all enabled feature sets."""
    installed_module_names = get_installed_modules_names()
    rigify_feature_sets = get_prefs_feature_sets()

    enabled_module_names = {fs.module_name for fs in rigify_feature_sets if fs.enabled}

    return [name for name in installed_module_names if name in enabled_module_names]


def find_module_name_by_link(link: str) -> str | None:
    """Returns the name of the feature set module that is associated with the specified url."""
    if not link:
        return None

    for fs in get_prefs_feature_sets():
        if fs.link == link:
            return fs.module_name or None

    return None


def mark_feature_set_exception(module_name: str):
    if module_name:
        for fs in get_prefs_feature_sets():
            if fs.module_name == module_name:
                fs.has_exceptions = True


def get_module(feature_set: str):
    return importlib.import_module('.'.join([*NAME_PREFIX, feature_set]))


def get_module_safe(feature_set: str):
    # noinspection PyBroadException
    try:
        return get_module(feature_set)
    except:  # noqa: E722
        return None


def get_module_by_link_safe(link: str):
    if module_name := find_module_name_by_link(link):
        return get_module_safe(module_name)


def get_dir_path(feature_set: str, *extra_items: list[str]):
    base_dir = os.path.join(INSTALL_PATH, feature_set, *extra_items)
    base_path = [*NAME_PREFIX, feature_set, *extra_items]
    return base_dir, base_path


def get_info_dict(feature_set: str):
    module = get_module_safe(feature_set)

    if module and hasattr(module, 'rigify_info'):
        data = module.rigify_info
        if isinstance(data, dict):
            return data

    return {}


def call_function_safe(module_name: str, func_name: str,
                       args: Optional[list] = None, kwargs: Optional[dict] = None,
                       mark_error=False):
    module = get_module_safe(module_name)

    if module:
        func = getattr(module, func_name, None)

        if callable(func):
            # noinspection PyBroadException
            try:
                return func(*(args or []), **(kwargs or {}))
            except Exception:
                print(f"Rigify Error: Could not call function '{func_name}' of feature set "
                      f"'{module_name}': exception occurred.\n")
                traceback.print_exc()
                print("")

                if mark_error:
                    mark_feature_set_exception(module_name)

    return None


def call_register_function(feature_set: str, do_register: bool):
    call_function_safe(feature_set, 'register' if do_register else 'unregister', mark_error=do_register)


def get_ui_name(feature_set: str):
    # Try to get user-defined name
    info = get_info_dict(feature_set)
    if 'name' in info:
        return info['name']

    # Default name based on directory
    name = re.sub(r'[_.-]', ' ', feature_set)
    name = re.sub(r'(?<=\d) (?=\d)', '.', name)
    return name.title()


def feature_set_items(_scene, _context):
    """Get items for the Feature Set EnumProperty"""
    items = [
        ('all', 'All', 'All installed feature sets and rigs bundled with Rigify'),
        ('rigify', 'Rigify Built-in', 'Rigs bundled with Rigify'),
    ]

    for fs in get_enabled_modules_names():
        ui_name = get_ui_name(fs)
        items.append((fs, ui_name, ui_name))

    return items


def verify_feature_set_archive(zipfile):
    """Verify that the zip file contains one root directory, and some required files."""
    dirname = None
    init_found = False
    data_found = False

    for name in zipfile.namelist():
        parts = re.split(r'[/\\]', name)

        if dirname is None:
            dirname = parts[0]
        elif dirname != parts[0]:
            dirname = None
            break

        if len(parts) == 2 and parts[1] == '__init__.py':
            init_found = True

        if len(parts) > 2 and parts[1] in {'rigs', 'metarigs'} and parts[-1] == '__init__.py':
            data_found = True

    return dirname, init_found, data_found


# noinspection PyPep8Naming
class DATA_OT_rigify_add_feature_set(bpy.types.Operator):
    bl_idname = "wm.rigify_add_feature_set"
    bl_label = "Add External Feature Set"
    bl_description = "Add external feature set (rigs, metarigs, ui templates)"
    bl_options = {"REGISTER", "UNDO", "INTERNAL"}

    filter_glob: StringProperty(default="*.zip", options={'HIDDEN'})
    filepath: StringProperty(maxlen=1024, subtype='FILE_PATH', options={'HIDDEN', 'SKIP_SAVE'})

    @classmethod
    def poll(cls, context):
        return True

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        from . import RigifyPreferences
        addon_prefs = RigifyPreferences.get_instance()

        rigify_config_path = get_install_path(create=True)

        with ZipFile(bpy.path.abspath(self.filepath), 'r') as zip_archive:
            base_dirname, init_found, data_found = verify_feature_set_archive(zip_archive)

            if not base_dirname:
                self.report({'ERROR'}, "The feature set archive must contain one base directory.")
                return {'CANCELLED'}

            # Patch up some invalid characters to allow using 'Download ZIP' on GitHub.
            fixed_dirname = re.sub(r'[.-]', '_', base_dirname)

            if not re.fullmatch(r'[a-zA-Z][a-zA-Z_0-9]*', fixed_dirname):
                self.report({'ERROR'},
                            f"The feature set archive base directory name is not a valid "
                            f"identifier: '{base_dirname}'.")
                return {'CANCELLED'}

            if fixed_dirname == DEFAULT_NAME:
                self.report(
                    {'ERROR'}, f"The '{DEFAULT_NAME}' name is not allowed for feature sets.")
                return {'CANCELLED'}

            if not init_found or not data_found:
                self.report(
                    {'ERROR'},
                    "The feature set archive has no rigs or metarigs, or is missing __init__.py.")
                return {'CANCELLED'}

            base_dir = os.path.join(rigify_config_path, base_dirname)
            fixed_dir = os.path.join(rigify_config_path, fixed_dirname)

            for path, name in [(base_dir, base_dirname), (fixed_dir, fixed_dirname)]:
                if os.path.exists(path):
                    self.report({'ERROR'}, f"Feature set directory already exists: '{name}'.")
                    return {'CANCELLED'}

            # Unpack the validated archive and fix the directory name if necessary
            zip_archive.extractall(rigify_config_path)

            if base_dir != fixed_dir:
                os.rename(base_dir, fixed_dir)

            # Call the register callback of the new set
            addon_prefs.refresh_installed_feature_sets()

            call_register_function(fixed_dirname, True)

            addon_prefs.update_external_rigs()

            # Select the new entry
            for i, fs in enumerate(addon_prefs.rigify_feature_sets):
                if fs.module_name == fixed_dirname:
                    addon_prefs.active_feature_set_index = i
                    break

        return {'FINISHED'}


# noinspection PyPep8Naming
class DATA_OT_rigify_remove_feature_set(bpy.types.Operator):
    bl_idname = "wm.rigify_remove_feature_set"
    bl_label = "Remove External Feature Set"
    bl_description = "Remove external feature set (rigs, metarigs, ui templates)"
    bl_options = {"REGISTER", "UNDO", "INTERNAL"}

    @classmethod
    def poll(cls, context):
        return True

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        from . import RigifyPreferences
        addon_prefs = RigifyPreferences.get_instance()
        feature_set_list = addon_prefs.rigify_feature_sets
        active_idx = addon_prefs.active_feature_set_index
        active_fs: 'RigifyFeatureSets' = feature_set_list[active_idx]

        # Call the 'unregister' callback of the set being removed.
        if active_fs.enabled:
            call_register_function(active_fs.module_name, do_register=False)

        # Remove the feature set's folder from the file system.
        rigify_config_path = get_install_path()
        if rigify_config_path:
            set_path = os.path.join(rigify_config_path, active_fs.module_name)
            if os.path.exists(set_path):
                rmtree(set_path)

        # Remove the feature set's entry from the addon preferences.
        feature_set_list.remove(active_idx)

        # Remove the feature set's entries from the metarigs and rig types.
        addon_prefs.refresh_installed_feature_sets()
        addon_prefs.update_external_rigs()

        # Update active index.
        addon_prefs.active_feature_set_index -= 1

        return {'FINISHED'}


def register():
    bpy.utils.register_class(DATA_OT_rigify_add_feature_set)
    bpy.utils.register_class(DATA_OT_rigify_remove_feature_set)


def unregister():
    bpy.utils.unregister_class(DATA_OT_rigify_add_feature_set)
    bpy.utils.unregister_class(DATA_OT_rigify_remove_feature_set)
