# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Pose Library - GUI definition.
"""

import bpy
from bpy.types import (
    AssetRepresentation,
    Context,
    Menu,
    Panel,
    UILayout,
    UIList,
)
from _bl_ui_utils.layout import operator_context


class VIEW3D_MT_pose_modify(Menu):
    bl_label = "Modify Pose Asset"

    def draw(self, _context):
        layout = self.layout

        layout.operator("poselib.asset_modify", text="Replace").mode = "REPLACE"
        layout.operator("poselib.asset_modify", text="Add Selected Bones").mode = "ADD"
        layout.operator("poselib.asset_modify", text="Remove Selected Bones").mode = "REMOVE"


class PoseLibraryPanel:
    @classmethod
    def pose_library_panel_poll(cls, context: Context) -> bool:
        return context.mode == 'POSE'

    @classmethod
    def poll(cls, context: Context) -> bool:
        return cls.pose_library_panel_poll(context)


class VIEW3D_AST_pose_library(bpy.types.AssetShelf):
    bl_space_type = "VIEW_3D"
    bl_activate_operator = "POSELIB_OT_apply_pose_asset"
    bl_drag_operator = "POSELIB_OT_blend_pose_asset"
    bl_default_preview_size = 64

    @classmethod
    def poll(cls, context: Context) -> bool:
        return PoseLibraryPanel.poll(context)

    @classmethod
    def asset_poll(cls, asset: AssetRepresentation) -> bool:
        return asset.id_type == 'ACTION'

    @classmethod
    def draw_context_menu(cls, _context: Context, _asset: AssetRepresentation, layout: UILayout):
        layout.operator("poselib.apply_pose_asset", text="Apply Pose").flipped = False
        layout.operator("poselib.apply_pose_asset", text="Apply Pose Flipped").flipped = True

        with operator_context(layout, 'INVOKE_DEFAULT'):
            layout.operator("poselib.blend_pose_asset", text="Blend Pose").flipped = False
            layout.operator("poselib.blend_pose_asset", text="Blend Pose Flipped").flipped = True

        layout.separator()
        props = layout.operator("poselib.pose_asset_select_bones", text="Select Pose Bones")
        props.select = True
        props = layout.operator("poselib.pose_asset_select_bones", text="Deselect Pose Bones")
        props.select = False

        layout.separator()
        layout.operator("poselib.asset_modify", text="Adjust Pose Asset").mode = 'ADJUST'
        layout.menu("VIEW3D_MT_pose_modify")
        layout.operator("poselib.asset_delete")

        layout.separator()
        layout.operator("asset.open_containing_blend_file")


def pose_library_asset_browser_context_menu(self: UIList, context: Context) -> None:
    def is_pose_library_asset_browser() -> bool:
        asset_library_ref = getattr(context, "asset_library_reference", None)
        if not asset_library_ref:
            return False
        asset = getattr(context, "asset", None)
        if not asset:
            return False
        return bool(asset.id_type == 'ACTION')

    if not is_pose_library_asset_browser():
        return

    layout = self.layout

    layout.separator()

    layout.operator("poselib.apply_pose_asset", text="Apply Pose").flipped = False
    layout.operator("poselib.apply_pose_asset", text="Apply Pose Flipped").flipped = True

    with operator_context(layout, 'INVOKE_DEFAULT'):
        layout.operator("poselib.blend_pose_asset", text="Blend Pose").flipped = False
        layout.operator("poselib.blend_pose_asset", text="Blend Pose Flipped").flipped = True

    layout.separator()
    props = layout.operator("poselib.pose_asset_select_bones", text="Select Pose Bones")
    props.select = True
    props = layout.operator("poselib.pose_asset_select_bones", text="Deselect Pose Bones")
    props.select = False

    layout.separator()
    layout.operator("poselib.asset_modify", text="Adjust Pose Asset").mode = 'ADJUST'
    layout.menu("VIEW3D_MT_pose_modify")
    with operator_context(layout, 'INVOKE_DEFAULT'):
        layout.operator("poselib.asset_delete")

    layout.separator()
    layout.operator("asset.assign_action")

    layout.separator()


class DOPESHEET_PT_asset_panel(PoseLibraryPanel, Panel):
    bl_space_type = "DOPESHEET_EDITOR"
    bl_region_type = "UI"
    bl_label = "Create Pose Asset"
    bl_category = "Action"

    def draw(self, context: Context) -> None:
        layout = self.layout
        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("poselib.create_pose_asset")
        if bpy.types.POSELIB_OT_restore_previous_action.poll(context):
            row.operator("poselib.restore_previous_action", text="", icon='LOOP_BACK')
        col.operator("poselib.copy_as_asset", icon="COPYDOWN")


def pose_library_list_item_asset_menu(self: UIList, context: Context) -> None:
    layout = self.layout
    layout.menu("ASSETBROWSER_MT_asset")


class ASSETBROWSER_MT_asset(Menu):
    bl_label = "Asset"

    @classmethod
    def poll(cls, context):
        from bpy_extras.asset_utils import SpaceAssetInfo

        return SpaceAssetInfo.is_asset_browser_poll(context)

    def draw(self, context: Context) -> None:
        layout = self.layout

        layout.operator("poselib.paste_asset", icon='PASTEDOWN')
        layout.separator()
        layout.operator("poselib.create_pose_asset")


# Messagebus subscription to monitor asset library changes.
_msgbus_owner = object()


def _on_asset_library_changed() -> None:
    """Update areas when a different asset library is selected."""
    refresh_area_types = {'DOPESHEET_EDITOR', 'VIEW_3D'}
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type not in refresh_area_types:
                continue

            area.tag_redraw()


def register_message_bus() -> None:
    bpy.msgbus.subscribe_rna(
        key=(bpy.types.FileAssetSelectParams, "asset_library_reference"),
        owner=_msgbus_owner,
        args=(),
        notify=_on_asset_library_changed,
        options={'PERSISTENT'},
    )


def unregister_message_bus() -> None:
    bpy.msgbus.clear_by_owner(_msgbus_owner)


@bpy.app.handlers.persistent
def _on_blendfile_load_pre(none, other_none) -> None:
    # The parameters are required, but both are None.
    unregister_message_bus()


@bpy.app.handlers.persistent
def _on_blendfile_load_post(none, other_none) -> None:
    # The parameters are required, but both are None.
    register_message_bus()


classes = (
    DOPESHEET_PT_asset_panel,
    ASSETBROWSER_MT_asset,
    VIEW3D_MT_pose_modify,
    VIEW3D_AST_pose_library,
)

_register, _unregister = bpy.utils.register_classes_factory(classes)


def register() -> None:
    _register()

    bpy.types.ASSETBROWSER_MT_context_menu.prepend(pose_library_asset_browser_context_menu)
    bpy.types.ASSETBROWSER_MT_editor_menus.append(pose_library_list_item_asset_menu)

    register_message_bus()
    bpy.app.handlers.load_pre.append(_on_blendfile_load_pre)
    bpy.app.handlers.load_post.append(_on_blendfile_load_post)


def unregister() -> None:
    _unregister()

    unregister_message_bus()

    bpy.types.ASSETBROWSER_MT_context_menu.remove(pose_library_asset_browser_context_menu)
    bpy.types.ASSETBROWSER_MT_editor_menus.remove(pose_library_list_item_asset_menu)
