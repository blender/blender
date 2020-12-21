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
        return space_data.type == 'FILE_BROWSER' and space_data.browse_mode == 'ASSETS'

    @classmethod
    def is_asset_browser_poll(cls, context: Context):
        return cls.is_asset_browser(context.space_data)

    @classmethod
    def get_active_asset(cls, context: Context):
        if hasattr(context, "active_file"):
            active_file = context.active_file
            return active_file.asset_data if active_file else None

class AssetBrowserPanel:
    bl_space_type = 'FILE_BROWSER'

    @classmethod
    def poll(cls, context):
        return SpaceAssetInfo.is_asset_browser_poll(context)

class AssetMetaDataPanel:
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'

    @classmethod
    def poll(cls, context):
        active_file = context.active_file
        return SpaceAssetInfo.is_asset_browser_poll(context) and active_file and active_file.asset_data
