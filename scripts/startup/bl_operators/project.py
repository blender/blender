# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "register",
    "unregister",
)

import os
import logging
from dataclasses import dataclass

from enum import Enum

import bpy
from bpy.types import Operator
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    pgettext_data as data_,
)

logger = logging.getLogger(__name__)

# The schema version of the Project Config, for managing breaking schema changes.
#
# Only increment this when there are breaking schema changes.
#
# Project versioning should generally follow the same approach as blend file
# versioning. In short:
#
# - When reasonably possible, change the schema in ways that fully preserve
#   backwards compatibility and that avoid critical forwards compatibility
#   breaks.
# - When that's not possible, plan ahead and only make the needed breaking
#   changes on major version bumps of Blender (e.g. 5.x -> 6.x).
# - In the version just before a major version bump, add forwards compatibility
#   code for the anticipated breaking changes, so that e.g. the last 5.x release
#   can still open projects from Blender 6.0.
#
# For more details, see:
# https://developer.blender.org/docs/handbook/guidelines/compatibility_handling_for_blend_files/
#
# NOTE: when incrementing this version number, ensure that the versioning code
# in `read_project_toml_config()` is appropriately expanded to auto-upgrade
# between versions
PROJECT_SCHEMA_VERSION = 1

# Directory and file name where the project is read/written to disk.
PROJECT_DIR = ".blender_project"
PROJECT_CONFIG = "project.toml"

PROJECT_DEFAULT_NAME = "Untitled Project"
ASSET_LIBRARY_DEFAULT_NAME = "Untitled Asset Library"


# -------------------------------------------------------------
# TOML Schema
#
# Types that define the schema for reading/writing project config TOML files.

class VariableType(Enum):
    INTEGER = 'INTEGER'
    FLOAT = 'FLOAT'
    STRING = 'STRING'


class VariableSubtype(Enum):
    NONE = 'NONE'
    FILEPATH = 'FILEPATH'


@dataclass
class AssetLibraryDefinition:
    name: str
    path: str
    use_relative_path: bool
    import_method: str
    uuid: str | None = None


@dataclass
class ProjectVariable:
    name: str
    type: VariableType
    value: int | str | float
    subtype: VariableSubtype | None = None
    description: str = ""

    @staticmethod
    def new_from_real(project_variable):
        """Create a ProjectVariable config object from an existing real project variable."""
        subtype = None
        if project_variable.type == 'STRING':
            subtype = VariableSubtype(project_variable.subtype)

        return ProjectVariable(
            name=project_variable.name,
            type=VariableType(project_variable.type),
            value=project_variable.value,
            description=project_variable.description,
            subtype=subtype,
        )

    def add_as_real(self, variables):
        """Adds this as a real variable to the given real project variables list."""
        variable = variables.new(name=self.name, type=self.type.value)
        variable.value = self.value
        if self.subtype is not None and self.type == VariableType.STRING:
            variable.subtype = self.subtype.value
        variable.description = self.description

    def __post_init__(self):
        """Validation of invariants that cattrs doesn't check."""
        import re

        if self.name == "":
            raise ValueError("Invalid variable name '{:s}': variable names cannot be empty.")

        if re.match("^[a-zA-Z_][a-zA-Z0-9_]*$", self.name) is None:
            raise ValueError(
                "Invalid variable name '{:s}': variable names must not start with a digit, and "
                "must contain only alphanumeric characters and underscores.")

        # Check that value matches the declared variable type.
        match (self.type, self.value):
            case (VariableType.INTEGER, int()):
                pass
            case (VariableType.FLOAT, float()):
                pass
            case (VariableType.STRING, str()):
                pass
            case _:
                raise ValueError("Actual and declared type of project variable '{:s}' do not match.".format(self.name))

        # Check that value matches the declared variable type.
        match (self.type, self.subtype):
            case (VariableType.INTEGER, None):
                pass
            case (VariableType.FLOAT, None):
                pass
            case (VariableType.STRING, None) | (VariableType.STRING, VariableSubtype.NONE) \
                    | (VariableType.STRING, VariableSubtype.FILEPATH):
                pass
            case _:
                raise ValueError("Invalid subtype '{:s}' for variable type '{:s}' of variable '{:s}'.".format(
                    self.type.value,
                    self.subtype.value,
                    self.name,
                ))


