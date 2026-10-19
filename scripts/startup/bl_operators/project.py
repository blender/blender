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

# Directory and file name where the project is read/written to disk.
PROJECT_DIR = ".blender_project"

PROJECT_DEFAULT_NAME = "Untitled Project"
ASSET_LIBRARY_DEFAULT_NAME = "Untitled Asset Library"


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
def find_and_load_project_for_blend_path(context, blend_path, report=None):
    """
    Load the project the blend file is in, or clears the project if none is found.

    `blend_path` should be an absolute path. An empty path is a new file,
    which keeps the active project.

    Throws a ProjectLoadException if a project is found but is invalid
    (missing config file, config validation error, etc.).

    Optionally takes an `Operator.report` for reporting errors to the user.
    """

    from pathlib import Path

    if blend_path == "":
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
    try:
        bpy.data.project_init_from_root_path(str(root_path))
    except RuntimeError:
        raise ProjectLoadException


def find_project_root_from_blend_file_path(blend_path):
    """
    Search for a project root in the parent directories of the given path.

    Returns the project root if found, or None otherwise.
    """

    for parent in blend_path.parents:
        if parent.joinpath(PROJECT_DIR).is_dir():
            return parent
    return None


def blend_file_is_has_project_root(blend_file_path):
    """
    Return whether the blend file has a project root.

    True if the blend file is inside a folder structure with valid project root folder,
    false if no project is found or if the project is invalid.
    """
    project_root = find_project_root_from_blend_file_path(blend_file_path)
    if project_root is None:
        return False

    return True


# -------------------------------------------------------------
# Operators

class PROJECT_OT_SaveProject(Operator):
    """Save the current project to disk"""
    bl_idname = "project.save_project"
    bl_label = "Save Project"

    @classmethod
    def poll(cls, context):
        return bpy.data.project is not None

    def execute(self, context):
        try:
            bpy.data.project_save()
        except RuntimeError:
            return {'CANCELLED'}

        return {'FINISHED'}


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
            self.report({'ERROR'}, "Cannot create an asset library with an empty directory path.")
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
            bpy.data.project_save()
        except RuntimeError:
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
        bpy.data.project_save()

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
        bpy.data.project_save()


# -----------------------------------------------------------------------------
# Register

classes = (
    PROJECT_OT_SaveProject,
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
