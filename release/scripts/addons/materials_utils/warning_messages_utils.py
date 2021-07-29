# gpl: author lijenstina
# -*- coding: utf-8 -*-

import bpy

# Globals #

# change the name for the properties settings
MAT_SPEC_NAME = "materials_specials"

# collect messages for the report operator
COLLECT_REPORT = []


# Functions

def warning_messages(operator=None, warn='DEFAULT', object_name="", is_mat=None,
                     fake="", override=False):
    # Enter warning messages to the message dictionary
    # warn - if nothing passed falls back to DEFAULT
    # a list of strings can be passed and concatenated in obj_name too
    # is_mat a switch to change to materials or textures for obj_name('MAT','TEX', 'FILE', None)
    # fake - optional string that can be passed
    # MAX_COUNT - max members of an list to be displayed
    # override - important messages that should be enabled, no matter the setting

    # pass the show_warnings bool to enable/disable them
    addon = bpy.context.user_preferences.addons[MAT_SPEC_NAME]
    get_warn = addon.preferences.show_warnings if addon else False
    show_warn = get_warn if override is False else override

    if show_warn and operator:
        obj_name = ""
        MAX_COUNT = 6
        gramma_s, gramma_p = " - has ", " - have "

        if is_mat:
            if is_mat in ('MAT'):
                gramma_s, gramma_p = " - Material has ", " - Materials have "
            elif is_mat in ('TEX'):
                gramma_s, gramma_p = " - Texture has ", " - Textures have "
            elif is_mat in ('FILE'):
                gramma_s, gramma_p = " - File ", " - Files "

        # print the whole list in the console if abbreviated
        obj_size_big = False

        if object_name:
            if type(object_name) is list:
                obj_name = ", ".join(object_name)
                obj_size = len(object_name)

                # compare string list size
                if (1 < obj_size <= MAX_COUNT):
                    obj_name = "{}{}".format(obj_name, gramma_p)
                elif (obj_size > MAX_COUNT):
                    abbrevation = ("(Multiple)" if is_mat else "(Multiple Objects)")
                    obj_size_big = True
                    obj_name = "{}{}".format(abbrevation, gramma_p)
                elif (obj_size == 1):
                    obj_name = "{}{}".format(obj_name, gramma_s)
            else:
                obj_name = "{}{}".format(object_name, gramma_s)

        message = {
            'DEFAULT': "No editable selected objects, could not finish",
            'PLACEHOLDER': "{}{}".format(warn, " - Message key is not present in the warning_message_utils"),
            'RMV_EDIT': "{}{}".format(obj_name, "Unable to remove material slot in edit mode)"),
            'A_OB_MIX_NO_MAT': "{}{}".format(obj_name, "No Material applied. Object type can't have materials"),
            'A_MAT_NAME_EDIT': "{}{}".format(obj_name, " been applied to selection"),
            'C_OB_NO_MAT': "{}{}".format(obj_name, "No Materials. Unused material slots are "
                                         "not cleaned"),
            'C_OB_MIX_NO_MAT': "{}{}".format(obj_name, "No Materials or an Object type that "
                                             "can't have Materials (Clean Material Slots)"),
            'R_OB_NO_MAT': "{}{}".format(obj_name, "No Materials. Nothing to remove"),
            'R_OB_FAIL_MAT': "{}{}".format(obj_name, "Failed to remove materials - (Operator Error)"),
            'R_NO_SL_MAT': "No Selection. Material slots are not removed",
            'R_ALL_SL_MAT': "All materials removed from selected objects",
            'R_ALL_NO_MAT': "Object(s) have no materials to remove",
            'R_ACT_MAT': "{}{}".format(obj_name, "Removed active Material"),
            'R_ACT_MAT_ALL': "{}{}".format(obj_name, "Removed all Material from the Object"),
            'SL_MAT_BY_NAME': "{}{}{}".format("Objects with the Material ", obj_name, "been selected"),
            'OB_CANT_MAT': "{}{}".format(obj_name, "Object type that can't have Materials"),
            'REP_MAT_NONE': "Replace Material: No materials replaced",
            'FAKE_SET_ON': "{}{}{}".format(obj_name, "set Fake user ", fake),
            'FAKE_SET_OFF': "{}{}{}".format(obj_name, "disabled Fake user ", fake),
            'FAKE_NO_MAT': "Fake User Settings: Object(s) with no Materials or no changes needed",
            'CPY_MAT_MIX_OB': "Copy Materials to others: Some of the Object types can't have Materials",
            'CPY_MAT_ONE_OB': "Copy Materials to others: Only one object selected",
            'CPY_MAT_FAIL': "Copy Materials to others: (Operator Error)",
            'CPY_MAT_DONE': "Materials are copied from active to selected objects",
            'TEX_MAT_NO_SL': "Texface to Material: No Selected Objects",
            'TEX_MAT_NO_CRT': "{}{}".format(obj_name, "not met the conditions for the tool (UVs, Active Images)"),
            'MAT_TEX_NO_SL': "Material to Texface: No Selected Objects",
            'MAT_TEX_NO_MESH': "{}{}".format(obj_name, "not met the conditions for the tool (Mesh)"),
            'MAT_TEX_NO_MAT': "{}{}".format(obj_name, "not met the conditions for the tool (Material)"),
            'BI_SW_NODES_OFF': "Switching to Blender Render, Use Nodes disabled",
            'BI_SW_NODES_ON': "Switching to Blender Render, Use Nodes enabled",
            'CYC_SW_NODES_ON': "Switching back to Cycles, Use Nodes enabled",
            'CYC_SW_NODES_OFF': "Switching back to Cycles, Use Nodes disabled",
            'TEX_RENAME_F': "{}{}".format(obj_name, "no Images assigned, skipping"),
            'NO_TEX_RENAME': "No Textures in Data, nothing to rename",
            'DIR_PATH_W_ERROR': "ERROR: Directory without writing privileges",
            'DIR_PATH_N_ERROR': "ERROR: Directory not existing",
            'DIR_PATH_A_ERROR': "ERROR: Directory not accessible",
            'DIR_PATH_W_OK': "Directory has writing privileges",
            'DIR_PATH_CONVERT': "Conversion Cancelled. Problem with chosen Directory, check System Console",
            'DIR_PATH_EMPTY': "File Path is empty. Please save the .blend file first",
            'MAT_LINK_ERROR': "{}{}".format(obj_name, "not be renamed or set as Base(s)"),
            'MAT_LINK_NO_NAME': "No Base name given, No changes applied",
            'MOVE_SLOT_UP': "{}{}".format(obj_name, "been moved on top of the stack"),
            'MOVE_SLOT_DOWN': "{}{}".format(obj_name, "been moved to the bottom of the stack"),
            'MAT_TRNSP_BACK': "{}{}".format(obj_name, "been set with Alpha connected to Front/Back Geometry node"),
            'E_MAT_TRNSP_BACK': "Transparent back (BI): Failure to set the action",
            'CONV_NO_OBJ_MAT': "{}{}".format(obj_name, "has no Materials. Nothing to convert"),
            'CONV_NO_SC_MAT': "No Materials in the Scene. Nothing to convert",
            'CONV_NO_SEL_MAT': "No Materials on Selected Objects. Nothing to convert",
            }

        # doh! did we passed an non existing dict key
        warn = (warn if warn in message else 'PLACEHOLDER')

        operator.report({'INFO'}, message[warn])

        if obj_size_big is True:
            print("\n** MATERIAL SPECIALS **: \n Full list for the Info message is: \n",
                  ", ".join(object_name), "\n")

    # restore settings if overriden
    if override:
        addon.preferences.show_warnings = get_warn


