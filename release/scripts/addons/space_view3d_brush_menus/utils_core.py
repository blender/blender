# gpl author: Ryan Inch (Imaginer)

import bpy

get_addon_name = 'space_view3d_brush_menus'

# Property Icon Width
PIW = '       '


# check for (currently) brushes being linked
def get_brush_link(context, types="brush"):
    tool_settings = context.tool_settings
    has_brush = None

    if get_mode() == 'SCULPT':
        datapath = tool_settings.sculpt

    elif get_mode() == 'VERTEX_PAINT':
        datapath = tool_settings.vertex_paint

    elif get_mode() == 'WEIGHT_PAINT':
        datapath = tool_settings.weight_paint

    elif get_mode() == 'TEXTURE_PAINT':
        datapath = tool_settings.image_paint
    else:
        datapath = None

    if types == "brush":
        has_brush = getattr(datapath, "brush", None)

    return has_brush


# Addon settings
def addon_settings(lists=True):
    # separate function just for more convience
    addon = bpy.context.user_preferences.addons[get_addon_name]
    colum_n = addon.preferences.column_set if addon else 1
    use_list = addon.preferences.use_brushes_menu_type

    return use_list if lists else colum_n


def error_handlers(self, op_name, error, reports="ERROR", func=False):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    is_func = "Function" if func else "Operator"
    print("\n[Sculpt/Paint Brush Menus]\n{}: {}\nError: {}\n".format(is_func, op_name, error))


# Object modes:
# 'OBJECT' 'EDIT' 'SCULPT'
# 'VERTEX_PAINT' 'WEIGHT_PAINT' 'TEXTURE_PAINT'
# 'PARTICLE_EDIT' 'POSE' 'GPENCIL_EDIT'
def get_mode():
    try:
        if bpy.context.gpencil_data and \
        bpy.context.object.mode == 'OBJECT' and \
        bpy.context.scene.grease_pencil.use_stroke_edit_mode:
            return 'GPENCIL_EDIT'
        else:
            return bpy.context.object.mode
    except:
        return None


def menuprop(item, name, value, data_path,
             icon='NONE', disable=False, disable_icon=None,
             custom_disable_exp=None, method=None, path=False):

    # disable the ui
    if disable:
        disabled = False

        # used if you need a custom expression to disable the ui
        if custom_disable_exp:
            if custom_disable_exp[0] == custom_disable_exp[1]:
                item.enabled = False
                disabled = True

        # check if the ui should be disabled for numbers
        elif isinstance(eval("bpy.context.{}".format(data_path)), float):
            if round(eval("bpy.context.{}".format(data_path)), 2) == value:
                item.enabled = False
                disabled = True

        # check if the ui should be disabled for anything else
        else:
            if eval("bpy.context.{}".format(data_path)) == value:
                item.enabled = False
                disabled = True

        # change the icon to the disable_icon if the ui has been disabled
        if disable_icon and disabled:
            icon = disable_icon

    # creates the menu item
    prop = item.operator("wm.context_set_value", text=name, icon=icon)

    # sets what the menu item changes
    if path:
        prop.value = value
        value = eval(value)

    elif type(value) == str:
        prop.value = "'{}'".format(value)

    else:
        prop.value = '{}'.format(value)

    # sets the path to what is changed
    prop.data_path = data_path
