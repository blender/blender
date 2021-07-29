import bpy
import os
import glob
import bpy.utils.previews

import sverchok

# custom icons dictionary
_icon_collection = {}
addon_name = sverchok.__name__

def custom_icon(name):
    load_custom_icons()  # load in case they custom icons not already loaded

    custom_icons = _icon_collection["main"]

    default = lambda: None  # for no icon with given name will return zero
    default.icon_id = 0

    return custom_icons.get(name, default).icon_id


def load_custom_icons():
    if len(_icon_collection):  # return if custom icons already loaded
        return

    custom_icons = bpy.utils.previews.new()

    iconsDir = os.path.join(os.path.dirname(__file__), "icons")
    iconPattern = "sv_*.png"
    iconPath = os.path.join(iconsDir, iconPattern)
    iconFiles = [os.path.basename(x) for x in glob.glob(iconPath)]

    for iconFile in iconFiles:
        iconName = os.path.splitext(iconFile)[0]
        iconID = iconName.upper()
        custom_icons.load(iconID, os.path.join(iconsDir, iconFile), "IMAGE")

    _icon_collection["main"] = custom_icons


def remove_custom_icons():
    for custom_icons in _icon_collection.values():
        bpy.utils.previews.remove(custom_icons)
    _icon_collection.clear()

def get_icon_switch():
    """Return show_icons setting from addon preferences"""

    addon = bpy.context.user_preferences.addons.get(addon_name)

    if addon and hasattr(addon, "preferences"):
        return addon.preferences.show_icons

def icon(display_icon):
    '''returns empty dict if show_icons is False, else the icon passed'''
    kws = {}
    if get_icon_switch():
        if display_icon.startswith('SV_'):
            kws = {'icon_value': custom_icon(display_icon)}
        elif display_icon != 'OUTLINER_OB_EMPTY':
            kws = {'icon': display_icon}
    return kws


def node_icon(node_ref):
    '''returns empty dict if show_icons is False, else the icon passed'''
    if not get_icon_switch():
        return {}
    else:
        if hasattr(node_ref, 'sv_icon'):
            iconID = custom_icon(node_ref.sv_icon)
            return {'icon_value': iconID} if iconID else {}
        elif hasattr(node_ref, 'bl_icon') and node_ref.bl_icon != 'OUTLINER_OB_EMPTY':
            iconID = node_ref.bl_icon
            return {'icon': iconID} if iconID else {}
        else:
            return {}

def register():
    load_custom_icons()


def unregister():
    remove_custom_icons()

if __name__ == '__main__':
    register()