@dataclass
class ProjectConfig:
    schema_version: int
    name: str
    variables: list[ProjectVariable] | None = None
    asset_libraries: list[AssetLibraryDefinition] | None = None

    @staticmethod
    def new_from_real(project):
        """Create a ProjectConfig object from an existing real project."""
        variables = None
        if len(project.variables) > 0:
            variables = [ProjectVariable.new_from_real(var) for var in project.variables]

        asset_list = None
        if len(project.asset_libraries) > 0:
            asset_list = []
            for asset_lib in project.asset_libraries:
                uuid_str = asset_lib.uuid
                if not uuid_str:
                    # No uuid string present, the uuid must be invalid
                    uuid_str = asset_lib.invalid_uuid

                asset_data = AssetLibraryDefinition(
                    asset_lib.name,
                    asset_lib.path,
                    asset_lib.use_relative_path,
                    asset_lib.import_method,
                    uuid_str)
                asset_list.append(asset_data)

        return ProjectConfig(
            schema_version=PROJECT_SCHEMA_VERSION,
            name=project.name,
            variables=variables,
            asset_libraries=asset_list,
        )

    def populate_real(self, project):
        """Fills in an existing real project's data from this ProjectConfig object."""
        if self.variables is not None:
            for config_var in self.variables:
                config_var.add_as_real(bpy.data.project.variables)

        if self.asset_libraries is None:
            return

        # Populate the project asset libraries (if any)
        for asset_lib in self.asset_libraries:
            if asset_lib.uuid:
                # Use the pre-generated UUID
                lib = project.asset_libraries.new(name=asset_lib.name, directory=asset_lib.path, uuid=asset_lib.uuid)
            else:
                # Generate a new UUID for the asset library
                lib = project.asset_libraries.new(name=asset_lib.name, directory=asset_lib.path)

            if asset_lib.import_method:
                lib.import_method = asset_lib.import_method
            if asset_lib.use_relative_path:
                lib.use_relative_path = asset_lib.use_relative_path

    def __post_init__(self):
        """Validation of invariants that cattrs doesn't check."""
        if self.name == "":
            raise ValueError("Project name cannot be empty.")

        if self.variables is not None:
            var_names = set()
            for var in self.variables:
                if var.name in var_names:
                    raise ValueError("Duplicate project variable names are not allowed.")
                var_names.add(var.name)


# -------------------------------------------------------------
# Helpers for cattrs

def structure_int_float_str(obj: int | float | str, cl: type) -> int | float | str:
    if isinstance(obj, int) or isinstance(obj, float) or isinstance(obj, str):
        return obj
    else:
        raise ValueError(f"Cannot structure {obj!r} as int | float | str.")


# -------------------------------------------------------------
# Exceptions
#
# Custom exception types for anticipated errors that should be reported to the
# user.

class ProjectSaveException(Exception):
    pass


class ProjectLoadException(Exception):
    pass


# -------------------------------------------------------------
# Internal Utilities

