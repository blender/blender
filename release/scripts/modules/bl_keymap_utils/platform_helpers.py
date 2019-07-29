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


def keyconfig_data_oskey_from_ctrl(keyconfig_data_src, filter_fn=None):
    keyconfig_data_dst = []
    for km_name, km_parms, km_items_data_src in keyconfig_data_src:
        km_items_data_dst = km_items_data_src.copy()
        items_dst = []
        km_items_data_dst["items"] = items_dst
        for item_src in km_items_data_src["items"]:
            item_op, item_event, item_prop = item_src
            if "ctrl" in item_event:
                if filter_fn is None or filter_fn(item_event):
                    item_event = item_event.copy()
                    item_event["oskey"] = item_event["ctrl"]
                    del item_event["ctrl"]
                    items_dst.append((item_op, item_event, item_prop))
            items_dst.append(item_src)
        keyconfig_data_dst.append((km_name, km_parms, km_items_data_dst))
    return keyconfig_data_dst


def keyconfig_data_oskey_from_ctrl_for_macos(keyconfig_data_src):
    """Use for apple since Cmd is typically used in-place of Ctrl."""
    def filter_fn(item_event):
        if (item_event["type"] in {
                'H',
                'M',
                'SPACE',
                'W',
                'ACCENT_GRAVE',
        }) and (
            item_event.get("ctrl") and
            (not item_event.get("alt")) and
            (not item_event.get("shift"))
        ):
            return False
        return True

    return keyconfig_data_oskey_from_ctrl(keyconfig_data_src, filter_fn)
