# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Helpers for asset management tasks.
"""

__all__ = (
    "AssetBrowserPanel",
    "AssetMetaDataPanel",
    "SpaceAssetInfo",
)


class SpaceAssetInfo:
    """Utility class for checking if a space is an asset browser."""

    @classmethod
    def is_asset_browser(cls, space_data):
        """
        Check if the given space is an asset browser.

        :param space_data: The space to check.
        :type space_data: :class:`bpy.types.Space`
        :return: True when the space is an asset browser.
        :rtype: bool
        """
        return space_data and space_data.type == 'FILE_BROWSER' and space_data.browse_mode == 'ASSETS'

    @classmethod
    def is_asset_browser_poll(cls, context):
        """
        Poll whether the active space is an asset browser.

        :param context: The context.
        :type context: :class:`bpy.types.Context`
        :return: True when the active space is an asset browser.
        :rtype: bool
        """
        return cls.is_asset_browser(context.space_data)


class AssetBrowserPanel:
    """Mixin class for panels that should only show in the asset browser."""

    bl_space_type = 'FILE_BROWSER'

    @classmethod
    def asset_browser_panel_poll(cls, context):
        """
        Check if the panel should be shown in the asset browser.

        :param context: The context.
        :type context: :class:`bpy.types.Context`
        :return: True when the panel should be visible.
        :rtype: bool
        """
        return SpaceAssetInfo.is_asset_browser_poll(context)

    @classmethod
    def poll(cls, context):
        """
        Poll for asset browser visibility.

        :param context: The context.
        :type context: :class:`bpy.types.Context`
        :return: True when the panel should be visible.
        :rtype: bool
        """
        return cls.asset_browser_panel_poll(context)


class AssetMetaDataPanel:
    """Mixin class for panels that display asset metadata in the asset browser."""

    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'

    @classmethod
    def poll(cls, context):
        """
        Poll for asset browser with active asset metadata.

        :param context: The context.
        :type context: :class:`bpy.types.Context`
        :return: True when the asset browser has active asset data.
        :rtype: bool
        """
        active_file = context.active_file
        return SpaceAssetInfo.is_asset_browser_poll(context) and active_file and active_file.asset_data