def save_project(project, report=None):
    """
    Save the passed project to disk.

    Throws a ProjectSaveException in any of the following cases:

    - There is no project to save.
    - The project's root path is relative or doesn't exist.
    - The project can't be written due to any of a number of file-system
      issues (directory isn't writable, etc.).

    Optionally takes an `Operator.report` for reporting errors to the user.
    """

    import cattrs
    import tomli_w
    from pathlib import Path

    if project is None:
        if report:
            report({'ERROR'}, "Cannot save project because there is no project to save.")
        raise ProjectSaveException

    logger.info("Saving project '{:s}' at '{:s}'...".format(project.name, project.root_path))

    # Get and validate the root path.
    root_path = Path(project.root_path)
    try:
        if not root_path.is_absolute():
            if report:
                report({'ERROR'}, "Cannot write project to non-absolute path.")
            raise ProjectSaveException

        if not root_path.is_dir():
            if report:
                report({'ERROR'}, "Cannot save project: root directory does not exist.")
            raise ProjectSaveException
    except PermissionError:
        if report:
            report({'ERROR'}, rpt_("Cannot access '{:s}' due to file-system permissions.").format(PROJECT_DIR))
        raise ProjectSaveException
    except Exception as e:
        if report:
            report({'ERROR'}, str(e))
        raise ProjectSaveException

    config_dir_path = root_path.joinpath(PROJECT_DIR)

    # Ensure the project config directory exists.
    try:
        config_dir_path.mkdir(parents=True, exist_ok=True)
    except FileExistsError:
        if report:
            report(
                {'ERROR'},
                rpt_("A file named '{:s}' already exists, but it needs to be a directory.").format(PROJECT_DIR),
            )
        raise ProjectSaveException
    except PermissionError:
        if report:
            report(
                {'ERROR'},
                rpt_("Cannot create '{:s}' directory due to file-system permissions.").format(PROJECT_DIR),
            )
        raise ProjectSaveException
    except Exception as e:
        if report:
            report({'ERROR'}, str(e))
        raise ProjectSaveException

    # Create a project config dict from the current project.
    converter = cattrs.Converter(omit_if_default=True)
    config = ProjectConfig.new_from_real(project)
    config_dict = converter.unstructure(config, ProjectConfig)

    # Serialize the config to TOML.
    #
    # We set the "max" line length to something short so that TOML items are
    # always serialized in the expanded format. Without that, e.g. project
    # variables might *sometimes* be written in the more compact TOML syntax.
    # Either format is fine, but flip-flopping is annoying for diffs, so we
    # try to force just one here.
    max_len_backup = tomli_w._writer.MAX_LINE_LENGTH
    try:
        tomli_w._writer.MAX_LINE_LENGTH = 10
        config_toml = tomli_w.dumps(config_dict)
    finally:
        # Restore max line length, for any other users of tomli_w.
        tomli_w._writer.MAX_LINE_LENGTH = max_len_backup

    # Write the config TOML file.
    config_path = root_path.joinpath(PROJECT_DIR, PROJECT_CONFIG)

    try:
        with config_path.open(mode='wt') as f:
            f.write(config_toml)
    except PermissionError:
        if report:
            report({'ERROR'}, rpt_("Cannot write to '{:s}' due to file-system permissions.").format(PROJECT_CONFIG))
        raise ProjectSaveException
    except Exception as e:
        if report:
            report({'ERROR'}, str(e))
        raise ProjectSaveException

    project.is_dirty = False

    logger.info("...done.")


def find_and_load_project_for_blend_path(context, blend_path, report=None):
    """
    Load the project the blend file is in, or clears the project if none is found.

    `blend_path` should be an absolute path.

    Throws a ProjectLoadException if a project is found but is invalid
    (missing config file, config validation error, etc.).

    Optionally takes an `Operator.report` for reporting errors to the user.
    """

    from pathlib import Path

    if blend_path == "":
        # Not an on-disk blend file, so there is no project to load.
        bpy.data.project_clear()
        return

    # Note: `blend_path` (and consequently the resulting `root_path`) are
    # assumed/expected to be absolute here.
    root_path = find_project_root_from_blend_file_path(Path(blend_path))
    if root_path is None:
        # No project.
        bpy.data.project_clear()
        return

    if bpy.data.project is not None and os.path.normpath(root_path) == os.path.normpath(bpy.data.project.root_path):
        # We already have this project loaded, and we don't want to obliterate
        # local unsaved changes if auto-save isn't turned on.
        return

    bpy.data.project_clear()

    # Load project.
    config = read_project_toml_config(root_path, report)
    bpy.data.project_init(config.name, str(root_path))
    config.populate_real(bpy.data.project)
    bpy.data.project.is_dirty = False


def find_project_root_from_blend_file_path(blend_path):
    """
    Search for a project root in the parent directories of the given path.

    Returns the project root if found, or None otherwise.
    """

    for parent in blend_path.parents:
        if parent.joinpath(PROJECT_DIR).is_dir():
            return parent
    return None


