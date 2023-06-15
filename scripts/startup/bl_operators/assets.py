# SPDX-FileCopyrightText: 2020-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import bpy
from bpy.types import Operator
from bpy.app.translations import (
    pgettext_data as data_,
    pgettext_tip as tip_,
)


from bpy_extras.asset_utils import (
    SpaceAssetInfo,
)


class AssetBrowserMetadataOperator:
    @classmethod
    def poll(cls, context):
        if not SpaceAssetInfo.is_asset_browser_poll(context) or not context.asset_file_handle:
            return False

        if not context.asset_file_handle.local_id:
            Operator.poll_message_set(
                "Asset metadata from external asset libraries can't be "
                "edited, only assets stored in the current file can"
            )
            return False
        return True


class ASSET_OT_tag_add(AssetBrowserMetadataOperator, Operator):
    """Add a new keyword tag to the active asset"""

    bl_idname = "asset.tag_add"
    bl_label = "Add Asset Tag"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        active_asset = SpaceAssetInfo.get_active_asset(context)
        active_asset.tags.new(data_("Tag"))

        return {'FINISHED'}


class ASSET_OT_tag_remove(AssetBrowserMetadataOperator, Operator):
    """Remove an existing keyword tag from the active asset"""

    bl_idname = "asset.tag_remove"
    bl_label = "Remove Asset Tag"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        active_asset_file = context.asset_file_handle
        asset_metadata = active_asset_file.asset_data
        return asset_metadata.active_tag in range(len(asset_metadata.tags))

    def execute(self, context):
        active_asset_file = context.asset_file_handle
        asset_metadata = active_asset_file.asset_data
        tag = asset_metadata.tags[asset_metadata.active_tag]

        asset_metadata.tags.remove(tag)
        asset_metadata.active_tag -= 1

        return {'FINISHED'}


class ASSET_OT_open_containing_blend_file(Operator):
    """Open the blend file that contains the active asset"""

    bl_idname = "asset.open_containing_blend_file"
    bl_label = "Open Blend File"
    bl_options = {'REGISTER'}

    _process = None  # Optional[subprocess.Popen]

    @classmethod
    def poll(cls, context):
        asset_file_handle = getattr(context, "asset_file_handle", None)

        if not asset_file_handle:
            cls.poll_message_set("No asset selected")
            return False
        if asset_file_handle.local_id:
            cls.poll_message_set("Selected asset is contained in the current file")
            return False
        return True

    def execute(self, context):
        asset_file_handle = context.asset_file_handle

        if asset_file_handle.local_id:
            self.report({'WARNING'}, "This asset is stored in the current blend file")
            return {'CANCELLED'}

        asset_lib_path = bpy.types.AssetHandle.get_full_library_path(asset_file_handle)
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
            self.report({'WARNING'}, tip_("Blender sub-process exited with error code %d") % returncode)

        if bpy.ops.asset.library_refresh.poll():
            bpy.ops.asset.library_refresh()

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
