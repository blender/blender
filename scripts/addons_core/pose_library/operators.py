# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Pose Library - operators.
"""

from pathlib import Path
from typing import Set

_need_reload = "functions" in locals()
from . import asset_browser, functions, pose_creation, pose_usage

if _need_reload:
    import importlib

    asset_browser = importlib.reload(asset_browser)
    functions = importlib.reload(functions)
    pose_creation = importlib.reload(pose_creation)
    pose_usage = importlib.reload(pose_usage)


import bpy
from bpy.props import BoolProperty
from bpy.types import (
    Action,
    AssetRepresentation,
    Context,
    Object,
    Operator,
)
from bpy.app.translations import pgettext_tip as tip_


class PoseAssetCreator:
    @classmethod
    def poll(cls, context: Context) -> bool:
        return bool(
            # There must be an object.
            context.object
            # It must be in pose mode with selected bones.
            and context.object.mode == "POSE"
            and context.object.pose
            and context.selected_pose_bones_from_active_object
        )


class POSELIB_OT_restore_previous_action(Operator):
    bl_idname = "poselib.restore_previous_action"
    bl_label = "Restore Previous Action"
    bl_description = "Switch back to the previous Action, after creating a pose asset"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context: Context) -> bool:
        return bool(
            context.window_manager.poselib_previous_action
            and context.object
            and context.object.animation_data
            and context.object.animation_data.action
            and context.object.animation_data.action.asset_data is not None
        )

    def execute(self, context: Context) -> Set[str]:
        # This is the Action that was just created with "Create Pose Asset".
        # It has to be re-applied after switching to the previous action,
        # to ensure the character keeps the same pose.
        self.pose_action = context.object.animation_data.action

        prev_action = context.window_manager.poselib_previous_action
        context.object.animation_data.action = prev_action
        context.window_manager.poselib_previous_action = None

        # Wait a bit for the action assignment to be handled, before applying the pose.
        wm = context.window_manager
        self._timer = wm.event_timer_add(0.001, window=context.window)
        wm.modal_handler_add(self)

        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        if event.type != 'TIMER':
            return {'RUNNING_MODAL'}

        wm = context.window_manager
        wm.event_timer_remove(self._timer)

        context.object.pose.apply_pose_from_action(self.pose_action)
        return {'FINISHED'}


class ASSET_OT_assign_action(Operator):
    bl_idname = "asset.assign_action"
    bl_label = "Assign Action"
    bl_description = "Set this pose Action as active Action on the active Object"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context: Context) -> bool:
        return bool(isinstance(getattr(context, "id", None), Action) and context.object)

    def execute(self, context: Context) -> Set[str]:
        context.object.animation_data_create().action = context.id
        return {"FINISHED"}


class POSELIB_OT_copy_as_asset(PoseAssetCreator, Operator):
    bl_idname = "poselib.copy_as_asset"
    bl_label = "Copy Pose as Asset"
    bl_description = "Create a new pose asset on the clipboard, to be pasted into an Asset Browser"
    bl_options = {'REGISTER'}

    CLIPBOARD_ASSET_MARKER = "ASSET-BLEND="

    def execute(self, context: Context) -> Set[str]:
        asset = pose_creation.create_pose_asset_from_context(context, new_asset_name=context.object.name)
        if asset is None:
            self.report({"WARNING"}, "No animation data found to create asset from")
            return {"CANCELLED"}

        filepath = self.save_datablock(asset)

        context.window_manager.clipboard = "%s%s" % (
            self.CLIPBOARD_ASSET_MARKER,
            filepath,
        )
        asset_browser.tag_redraw(context.screen)
        self.report({"INFO"}, "Pose Asset copied, use Paste As New Asset in any Asset Browser to paste")

        # The asset has been saved to disk, so to clean up it has to loose its asset & fake user status.
        asset.asset_clear()
        asset.use_fake_user = False

        # The asset can be removed from the main DB, as it was purely created to
        # be stored to disk, and not to be used in this file.
        if asset.users > 0:
            # This should never happen, and indicates a bug in the code. Having a warning about it is nice,
            # but it shouldn't stand in the way of actually cleaning up the meant-to-be-temporary datablock.
            self.report({"WARNING"}, "Unexpected non-zero user count for the asset, please report this as a bug")

        bpy.data.actions.remove(asset)
        return {"FINISHED"}

    def save_datablock(self, action: Action) -> Path:
        tempdir = Path(bpy.app.tempdir)
        filepath = tempdir / "copied_asset.blend"
        bpy.data.libraries.write(
            str(filepath),
            datablocks={action},
            path_remap="NONE",
            fake_user=True,
            compress=True,  # Single-datablock blend file, likely little need to diff.
        )
        return filepath


class POSELIB_OT_paste_asset(Operator):
    bl_idname = "poselib.paste_asset"
    bl_label = "Paste as New Asset"
    bl_description = "Paste the Asset that was previously copied using Copy As Asset"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context: Context) -> bool:
        from bpy_extras import asset_utils

        if not asset_utils.SpaceAssetInfo.is_asset_browser(context.space_data):
            cls.poll_message_set("Current editor is not an asset browser")
            return False

        asset_lib_ref = context.space_data.params.asset_library_reference
        if asset_lib_ref != 'LOCAL':
            cls.poll_message_set("Asset Browser must be set to the Current File library")
            return False

        # Delay checking the clipboard as much as possible, as it's CPU-heavier than the other checks.
        clipboard: str = context.window_manager.clipboard
        if not clipboard:
            cls.poll_message_set("Clipboard is empty")
            return False

        marker = POSELIB_OT_copy_as_asset.CLIPBOARD_ASSET_MARKER
        if not clipboard.startswith(marker):
            cls.poll_message_set("Clipboard does not contain an asset")
            return False

        return True

    def execute(self, context: Context) -> Set[str]:
        clipboard = context.window_manager.clipboard
        marker_len = len(POSELIB_OT_copy_as_asset.CLIPBOARD_ASSET_MARKER)
        filepath = Path(clipboard[marker_len:])

        assets = functions.load_assets_from(filepath)
        if not assets:
            self.report({"ERROR"}, "Did not find any assets on clipboard")
            return {"CANCELLED"}

        self.report({"INFO"}, tip_("Pasted %d assets") % len(assets))

        bpy.ops.asset.library_refresh()

        asset_browser_area = asset_browser.area_from_context(context)
        if not asset_browser_area:
            return {"FINISHED"}

        # Assign same catalog as in asset browser.
        catalog_id = asset_browser.active_catalog_id(asset_browser_area)
        for asset in assets:
            asset.asset_data.catalog_id = catalog_id
        asset_browser.activate_asset(assets[0], asset_browser_area, deferred=True)

        return {"FINISHED"}


class PoseAssetUser:
    @classmethod
    def poll(cls, context: Context) -> bool:
        if not (
            context.object
            and context.object.mode == "POSE"  # This condition may not be desired.
            and context.asset
        ):
            return False
        return context.asset.id_type == 'ACTION'

    def execute(self, context: Context) -> Set[str]:
        asset: AssetRepresentation = context.asset
        if asset.local_id:
            return self.use_pose(context, asset.local_id)
        return self._load_and_use_pose(context)

    def use_pose(self, context: Context, asset: bpy.types.ID) -> Set[str]:
        # Implement in subclass.
        pass

    def _load_and_use_pose(self, context: Context) -> Set[str]:
        asset = context.asset
        asset_lib_path = asset.full_library_path

        if not asset_lib_path:
            self.report(  # type: ignore
                {"ERROR"},
                # TODO: Add some way to get the library name from the library reference
                # (just asset_library_reference.name?).
                tip_("Selected asset %s could not be located inside the asset library") % asset.name,
            )
            return {"CANCELLED"}
        if asset.id_type != 'ACTION':
            self.report(  # type: ignore
                {"ERROR"},
                tip_("Selected asset %s is not an Action") % asset.name,
            )
            return {"CANCELLED"}

        with bpy.types.BlendData.temp_data() as temp_data:
            with temp_data.libraries.load(asset_lib_path) as (data_from, data_to):
                data_to.actions = [asset.name]

            action: Action = data_to.actions[0]
            return self.use_pose(context, action)


class POSELIB_OT_pose_asset_select_bones(PoseAssetUser, Operator):
    bl_idname = "poselib.pose_asset_select_bones"
    bl_label = "Select Bones"
    bl_description = "Select those bones that are used in this pose"
    bl_options = {'REGISTER', 'UNDO'}

    select: BoolProperty(name="Select", default=True)  # type: ignore
    flipped: BoolProperty(name="Flipped", default=False)  # type: ignore

    def use_pose(self, context: Context, pose_asset: Action) -> Set[str]:
        for object in context.selected_objects:
            pose_usage.select_bones(object, pose_asset, select=self.select, flipped=self.flipped)
        if self.select:
            msg = tip_("Selected bones from %s") % pose_asset.name
        else:
            msg = tip_("Deselected bones from %s") % pose_asset.name
        self.report({"INFO"}, msg)
        return {"FINISHED"}

    @classmethod
    def description(cls, _context: Context, properties: 'POSELIB_OT_pose_asset_select_bones') -> str:
        if properties.select:
            return tip_(cls.bl_description)
        return tip_("Deselect those bones that are used in this pose")


classes = (
    ASSET_OT_assign_action,
    POSELIB_OT_copy_as_asset,
    POSELIB_OT_paste_asset,
    POSELIB_OT_pose_asset_select_bones,
    POSELIB_OT_restore_previous_action,
)

register, unregister = bpy.utils.register_classes_factory(classes)
