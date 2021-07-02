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
from __future__ import annotations
from pathlib import Path

import bpy
from bpy.types import Operator

from bpy_extras.asset_utils import (
    SpaceAssetInfo,
)


class ASSET_OT_tag_add(Operator):
    """Add a new keyword tag to the active asset"""

    bl_idname = "asset.tag_add"
    bl_label = "Add Asset Tag"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return SpaceAssetInfo.is_asset_browser_poll(context) and SpaceAssetInfo.get_active_asset(context)

    def execute(self, context):
        active_asset = SpaceAssetInfo.get_active_asset(context)
        active_asset.tags.new("Unnamed Tag")

        return {'FINISHED'}


class ASSET_OT_tag_remove(Operator):
    """Remove an existing keyword tag from the active asset"""

    bl_idname = "asset.tag_remove"
    bl_label = "Remove Asset Tag"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not SpaceAssetInfo.is_asset_browser_poll(context):
            return False

        active_asset = SpaceAssetInfo.get_active_asset(context)
        if not active_asset:
            return False

        return active_asset.active_tag in range(len(active_asset.tags))

    def execute(self, context):
        active_asset = SpaceAssetInfo.get_active_asset(context)
        tag = active_asset.tags[active_asset.active_tag]

        active_asset.tags.remove(tag)
        active_asset.active_tag -= 1

        return {'FINISHED'}


class ASSET_OT_open_containing_blend_file(Operator):
    """Open the blend file that contains the active asset"""

    bl_idname = "asset.open_containing_blend_file"
    bl_label = "Open Blend File"
    bl_options = {'REGISTER'}

    _process = None  # Optional[subprocess.Popen]

    @classmethod
    def poll(cls, context):
        asset_file_handle = getattr(context, 'asset_file_handle', None)
        asset_library = getattr(context, 'asset_library', None)

        if not asset_library:
            cls.poll_message_set("No asset library selected")
            return False
        if not asset_file_handle:
            cls.poll_message_set("No asset selected")
            return False
        if asset_file_handle.local_id:
            cls.poll_message_set("Selected asset is contained in the current file")
            return False
        return True

    def execute(self, context):
        asset_file_handle = context.asset_file_handle
        asset_library = context.asset_library

        if asset_file_handle.local_id:
            self.report({'WARNING'}, "This asset is stored in the current blend file")
            return {'CANCELLED'}

        asset_lib_path = bpy.types.AssetHandle.get_full_library_path(asset_file_handle, asset_library)
        self.open_in_new_blender(asset_lib_path)

        wm = context.window_manager
        self._timer = wm.event_timer_add(0.1, window=context.window)
        wm.modal_handler_add(self)

        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        if event.type != 'TIMER':
            return {'PASS_THROUGH'}

        if self._process is None:
            self.report({'ERROR'}, "Unable to find any running process")
            self.cancel(context)
            return {'CANCELLED'}

        returncode = self._process.poll()
        if returncode is None:
            # Process is still running.
            return {'RUNNING_MODAL'}

        if returncode:
            self.report({'WARNING'}, "Blender subprocess exited with error code %d" % returncode)

        # TODO(Sybren): Replace this with a generic "reload assets" operator
        # that can run outside of the Asset Browser context.
        if bpy.ops.file.refresh.poll():
            bpy.ops.file.refresh()
        if bpy.ops.asset.list_refresh.poll():
            bpy.ops.asset.list_refresh()

        self.cancel(context)
        return {'FINISHED'}

    def cancel(self, context):
        wm = context.window_manager
        wm.event_timer_remove(self._timer)

    def open_in_new_blender(self, filepath):
        import subprocess

        cli_args = [bpy.app.binary_path, str(filepath)]
        self._process = subprocess.Popen(cli_args)


classes = (
    ASSET_OT_tag_add,
    ASSET_OT_tag_remove,
    ASSET_OT_open_containing_blend_file,
)
