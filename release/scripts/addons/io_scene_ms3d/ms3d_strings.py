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

###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------


# ##### BEGIN COPYRIGHT BLOCK #####
#
# initial script copyright (c)2011-2013 Alexander Nussbaumer
#
# ##### END COPYRIGHT BLOCK #####
SEE_MS3D_DOC = "see MilkShape 3D documentation"

ms3d_str = {
        'lang': "en-US",
        'RUNTIME_KEY': "Human friendly presentation",

        ###############################
        # blender key names
        'OBJECT_LAYER_EXTRA': "ms3d_extra_layer",
        'OBJECT_LAYER_GROUP': "ms3d_group_layer",
        'OBJECT_LAYER_SMOOTHING_GROUP': "ms3d_smoothing_group_layer",
        'OBJECT_MODIFIER_SMOOTHING_GROUP': "ms3d_smoothing_groups",
        # for some reason after bm.to_mesh(..)
        # the names of 'bm.loops.layers.uv' becomes to 'bm.faces.layers.tex'
        # to bypass this issue, i give both the same name.
        # 'OBJECT_LAYER_TEXTURE': "ms3d_texture_layer",
        'OBJECT_LAYER_TEXTURE': "ms3d_uv_layer",
        'OBJECT_LAYER_UV': "ms3d_uv_layer",

        ###############################
        # strings to be used with 'str().format()'
        'STRING_FORMAT_GROUP': "Group.{:03d}",
        'WARNING_IMPORT_SKIP_FACE_DOUBLE': "skipped face #{}:"\
                " contains double faces with same vertices!",
        'WARNING_IMPORT_SKIP_LESS_VERTICES': "skipped face #{}:"\
                " contains faces too less vertices!",
        'WARNING_IMPORT_SKIP_VERTEX_DOUBLE': "skipped face #{}:"\
                " contains faces with double vertices!",
        'WARNING_IMPORT_EXTRA_VERTEX_NORMAL': "created extra vertex"\
                " because of different normals #{} -> {}.",
        'SUMMARY_IMPORT': "elapsed time: {0:.4}s (media io:"\
                " ~{1:.4}s, converter: ~{2:.4}s)",
        'SUMMARY_EXPORT': "elapsed time: {0:.4}s (converter:"\
                " ~{1:.4}s, media io: ~{2:.4}s)",
        'WARNING_EXPORT_SKIP_WEIGHT' : "skipped weight (ms3d can handle 3 weighs max.,"\
                " the less weighty weight was skipped)",
        'WARNING_EXPORT_SKIP_WEIGHT_EX' : "skipped weight:"\
                " limit exceeded (ms3d can handle 3 weighs max., the less weighty"\
                " weights were skipped)",

        ###############################
        'TEXT_OPERATOR': "MilkShape 3D (.ms3d)",
        'FILE_EXT': ".ms3d",
        'FILE_FILTER': "*.ms3d",
        'BL_DESCRIPTION_EXPORTER': "Export to a MilkShape 3D file format (.ms3d)",
        'BL_DESCRIPTION_IMPORTER': "Import from a MilkShape 3D file format (.ms3d)",
        'BL_LABEL_EXPORTER': "Export MS3D",
        'BL_LABEL_GROUP_OPERATOR': "MS3D - Group Collection Operator",
        'BL_LABEL_IMPORTER': "Import MS3D",
        'BL_LABEL_PANEL_SMOOTHING_GROUP': "MS3D - Smoothing Group",
        'BL_LABEL_SMOOTHING_GROUP_OPERATOR': "MS3D Set Smoothing Group"\
                " Operator",
        'BL_LABEL_MATERIAL_OPERATOR' : "MS3D - Copy Material Operator",
        'ENUM_ADD_GROUP_1': "Add",
        'ENUM_ADD_GROUP_2': "adds an item",
        'ENUM_ASSIGN_1': "Assign",
        'ENUM_ASSIGN_2_GROUP': "assign selected faces to selected group",
        'ENUM_ASSIGN_2_SMOOTHING_GROUP': "assign all selected faces to"\
                " selected smoothing group",
        'ENUM_DESELECT_1': "Deselect",
        'ENUM_DESELECT_2_GROUP': "deselects faces of selected group",
        'ENUM_DESELECT_2_SMOOTHING_GROUP': "deselects all faces of selected"\
                " smoothing group",
        'ENUM_FLAG_DIRTY_1': "Dirty",
        'ENUM_FLAG_DIRTY_2': SEE_MS3D_DOC,
        'ENUM_FLAG_HIDDEN_1': "Hidden",
        'ENUM_FLAG_HIDDEN_2': SEE_MS3D_DOC,
        'ENUM_FLAG_ISKEY_1': "Is Key",
        'ENUM_FLAG_ISKEY_2': SEE_MS3D_DOC,
        'ENUM_FLAG_MARKED_1': "Marked",
        'ENUM_FLAG_MARKED_2': SEE_MS3D_DOC,
        'ENUM_FLAG_NEWLYCREATED_1': "Newly Created",
        'ENUM_FLAG_NEWLYCREATED_2': SEE_MS3D_DOC,
        'ENUM_FLAG_NONE_1': "None",
        'ENUM_FLAG_NONE_2': SEE_MS3D_DOC,
        'ENUM_FLAG_SELECTED_1': "Selected",
        'ENUM_FLAG_SELECTED_2': SEE_MS3D_DOC,
        'ENUM_FLAG_SELECTED2_1': "Selected Ex.",
        'ENUM_FLAG_SELECTED2_2': SEE_MS3D_DOC,
        'ENUM_REMOVE_1': "Remove",
        'ENUM_REMOVE_2_GROUP': "remove selected faces from selected group",
        'ENUM_REMOVE_GROUP_1': "Remove",
        'ENUM_REMOVE_GROUP_2': "removes an item",
        'ENUM_SELECT_1': "Select",
        'ENUM_SELECT_2_GROUP': "selects faces of selected group",
        'ENUM_SELECT_2_SMOOTHING_GROUP': "selects all faces of selected"\
                " smoothing group",
        'LABEL_NAME_ANIMATION': "Animation Processing:",
        'LABEL_NAME_OPTIONS': "Advanced Options:",
        'LABEL_NAME_PROCESSING': "Object Processing:",
        'LABEL_NAME_MODIFIER': "Modifier Processing:",
        'LABEL_PANEL_BUTTON_NONE': "None",
        'LABEL_PANEL_GROUPS': "MS3D - Groups",
        'LABEL_PANEL_JOINTS': "MS3D - Joint",
        'LABEL_PANEL_MATERIALS': "MS3D - Material",
        'LABEL_PANEL_MODEL': "MS3D - Model",
        'PROP_DESC_ALPHA_REF': "ms3d internal raw 'alpha_ref' of Model",
        'PROP_DESC_ALPHAMAP': "ms3d internal raw 'alphamap' file name of"\
                " Material",
        'PROP_DESC_AMBIENT': "ms3d internal raw 'ambient' of Material",
        'PROP_DESC_USE_ANIMATION': "keyframes (rotations, positions)",
        'PROP_DESC_COLOR_JOINT': "ms3d internal raw 'color' of Joint",
        'PROP_DESC_COMMENT_GROUP': "ms3d internal raw 'comment' of Group",
        'PROP_DESC_COMMENT_JOINT': "ms3d internal raw 'comment' of Joint",
        'PROP_DESC_COMMENT_MATERIAL': "ms3d internal raw 'comment' of Material",
        'PROP_DESC_COMMENT_MODEL': "ms3d internal raw 'comment' of Model",
        'PROP_DESC_DIFFUSE': "ms3d internal raw 'diffuse' of Material",
        'PROP_DESC_EMISSIVE': "ms3d internal raw 'emissive' of Material",
        'PROP_DESC_FLAGS_GROUP': "ms3d internal raw 'flags' of Group",
        'PROP_DESC_FLAGS_JOINT': "ms3d internal raw 'flags' of Joint",
        'PROP_DESC_GROUP_NAME': "ms3d internal raw 'name' of Group",
        'PROP_DESC_JOINT_SIZE': "ms3d internal raw 'joint_size' of Model",
        'PROP_DESC_MODE_TEXTURE': "ms3d internal raw 'mode' of Material",
        'PROP_DESC_NAME_ARMATURE': "ms3d internal raw 'name' of Model (not used"\
                " for export)",
        'PROP_DESC_NAME_JOINT': "ms3d internal raw 'name' of Joint",
        'PROP_DESC_NAME_MATERIAL': "ms3d internal raw 'name' of Material",
        'PROP_DESC_NAME_MODEL': "ms3d internal raw 'name' of Model (not used for export)",
        'PROP_DESC_SHININESS': "ms3d internal raw 'shininess' of Material",
        'PROP_DESC_SPECULAR': "ms3d internal raw 'specular' of Material",
        'PROP_DESC_TEXTURE': "ms3d internal raw 'texture' file name of"\
                " Material",
        'PROP_DESC_TRANSPARENCY': "ms3d internal raw 'transparency' of"\
                " Material",
        'PROP_DESC_TRANSPARENCY_MODE': "ms3d internal raw 'transparency_mode'"\
                " of Model",
        'PROP_DESC_VERBOSE': "Run the converter in debug mode."\
                " Check the console for output (Warning, may be very slow)",
        'PROP_FLAG_TEXTURE_COMBINE_ALPHA_1': "Combine Alpha",
        'PROP_FLAG_TEXTURE_COMBINE_ALPHA_2': SEE_MS3D_DOC,
        'PROP_FLAG_TEXTURE_HAS_ALPHA_1': "Has Alpha",
        'PROP_FLAG_TEXTURE_HAS_ALPHA_2': SEE_MS3D_DOC,
        'PROP_FLAG_TEXTURE_SPHERE_MAP_1': "Sphere Map",
        'PROP_FLAG_TEXTURE_SPHERE_MAP_2': SEE_MS3D_DOC,
        'PROP_MODE_TRANSPARENCY_DEPTH_BUFFERED_WITH_ALPHA_REF_1': "Depth"\
                " Buffered with Alpha Ref",
        'PROP_MODE_TRANSPARENCY_DEPTH_BUFFERED_WITH_ALPHA_REF_2': SEE_MS3D_DOC,
        'PROP_MODE_TRANSPARENCY_DEPTH_SORTED_TRIANGLES_1': "Depth Sorted"\
                " Triangles",
        'PROP_MODE_TRANSPARENCY_DEPTH_SORTED_TRIANGLES_2': SEE_MS3D_DOC,
        'PROP_MODE_TRANSPARENCY_SIMPLE_1': "Simple",
        'PROP_MODE_TRANSPARENCY_SIMPLE_2': SEE_MS3D_DOC,
        'PROP_NAME_ALPHA_REF': "Alpha Ref.",
        'PROP_NAME_ALPHAMAP': "Alphamap",
        'PROP_NAME_AMBIENT': "Ambient",
        'PROP_NAME_USE_ANIMATION': "Animation",
        'PROP_NAME_COLOR': "Color",
        'PROP_NAME_COMMENT': "Comment",
        'PROP_NAME_DIFFUSE': "Diffuse",
        'PROP_NAME_EMISSIVE': "Emissive",
        'PROP_NAME_FLAGS': "Flags",
        'PROP_NAME_JOINT_SIZE': "Joint Size",
        'PROP_NAME_MODE': "Mode",
        'PROP_NAME_NAME': "Name",
        'PROP_NAME_ACTIVE': "Active Mesh:",
        'PROP_NAME_SHININESS': "Shininess",
        'PROP_NAME_SPECULAR': "Specular",
        'PROP_NAME_TEXTURE': "Texture",
        'PROP_NAME_TRANSPARENCY': "Transparency",
        'PROP_NAME_TRANSPARENCY_MODE': "Transp. Mode",
        'PROP_NAME_VERBOSE': "Verbose",
        'ENUM_VERBOSE_NONE_1': "None",
        'ENUM_VERBOSE_NONE_2': "",
        'ENUM_VERBOSE_NORMAL_1': "Normal",
        'ENUM_VERBOSE_NORMAL_2': "",
        'ENUM_VERBOSE_MAXIMALIMAL_1': "Maximal",
        'ENUM_VERBOSE_MAXIMALIMAL_2': "",
        'PROP_SMOOTHING_GROUP_INDEX': "Smoothing group id",
        'PROP_NAME_ROTATION_MODE' : "Bone Rotation Mode",
        'PROP_DESC_ROTATION_MODE' : "set the preferred rotation mode of bones",
        'PROP_ITEM_ROTATION_MODE_EULER_1' : "Euler",
        'PROP_ITEM_ROTATION_MODE_EULER_2' : "use Euler bone rotation"\
                " (gimbal-lock can be fixed by using "\
                "'Graph Editor -> Key -> Discontinuity (Euler) Filter')",
        'PROP_ITEM_ROTATION_MODE_QUATERNION_1' : "Quaternion",
        'PROP_ITEM_ROTATION_MODE_QUATERNION_2' : "use quaternion bone rotation"\
                " (no gimbal-lock filter available!)",
        'PROP_NAME_USE_JOINT_SIZE': "Override Joint Size",
        'PROP_DESC_USE_JOINT_SIZE': "use value of 'Joint Size', the value of the"\
                " ms3d file is ignored for representation",
        'PROP_NAME_IMPORT_JOINT_SIZE': "Joint Size",
        'PROP_DESC_IMPORT_JOINT_SIZE': "size of the joint representation in"\
                " blender",
        'PROP_NAME_NORMALIZE_WEIGHTS' : "Normalize Weights",
        'PROP_DESC_NORMALIZE_WEIGHTS' : "normalize all weights to 100%,",
        'PROP_NAME_SHRINK_TO_KEYS' : "Shrink To Keys",
        'PROP_DESC_SHRINK_TO_KEYS' : "shrinks the animation to region from"\
                " first keyframe to last keyframe",
        'PROP_NAME_BAKE_EACH_FRAME' : "Bake Each Frame As Key",
        'PROP_DESC_BAKE_EACH_FRAME' : "if enabled, to each frame there will be"\
                " a key baked",
        'LABEL_NAME_JOINT_TO_BONES' : "use only for bones created in blender",
        'PROP_NAME_JOINT_TO_BONES' : "Joints To Bones",
        'PROP_DESC_JOINT_TO_BONES' : "changes the length of the bones",
        'PROP_NAME_USE_BLENDER_NAMES' : "Use Blender Names Only",
        'PROP_DESC_USE_BLENDER_NAMES' : "use only blender names, ignores ms3d"\
                " names (bone names will always be taken from blender)",
        'PROP_NAME_USE_BLENDER_MATERIALS' : "Use Blender Materials",
        'PROP_DESC_USE_BLENDER_MATERIALS' : "ignores ms3d material definition"\
                " (you loose some information by choosing this option)",
        'ENUM_FROM_BLENDER_1' : "Copy From Blender",
        'ENUM_FROM_BLENDER_2' : "takes and copies all available values from"\
                " blender",
        'ENUM_TO_BLENDER_1' : "Copy To Blender",
        'ENUM_TO_BLENDER_2' : "copies and puts all available values to blender",
        'PROP_NAME_EXTENDED_NORMAL_HANDLING': "Extended Normal Handling",
        'PROP_DESC_EXTENDED_NORMAL_HANDLING': "adds extra vertices if normals"\
                " are different",
        'PROP_NAME_APPLY_TRANSFORM': "Apply Transform",
        'PROP_DESC_APPLY_TRANSFORM': "applies location, rotation and scale on"\
                " export",
        'PROP_NAME_APPLY_MODIFIERS': "Apply Modifiers",
        'PROP_DESC_APPLY_MODIFIERS': "applies modifiers on export that are"\
                " enabled (except of armature modifiers)",
        'PROP_NAME_APPLY_MODIFIERS_MODE': "Apply Mode",
        'PROP_DESC_APPLY_MODIFIERS_MODE': "apply modifier, if enabled in its"\
                " mode",
        'PROP_ITEM_APPLY_MODIFIERS_MODE_VIEW_1': "View",
        'PROP_ITEM_APPLY_MODIFIERS_MODE_VIEW_2': "apply modifiers that are"\
                " enabled in viewport",
        'PROP_ITEM_APPLY_MODIFIERS_MODE_RENDER_1': "Render",
        'PROP_ITEM_APPLY_MODIFIERS_MODE_RENDER_2': "apply modifiers that are"\
                " enabled in renderer",

        'PROP_NAME_': "Name",
        'PROP_DESC_': "Description",
        # ms3d_str['']
        }


###############################################################################
#234567890123456789012345678901234567890123456789012345678901234567890123456789
#--------1---------2---------3---------4---------5---------6---------7---------
# ##### END OF FILE #####