def read_project_toml_config(root_path, report=None):
    """
    Read the project config for the given project root path.

    Throws a ProjectLoadException if no config is found, if the config is
    not readable due to file-system permissions, or if it's not a valid
    project config (e.g. contains invalid TOML or doesn't match the schema).

    Optionally takes an `Operator.report` for reporting errors to the user.

    Returns the configuration (`ProjectConfig`).
    """
    import tomllib
    import cattrs

    config_path = root_path.joinpath(PROJECT_DIR, PROJECT_CONFIG)
    try:
        with open(config_path, "rb") as f:
            config_dict = tomllib.load(f)
    except FileNotFoundError:
        if report:
            report({'ERROR'}, rpt_("Project has no {:s} file.").format(PROJECT_CONFIG))
        raise ProjectLoadException
    except PermissionError:
        if report:
            report({'ERROR'}, rpt_("Cannot access {:s} file due to file-system permissions.").format(PROJECT_CONFIG))
        raise ProjectLoadException
    except tomllib.TOMLDecodeError as e:
        if report:
            report({'ERROR'}, rpt_("Project's {:s} file contains invalid TOML: {:s}").format(PROJECT_CONFIG, str(e)))
        raise ProjectLoadException
    except Exception as e:
        if report:
            report({'ERROR'}, str(e))
        raise ProjectLoadException

    # If there's no version schema, that means it's version 1.
    if "schema_version" not in config_dict:
        config_dict["schema_version"] = 1

    # Handle project config versioning.
    if config_dict["schema_version"] > PROJECT_SCHEMA_VERSION:
        if report:
            report({'ERROR'}, rpt_("Project configuration is newer than this version of Blender supports"))
        raise ProjectLoadException
    elif config_dict["schema_version"] < PROJECT_SCHEMA_VERSION:
        # Put versioning upgrade code here when needed.
        pass

    # Validate schema and convert to ProjectConfig class.
    converter = cattrs.Converter()
    converter.register_structure_hook(int | float | str, structure_int_float_str)
    try:
        project_config = converter.structure(config_dict, ProjectConfig)
    except (cattrs.BaseValidationError, ValueError) as e:
        if report:
            report({'ERROR'}, rpt_("Invalid project configuration file: {:s}").format(str(e)))
        raise ProjectLoadException

    return project_config


def blend_file_is_in_valid_project(blend_file_path):
    """
    Return whether the blend file is inside a valid project or not.

    True if the blend file is inside a valid project, false if no project is
    found or if the project is invalid.

    An "invalid project" is one whose TOML config is non-existent or doesn't
    validate. See `read_project_toml_config()`.
    """
    project_root = find_project_root_from_blend_file_path(blend_file_path)
    if project_root is None:
        return False

    try:
        _ = read_project_toml_config(project_root, None)
    except ProjectLoadException:
        # No valid project found.
        return False

    return True


# -------------------------------------------------------------
# Operators

class PROJECT_OT_NewProject(Operator):
    """Create a new project"""
    bl_idname = "project.new_project"
    bl_label = "New Project"

    directory: bpy.props.StringProperty(
        name="Project Root",
        subtype='DIR_PATH',
        default="",
    )

    filter_folder: bpy.props.BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        return bpy.data.project is None and bpy.data.filepath != ""

    def execute(self, context):
        from pathlib import Path

        if self.directory == "":
            self.report({'ERROR'}, "Cannot create a project with an empty directory path.")
            return {'CANCELLED'}

        if not bpy.path.is_subdir(path=bpy.data.filepath, directory=self.directory):
            self.report({'ERROR'}, "New project directory must be a parent of the currently open blend file.")
            return {'CANCELLED'}

        # Double-check that we're not already in a project directory.
        #
        # Under normal circumstances this should never happen, because a project
        # would already be loaded in that case, and thus `poll()` would fail.
        # But if someone manually calls `bpy.data.project_clear()` then this can
        # happen.
        if blend_file_is_in_valid_project(Path(bpy.data.filepath)):
            self.report(
                {'ERROR'}, (
                    "New project directory is already inside of an existing project. "
                    "Try reloading the current blend file to open the existing project."
                )
            )
            return {'CANCELLED'}

        # Get the initial project name based on the folder name.
        #
        # If the folder name contains no valid unicode (resulting in an empty
        # string after processing), we fallback to a default.
        project_name = os.path.basename(os.path.normpath(self.directory)).title() \
            .encode('utf-8', 'surrogateescape') \
            .decode('utf-8', 'ignore') \
            or data_(PROJECT_DEFAULT_NAME)

        # Create the project.
        bpy.data.project_init(project_name, self.directory)

        # Immediately save the project.
        try:
            save_project(bpy.data.project, self.report)
        except ProjectSaveException:
            # Reporting is handled by `save_project()` call in the `try` block.
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        # Set our initial path as the directory that contains the currently open
        # blend file.
        dirpath = os.path.dirname(bpy.data.filepath)
        if dirpath != "":
            self.directory = dirpath

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PROJECT_OT_SaveProject(Operator):
    """Save the current project to disk"""
    bl_idname = "project.save_project"
    bl_label = "Save Project"

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def execute(self, context):
        try:
            save_project(bpy.data.project, self.report)
        except ProjectSaveException:
            # Reporting is handled by `save_project()` call in the `try` block.
            return {'CANCELLED'}

        return {'FINISHED'}