def collect_report(collection="", is_start=False, is_final=False):
    # collection passes a string for appending to COLLECT_REPORT global
    # is_final swithes to the final report with the operator in __init__
    global COLLECT_REPORT
    scene = bpy.context.scene.mat_specials
    use_report = scene.enable_report

    if is_start:
        # there was a crash somewhere before the is_final call
        COLLECT_REPORT = []

    if collection and type(collection) is str:
        if use_report:
            COLLECT_REPORT.append(collection)
        print(collection)

    if is_final and use_report:
        # final operator pass uses * as delimiter for splitting into new lines
        messages = "*".join(COLLECT_REPORT)
        bpy.ops.mat_converter.reports('INVOKE_DEFAULT', message=messages)
        COLLECT_REPORT = []


def c_is_cycles_addon_enabled():
    # checks if Cycles is enabled thanks to ideasman42
    return ('cycles' in bpy.context.user_preferences.addons.keys())


def c_data_has_materials():
    # check for material presence in data
    return (len(bpy.data.materials) > 0)


def c_data_has_images():
    # check for image presence in data
    return (len(bpy.data.images) > 0)


def register():
    bpy.utils.register_module(__name__)
    pass


def unregister():
    bpy.utils.unregister_module(__name__)
    pass


if __name__ == "__main__":
    register()
