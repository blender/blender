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

bl_info = {
    "name": "FBX format",
    "author": "Campbell Barton, Bastien Montagne, Jens Restemeier",
    "version": (3, 7, 17),
    "blender": (2, 79, 0),
    "location": "File > Import-Export",
    "description": "FBX IO meshes, UV's, vertex colors, materials, textures, cameras, lamps and actions",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Import-Export/Autodesk_FBX",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}


if "bpy" in locals():
    import importlib
    if "import_fbx" in locals():
        importlib.reload(import_fbx)
    if "export_fbx_bin" in locals():
        importlib.reload(export_fbx_bin)
    if "export_fbx" in locals():
        importlib.reload(export_fbx)


import bpy
from bpy.props import (
        StringProperty,
        BoolProperty,
        FloatProperty,
        EnumProperty,
        )
from bpy_extras.io_utils import (
        ImportHelper,
        ExportHelper,
        orientation_helper_factory,
        path_reference_mode,
        axis_conversion,
        )


IOFBXOrientationHelper = orientation_helper_factory("IOFBXOrientationHelper", axis_forward='-Z', axis_up='Y')


class ImportFBX(bpy.types.Operator, ImportHelper, IOFBXOrientationHelper):
    """Load a FBX file"""
    bl_idname = "import_scene.fbx"
    bl_label = "Import FBX"
    bl_options = {'UNDO', 'PRESET'}

    directory = StringProperty()

    filename_ext = ".fbx"
    filter_glob = StringProperty(default="*.fbx", options={'HIDDEN'})

    ui_tab = EnumProperty(
            items=(('MAIN', "Main", "Main basic settings"),
                   ('ARMATURE', "Armatures", "Armature-related settings"),
                   ),
            name="ui_tab",
            description="Import options categories",
            )

    use_manual_orientation = BoolProperty(
            name="Manual Orientation",
            description="Specify orientation and scale, instead of using embedded data in FBX file",
            default=False,
            )
    global_scale = FloatProperty(
            name="Scale",
            min=0.001, max=1000.0,
            default=1.0,
            )
    bake_space_transform = BoolProperty(
            name="!EXPERIMENTAL! Apply Transform",
            description="Bake space transform into object data, avoids getting unwanted rotations to objects when "
                        "target space is not aligned with Blender's space "
                        "(WARNING! experimental option, use at own risks, known broken with armatures/animations)",
            default=False,
            )

    use_custom_normals = BoolProperty(
            name="Import Normals",
            description="Import custom normals, if available (otherwise Blender will recompute them)",
            default=True,
            )

    use_image_search = BoolProperty(
            name="Image Search",
            description="Search subdirs for any associated images (WARNING: may be slow)",
            default=True,
            )

    use_alpha_decals = BoolProperty(
            name="Alpha Decals",
            description="Treat materials with alpha as decals (no shadow casting)",
            default=False,
            )
    decal_offset = FloatProperty(
            name="Decal Offset",
            description="Displace geometry of alpha meshes",
            min=0.0, max=1.0,
            default=0.0,
            )

    use_anim = BoolProperty(
            name="Import Animation",
            description="Import FBX animation",
            default=True,
            )
    anim_offset = FloatProperty(
            name="Animation Offset",
            description="Offset to apply to animation during import, in frames",
            default=1.0,
            )

    use_custom_props = BoolProperty(
            name="Import User Properties",
            description="Import user properties as custom properties",
            default=True,
            )
    use_custom_props_enum_as_string = BoolProperty(
            name="Import Enums As Strings",
            description="Store enumeration values as strings",
            default=True,
            )

    ignore_leaf_bones = BoolProperty(
            name="Ignore Leaf Bones",
            description="Ignore the last bone at the end of each chain (used to mark the length of the previous bone)",
            default=False,
            )
    force_connect_children = BoolProperty(
            name="Force Connect Children",
            description="Force connection of children bones to their parent, even if their computed head/tail "
                        "positions do not match (can be useful with pure-joints-type armatures)",
            default=False,
            )
    automatic_bone_orientation = BoolProperty(
            name="Automatic Bone Orientation",
            description="Try to align the major bone axis with the bone children",
            default=False,
            )
    primary_bone_axis = EnumProperty(
            name="Primary Bone Axis",
            items=(('X', "X Axis", ""),
                   ('Y', "Y Axis", ""),
                   ('Z', "Z Axis", ""),
                   ('-X', "-X Axis", ""),
                   ('-Y', "-Y Axis", ""),
                   ('-Z', "-Z Axis", ""),
                   ),
            default='Y',
            )
    secondary_bone_axis = EnumProperty(
            name="Secondary Bone Axis",
            items=(('X', "X Axis", ""),
                   ('Y', "Y Axis", ""),
                   ('Z', "Z Axis", ""),
                   ('-X', "-X Axis", ""),
                   ('-Y', "-Y Axis", ""),
                   ('-Z', "-Z Axis", ""),
                   ),
            default='X',
            )

    use_prepost_rot = BoolProperty(
            name="Use Pre/Post Rotation",
            description="Use pre/post rotation from FBX transform (you may have to disable that in some cases)",
            default=True,
            )

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "ui_tab", expand=True)
        if self.ui_tab == 'MAIN':
            layout.prop(self, "use_manual_orientation"),
            sub = layout.column()
            sub.enabled = self.use_manual_orientation
            sub.prop(self, "axis_forward")
            sub.prop(self, "axis_up")
            layout.prop(self, "global_scale")
            layout.prop(self, "bake_space_transform")

            layout.prop(self, "use_custom_normals")

            layout.prop(self, "use_anim")
            layout.prop(self, "anim_offset")

            layout.prop(self, "use_custom_props")
            sub = layout.row()
            sub.enabled = self.use_custom_props
            sub.prop(self, "use_custom_props_enum_as_string")

            layout.prop(self, "use_image_search")
            # layout.prop(self, "use_alpha_decals")
            layout.prop(self, "decal_offset")

            layout.prop(self, "use_prepost_rot")
        elif self.ui_tab == 'ARMATURE':
            layout.prop(self, "ignore_leaf_bones")
            layout.prop(self, "force_connect_children"),
            layout.prop(self, "automatic_bone_orientation"),
            sub = layout.column()
            sub.enabled = not self.automatic_bone_orientation
            sub.prop(self, "primary_bone_axis")
            sub.prop(self, "secondary_bone_axis")

    def execute(self, context):
        keywords = self.as_keywords(ignore=("filter_glob", "directory", "ui_tab"))
        keywords["use_cycles"] = (context.scene.render.engine == 'CYCLES')

        from . import import_fbx
        return import_fbx.load(self, context, **keywords)


