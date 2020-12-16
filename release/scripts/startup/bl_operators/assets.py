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

import bpy

from bpy_extras.asset_utils import (
    SpaceAssetInfo,
)


class ASSET_OT_tag_add(bpy.types.Operator):
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


class ASSET_OT_tag_remove(bpy.types.Operator):
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


classes = (
    ASSET_OT_tag_add,
    ASSET_OT_tag_remove,
)
