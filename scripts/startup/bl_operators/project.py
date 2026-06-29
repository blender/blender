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

import bpy
from bpy.types import Operator
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    pgettext_data as data_,
)

logger = logging.getLogger(__name__)

# Directory and file name where the project is read/written to disk.
PROJECT_DIR = ".blender_project"
PROJECT_CONFIG = "project.toml"

PROJECT_DEFAULT_NAME = "Untitled Project"


# -------------------------------------------------------------
# TOML Schema
#
# Types that define the schema for reading/writing project config TOML files.

@dataclass
class ProjectConfig:
    name: str

    @staticmethod
    def new_from_project(project):
        """Create a ProjectConfig object from an existing real project."""
        return ProjectConfig(name=project.name)

    def populate_project(self, project):
        """Fills in an existing real project's data from this ProjectConfig object."""

        # Currently we don't do anything, because the project name is handled
        # separately. But when projects have more than just a name, all of that
        # other data should be handled here, and be symmetric with
        # `new_from_project()` above.
        pass


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
            report({'ERROR'}, rpt_("A file named '{:s}' already exists, but it needs to be a directory.").format(PROJECT_DIR))
        raise ProjectSaveException
    except PermissionError:
        if report:
            report({'ERROR'}, rpt_("Cannot create '{:s}' directory due to file-system permissions.").format(PROJECT_DIR))
        raise ProjectSaveException
    except Exception as e:
        if report:
            report({'ERROR'}, str(e))
        raise ProjectSaveException

    # Create a project config dict from the current project.
    converter = cattrs.Converter()
    config = ProjectConfig.new_from_project(project)
    config_dict = converter.unstructure(config, ProjectConfig)

    # Write the config TOML file.
    config_path = root_path.joinpath(PROJECT_DIR, PROJECT_CONFIG)
    try:
        with config_path.open(mode='wb') as f:
            tomli_w.dump(config_dict, f)
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
    config.populate_project(bpy.data.project)
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

    # Validate schema and convert to ProjectConfig class.
    converter = cattrs.Converter()
    try:
        project_config = converter.structure(config_dict, ProjectConfig)
    except cattrs.BaseValidationError as e:
        if report:
            report({'ERROR'}, rpt_("Invalid project configuration file: {:s}").format(str(e)))
        raise ProjectLoadException

    # Other validation not handled by the schema.
    if project_config.name == "":
        if report:
            report({'ERROR'}, "Invalid project: project name is empty.")
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
                {'ERROR'},
                "New project directory is already inside of an existing project. Try reloading the current blend file to open the existing project.")
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
)


def register():
    bpy.app.handlers.load_pre.append(on_blend_load)
    bpy.app.handlers.save_post.append(on_blend_save)
    bpy.app.handlers.exit_pre.append(on_exit)


def unregister():
    bpy.app.handlers.load_pre.remove(on_blend_load)
    bpy.app.handlers.save_post.remove(on_blend_save)
    bpy.app.handlers.exit_pre.remove(on_exit)