class ExportFBX(bpy.types.Operator, ExportHelper, IOFBXOrientationHelper):
    """Write a FBX file"""
    bl_idname = "export_scene.fbx"
    bl_label = "Export FBX"
    bl_options = {'UNDO', 'PRESET'}

    filename_ext = ".fbx"
    filter_glob = StringProperty(default="*.fbx", options={'HIDDEN'})

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    version = EnumProperty(
            items=(('BIN7400', "FBX 7.4 binary", "Modern 7.4 binary version"),
                   ('ASCII6100', "FBX 6.1 ASCII",
                                 "Legacy 6.1 ascii version - WARNING: Deprecated and no more maintained"),
                   ),
            name="Version",
            description="Choose which version of the exporter to use",
            )

    # 7.4 only
    ui_tab = EnumProperty(
            items=(('MAIN', "Main", "Main basic settings"),
                   ('GEOMETRY', "Geometries", "Geometry-related settings"),
                   ('ARMATURE', "Armatures", "Armature-related settings"),
                   ('ANIMATION', "Animation", "Animation-related settings"),
                   ),
            name="ui_tab",
            description="Export options categories",
            )

    use_selection = BoolProperty(
            name="Selected Objects",
            description="Export selected objects on visible layers",
            default=False,
            )
    global_scale = FloatProperty(
            name="Scale",
            description="Scale all data (Some importers do not support scaled armatures!)",
            min=0.001, max=1000.0,
            soft_min=0.01, soft_max=1000.0,
            default=1.0,
            )
    # 7.4 only
    apply_unit_scale = BoolProperty(
            name="Apply Unit",
            description="Take into account current Blender units settings (if unset, raw Blender Units values are used as-is)",
            default=True,
            )
    # 7.4 only
    apply_scale_options = EnumProperty(
            items=(('FBX_SCALE_NONE', "All Local",
                    "Apply custom scaling and units scaling to each object transformation, FBX scale remains at 1.0"),
                   ('FBX_SCALE_UNITS', "FBX Units Scale",
                    "Apply custom scaling to each object transformation, and units scaling to FBX scale"),
                   ('FBX_SCALE_CUSTOM', "FBX Custom Scale",
                    "Apply custom scaling to FBX scale, and units scaling to each object transformation"),
                   ('FBX_SCALE_ALL', "FBX All",
                    "Apply custom scaling and units scaling to FBX scale"),
                   ),
            name="Apply Scalings",
            description="How to apply custom and units scalings in generated FBX file "
                        "(Blender uses FBX scale to detect units on import, "
                        "but many other applications do not handle the same way)",
            )
    # 7.4 only
    bake_space_transform = BoolProperty(
            name="!EXPERIMENTAL! Apply Transform",
            description="Bake space transform into object data, avoids getting unwanted rotations to objects when "
                        "target space is not aligned with Blender's space "
                        "(WARNING! experimental option, use at own risks, known broken with armatures/animations)",
            default=False,
            )

    object_types = EnumProperty(
            name="Object Types",
            options={'ENUM_FLAG'},
            items=(('EMPTY', "Empty", ""),
                   ('CAMERA', "Camera", ""),
                   ('LAMP', "Lamp", ""),
                   ('ARMATURE', "Armature", "WARNING: not supported in dupli/group instances"),
                   ('MESH', "Mesh", ""),
                   ('OTHER', "Other", "Other geometry types, like curve, metaball, etc. (converted to meshes)"),
                   ),
            description="Which kind of object to export",
            default={'EMPTY', 'CAMERA', 'LAMP', 'ARMATURE', 'MESH', 'OTHER'},
            )

    use_mesh_modifiers = BoolProperty(
            name="Apply Modifiers",
            description="Apply modifiers to mesh objects (except Armature ones) - "
                        "WARNING: prevents exporting shape keys",
            default=True,
            )
    use_mesh_modifiers_render = BoolProperty(
            name="Use Modifiers Render Setting",
            description="Use render settings when applying modifiers to mesh objects",
            default=True,
            )
    mesh_smooth_type = EnumProperty(
            name="Smoothing",
            items=(('OFF', "Normals Only", "Export only normals instead of writing edge or face smoothing data"),
                   ('FACE', "Face", "Write face smoothing"),
                   ('EDGE', "Edge", "Write edge smoothing"),
                   ),
            description="Export smoothing information "
                        "(prefer 'Normals Only' option if your target importer understand split normals)",
            default='OFF',
            )
    use_mesh_edges = BoolProperty(
            name="Loose Edges",
            description="Export loose edges (as two-vertices polygons)",
            default=False,
            )
    # 7.4 only
    use_tspace = BoolProperty(
            name="Tangent Space",
            description="Add binormal and tangent vectors, together with normal they form the tangent space "
                        "(will only work correctly with tris/quads only meshes!)",
            default=False,
            )
    # 7.4 only
    use_custom_props = BoolProperty(
            name="Custom Properties",
            description="Export custom properties",
            default=False,
            )
    add_leaf_bones = BoolProperty(
            name="Add Leaf Bones",
            description="Append a final bone to the end of each chain to specify last bone length "
                        "(use this when you intend to edit the armature from exported data)",
            default=True # False for commit!
            )
    primary_bone_axis = EnumProperty(
            name="Primary Bone Axis",
            items=(('X', "X Axis", ""),
                   ('Y', "Y Axis", ""),
                   ('Z', "Z Axis", ""),
                   ('-X', "-X Axis", ""),
                   ('-Y', "-Y Axis", ""),
                   ('-Z', "-Z Axis", ""),
                   ),
            default='Y',
            )
    secondary_bone_axis = EnumProperty(
            name="Secondary Bone Axis",
            items=(('X', "X Axis", ""),
                   ('Y', "Y Axis", ""),
                   ('Z', "Z Axis", ""),
                   ('-X', "-X Axis", ""),
                   ('-Y', "-Y Axis", ""),
                   ('-Z', "-Z Axis", ""),
                   ),
            default='X',
            )
    use_armature_deform_only = BoolProperty(
            name="Only Deform Bones",
            description="Only write deforming bones (and non-deforming ones when they have deforming children)",
            default=False,
            )
    armature_nodetype = EnumProperty(
            name="Armature FBXNode Type",
            items=(('NULL', "Null", "'Null' FBX node, similar to Blender's Empty (default)"),
                   ('ROOT', "Root", "'Root' FBX node, supposed to be the root of chains of bones..."),
                   ('LIMBNODE', "LimbNode", "'LimbNode' FBX node, a regular joint between two bones..."),
                  ),
            description="FBX type of node (object) used to represent Blender's armatures "
                        "(use Null one unless you experience issues with other app, other choices may no import back "
                        "perfectly in Blender...)",
            default='NULL',
            )
    # Anim - 7.4
    bake_anim = BoolProperty(
            name="Baked Animation",
            description="Export baked keyframe animation",
            default=True,
            )
    bake_anim_use_all_bones = BoolProperty(
            name="Key All Bones",
            description="Force exporting at least one key of animation for all bones "
                        "(needed with some target applications, like UE4)",
            default=True,
            )
    bake_anim_use_nla_strips = BoolProperty(
            name="NLA Strips",
            description="Export each non-muted NLA strip as a separated FBX's AnimStack, if any, "
                        "instead of global scene animation",
            default=True,
            )
    bake_anim_use_all_actions = BoolProperty(
            name="All Actions",
            description="Export each action as a separated FBX's AnimStack, instead of global scene animation "
                        "(note that animated objects will get all actions compatible with them, "
                        "others will get no animation at all)",
            default=True,
            )
    bake_anim_force_startend_keying = BoolProperty(
            name="Force Start/End Keying",
            description="Always add a keyframe at start and end of actions for animated channels",
            default=True,
            )
    bake_anim_step = FloatProperty(
            name="Sampling Rate",
            description="How often to evaluate animated values (in frames)",
            min=0.01, max=100.0,
            soft_min=0.1, soft_max=10.0,
            default=1.0,
            )
    bake_anim_simplify_factor = FloatProperty(
            name="Simplify",
            description="How much to simplify baked values (0.0 to disable, the higher the more simplified)",
            min=0.0, max=100.0,  # No simplification to up to 10% of current magnitude tolerance.
            soft_min=0.0, soft_max=10.0,
            default=1.0,  # default: min slope: 0.005, max frame step: 10.
            )
    # Anim - 6.1
    use_anim = BoolProperty(
            name="Animation",
            description="Export keyframe animation",
            default=True,
            )
    use_anim_action_all = BoolProperty(
            name="All Actions",
            description=("Export all actions for armatures or just the currently selected action"),
            default=True,
            )
    use_default_take = BoolProperty(
            name="Default Take",
            description="Export currently assigned object and armature animations into a default take from the scene "
                        "start/end frames",
            default=True
            )
    use_anim_optimize = BoolProperty(
            name="Optimize Keyframes",
            description="Remove double keyframes",
            default=True,
            )
    anim_optimize_precision = FloatProperty(
            name="Precision",
            description="Tolerance for comparing double keyframes (higher for greater accuracy)",
            min=0.0, max=20.0,  # from 10^2 to 10^-18 frames precision.
            soft_min=1.0, soft_max=16.0,
            default=6.0,  # default: 10^-4 frames.
            )
    # End anim
    path_mode = path_reference_mode
    # 7.4 only
    embed_textures = BoolProperty(
            name="Embed Textures",
            description="Embed textures in FBX binary file (only for \"Copy\" path mode!)",
            default=False,
            )
    batch_mode = EnumProperty(
            name="Batch Mode",
            items=(('OFF', "Off", "Active scene to file"),
                   ('SCENE', "Scene", "Each scene as a file"),
                   ('GROUP', "Group", "Each group as a file"),
                   ),
            )
    use_batch_own_dir = BoolProperty(
            name="Batch Own Dir",
            description="Create a dir for each exported file",
            default=True,
            )
    use_metadata = BoolProperty(
            name="Use Metadata",
            default=True,
            options={'HIDDEN'},
            )

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "version")

        if self.version == 'BIN7400':
            layout.prop(self, "ui_tab", expand=True)
            if self.ui_tab == 'MAIN':
                layout.prop(self, "use_selection")

                col = layout.column(align=True)
                row = col.row(align=True)
                row.prop(self, "global_scale")
                sub = row.row(align=True)
                sub.prop(self, "apply_unit_scale", text="", icon='NDOF_TRANS')
                col.prop(self, "apply_scale_options")

                layout.prop(self, "axis_forward")
                layout.prop(self, "axis_up")

                layout.separator()
                layout.prop(self, "object_types")
                layout.prop(self, "bake_space_transform")
                layout.prop(self, "use_custom_props")

                layout.separator()
                row = layout.row(align=True)
                row.prop(self, "path_mode")
                sub = row.row(align=True)
                sub.enabled = (self.path_mode == 'COPY')
                sub.prop(self, "embed_textures", text="", icon='PACKAGE' if self.embed_textures else 'UGLYPACKAGE')
                row = layout.row(align=True)
                row.prop(self, "batch_mode")
                sub = row.row(align=True)
                sub.prop(self, "use_batch_own_dir", text="", icon='NEWFOLDER')
            elif self.ui_tab == 'GEOMETRY':
                layout.prop(self, "use_mesh_modifiers")
                sub = layout.row()
                sub.enabled = self.use_mesh_modifiers
                sub.prop(self, "use_mesh_modifiers_render")
                layout.prop(self, "mesh_smooth_type")
                layout.prop(self, "use_mesh_edges")
                sub = layout.row()
                #~ sub.enabled = self.mesh_smooth_type in {'OFF'}
                sub.prop(self, "use_tspace")
            elif self.ui_tab == 'ARMATURE':
                layout.prop(self, "use_armature_deform_only")
                layout.prop(self, "add_leaf_bones")
                layout.prop(self, "primary_bone_axis")
                layout.prop(self, "secondary_bone_axis")
                layout.prop(self, "armature_nodetype")
            elif self.ui_tab == 'ANIMATION':
                layout.prop(self, "bake_anim")
                col = layout.column()
                col.enabled = self.bake_anim
                col.prop(self, "bake_anim_use_all_bones")
                col.prop(self, "bake_anim_use_nla_strips")
                col.prop(self, "bake_anim_use_all_actions")
                col.prop(self, "bake_anim_force_startend_keying")
                col.prop(self, "bake_anim_step")
                col.prop(self, "bake_anim_simplify_factor")
        else:
            layout.prop(self, "use_selection")
            layout.prop(self, "global_scale")
            layout.prop(self, "axis_forward")
            layout.prop(self, "axis_up")

            layout.separator()
            layout.prop(self, "object_types")
            layout.prop(self, "use_mesh_modifiers")
            layout.prop(self, "mesh_smooth_type")
            layout.prop(self, "use_mesh_edges")
            sub = layout.row()
            #~ sub.enabled = self.mesh_smooth_type in {'OFF'}
            sub.prop(self, "use_tspace")
            layout.prop(self, "use_armature_deform_only")
            layout.prop(self, "use_anim")
            col = layout.column()
            col.enabled = self.use_anim
            col.prop(self, "use_anim_action_all")
            col.prop(self, "use_default_take")
            col.prop(self, "use_anim_optimize")
            col.prop(self, "anim_optimize_precision")

            layout.separator()
            layout.prop(self, "path_mode")

            layout.prop(self, "batch_mode")
            layout.prop(self, "use_batch_own_dir")

    @property
    def check_extension(self):
        return self.batch_mode == 'OFF'

    def execute(self, context):
        from mathutils import Matrix
        if not self.filepath:
            raise Exception("filepath not set")

        global_matrix = (axis_conversion(to_forward=self.axis_forward,
                                         to_up=self.axis_up,
                                         ).to_4x4())

        keywords = self.as_keywords(ignore=("check_existing",
                                            "filter_glob",
                                            "ui_tab",
                                            ))

        keywords["global_matrix"] = global_matrix

        if self.version == 'BIN7400':
            from . import export_fbx_bin
            return export_fbx_bin.save(self, context, **keywords)
        else:
            from . import export_fbx
            return export_fbx.save(self, context, **keywords)


def menu_func_import(self, context):
    self.layout.operator(ImportFBX.bl_idname, text="FBX (.fbx)")


def menu_func_export(self, context):
    self.layout.operator(ExportFBX.bl_idname, text="FBX (.fbx)")


classes = (
    ImportFBX,
    ExportFBX,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
