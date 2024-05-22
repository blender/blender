# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Helpers for asset management tasks.
"""

import bpy
from bpy.types import (
    Context,
)

__all__ = (
    "SpaceAssetInfo",
)


class SpaceAssetInfo:
    @classmethod
    def is_asset_browser(cls, space_data: bpy.types.Space):
        return space_data and space_data.type == 'FILE_BROWSER' and space_data.browse_mode == 'ASSETS'

    @classmethod
    def is_asset_browser_poll(cls, context: Context):
        return cls.is_asset_browser(context.space_data)

    @classmethod
    def get_active_asset(cls, context: Context):
        if active_file := getattr(context, "active_file", None):
            return active_file.asset_data


class AssetBrowserPanel:
    bl_space_type = 'FILE_BROWSER'

    @classmethod
    def asset_browser_panel_poll(cls, context):
        return SpaceAssetInfo.is_asset_browser_poll(context)

    @classmethod
    def poll(cls, context):
        return cls.asset_browser_panel_poll(context)


class AssetMetaDataPanel:
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'

    @classmethod
    def poll(cls, context):
        active_file = context.active_file
        return SpaceAssetInfo.is_asset_browser_poll(context) and active_file and active_file.asset_data