class PROJECT_OT_OpenBlendInProject(Operator):
    """Opens a blend file, but only if it's inside of a project."""
    bl_idname = "project.open_blend_in_project"
    bl_label = "Open File..."

    filepath: bpy.props.StringProperty(
        name="Blend file path",
        subtype='FILE_PATH',
        default="",
    )

    filter_folder: bpy.props.BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )

    filter_blender: bpy.props.BoolProperty(
        name="Filter blend files",
        default=True,
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        from pathlib import Path

        if not blend_file_is_in_valid_project(Path(self.filepath)):
            self.report(
                {'ERROR'},
                "Selected blend file is not part of a project.")
            return {'CANCELLED'}

        bpy.ops.wm.open_mainfile(filepath=self.filepath)

        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PROJECT_OT_AddVariable(Operator):
    """Add a new variable to the current project"""
    bl_idname = "project.add_variable"
    bl_label = "Add Variable"

    variable_type: bpy.props.EnumProperty(
        default='STRING',
        items=[
            ('STRING', "String Variable", ""),
            ('FILEPATH', "Filepath Variable", ""),
            ('INTEGER', "Integer Variable", ""),
            ('FLOAT', "Float Variable", ""),
        ],
    )

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def execute(self, context):
        match self.variable_type:
            case 'STRING':
                bpy.data.project.variables.new(name="string_variable", type='STRING')
            case 'FILEPATH':
                var = bpy.data.project.variables.new(name="filepath_variable", type='STRING')
                var.subtype = 'FILEPATH'
            case 'INTEGER':
                bpy.data.project.variables.new(name="integer_variable", type='INTEGER')
            case 'FLOAT':
                bpy.data.project.variables.new(name="float_variable", type='FLOAT')
            case _:
                assert (False)

        return {'FINISHED'}


class PROJECT_OT_RemoveVariable(Operator):
    """Remove the active variable from the current project"""
    bl_idname = "project.remove_variable"
    bl_label = "Remove Variable"

    @classmethod
    def poll(cls, context):
        project = bpy.data.project
        if project is None:
            return False
        return project.active_variable_index >= 0 and project.active_variable_index < len(project.variables)

    def execute(self, context):
        project = bpy.data.project
        var = project.variables[project.active_variable_index]
        project.variables.remove(var)

        return {'FINISHED'}


class PROJECT_OT_MoveVariable(Operator):
    """Move the active variable up or down in the list of variables"""
    bl_idname = "project.move_variable"
    bl_label = "Move Variable"

    direction: bpy.props.EnumProperty(items=[
        ('UP', "Move Up", ""),
        ('DOWN', "Move Down", ""),
    ])

    @classmethod
    def poll(cls, context):
        project = bpy.data.project
        if project is None:
            return False
        return project.active_variable_index >= 0 and project.active_variable_index < len(project.variables)

    def execute(self, context):
        project = bpy.data.project

        index = project.active_variable_index
        if self.direction == 'UP' and index > 0:
            project.variables.move(index, index - 1)
            project.active_variable_index -= 1
        elif self.direction == 'DOWN' and (index + 1) < len(project.variables):
            project.variables.move(index, index + 1)
            project.active_variable_index += 1
        return {'FINISHED'}


class PROJECT_OT_AssetLibraryAdd(Operator):
    """
    Register a directory to be used by the Asset Browser and other places showing assets,
    as source of assets within the current project.
    """
    bl_idname = "project.asset_library_add"
    bl_label = "Add Asset Library"
    bl_options = {'INTERNAL'}

    directory: bpy.props.StringProperty(
        name="Asset Library Directory",
        subtype='DIR_PATH',
        default="",
    )

    filter_folder: bpy.props.BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def execute(self, context):
        if self.directory == "":
            self.report({'ERROR'}, "Cannot create an assset library with an empty directory path.")
            return {'CANCELLED'}

        # Create an initial name from the folder name
        #
        # If the folder name contains no valid unicode (resulting in an empty
        # string after processing), we fallback to a default.
        asset_library_path = os.path.normpath(self.directory)
        asset_library_name = os.path.basename(asset_library_path).title() \
            .encode('utf-8', 'surrogateescape') \
            .decode('utf-8', 'ignore') \
            or data_(ASSET_LIBRARY_DEFAULT_NAME)

        # Replace base path with {project_root} if it is within the project folder.
        root_path = bpy.data.project.root_path
        if asset_library_path.startswith(root_path):
            asset_library_path = "{project_root}" + asset_library_path[len(root_path):]
        bpy.data.project.asset_libraries.new(name=asset_library_name, directory=asset_library_path)

        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


class PROJECT_OT_AssetLibraryRemove(Operator):
    """
    Deregister an asset library so that its directory will no longer show up in
    Asset Browsers and other places showing assets.
    """
    bl_idname = "project.asset_library_remove"
    bl_label = "Remove Project Asset Library"
    bl_options = {'INTERNAL'}

    index: bpy.props.IntProperty(
        name="Index",
        default=0,
    )

    @classmethod
    def poll(cls, context):
        if bpy.data.project is None:
            return False
        return len(bpy.data.project.asset_libraries) != 0

    def execute(self, context):
        if self.index >= len(bpy.data.project.asset_libraries):
            return {'CANCELLED'}

        library = bpy.data.project.asset_libraries[self.index]
        bpy.data.project.asset_libraries.remove(library)

        return {'FINISHED'}


# -------------------------------------------------------------
# Handler Callbacks
#
# Auto-loading / clearing of projects when loading/saving blend files or
# exiting.

def log_project_save_error():
    logger.error(
        "Error trying to save project '{:s}' at '{:s}'.".format(
            bpy.data.project.name,
            bpy.data.project.root_path))


def log_project_load_error(blend_path):
    logger.error("Error trying to load project for blend file '{:s}'.".format(blend_path))


@bpy.app.handlers.persistent
def on_blend_load(blend_path):
    # Auto-save the current project before loading a different blend file.
    if bpy.context.preferences.use_project_auto_save and bpy.data.project is not None and bpy.data.project.is_dirty:
        try:
            save_project(bpy.data.project)
        except ProjectSaveException:
            log_project_save_error()

    # Load the project (or clear if none) for the blend file we're about to
    # load.
    try:
        find_and_load_project_for_blend_path(bpy.context, blend_path)
    except ProjectLoadException:
        log_project_load_error(blend_path)


@bpy.app.handlers.persistent
def on_blend_save(blend_path):
    # Auto-save project when saving the current blend file.
    if bpy.context.preferences.use_project_auto_save and bpy.data.project is not None and bpy.data.project.is_dirty:
        try:
            save_project(bpy.data.project)
        except ProjectSaveException:
            log_project_save_error()

    # In case we're saving the blend to disk for the first time or to a new
    # location, load the project there (if any).
    #
    # The equality check here is to prevent loading projects from the copy
    # location when doing "Save Copy...".
    if blend_path == bpy.data.filepath:
        try:
            find_and_load_project_for_blend_path(bpy.context, blend_path)
        except ProjectLoadException:
            log_project_load_error(blend_path)


@bpy.app.handlers.persistent
def on_exit(is_user_exit):
    if not is_user_exit:
        return

    if bpy.context.preferences.use_project_auto_save and bpy.data.project is not None and bpy.data.project.is_dirty:
        try:
            save_project(bpy.data.project)
        except ProjectSaveException:
            log_project_save_error()


# -----------------------------------------------------------------------------
# Register

classes = (
    PROJECT_OT_NewProject,
    PROJECT_OT_SaveProject,
    PROJECT_OT_OpenBlendInProject,
    PROJECT_OT_AddVariable,
    PROJECT_OT_RemoveVariable,
    PROJECT_OT_MoveVariable,
    PROJECT_OT_AssetLibraryAdd,
    PROJECT_OT_AssetLibraryRemove,
)


def register():
    bpy.app.handlers.load_pre.append(on_blend_load)
    bpy.app.handlers.save_post.append(on_blend_save)
    bpy.app.handlers.exit_pre.append(on_exit)


def unregister():
    bpy.app.handlers.load_pre.remove(on_blend_load)
    bpy.app.handlers.save_post.remove(on_blend_save)
    bpy.app.handlers.exit_pre.remove(on_exit)
