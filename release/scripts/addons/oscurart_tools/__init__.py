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
    "name": "Oscurart Tools",
    "author": "Oscurart, CodemanX",
    "version": (3, 5, 2),
    "blender": (2, 77, 0),
    "location": "View3D > Tools > Oscurart Tools",
    "description": "Tools for objects, render, shapes, and files.",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/Oscurart_Tools",
    "category": "Object",
    }

if "bpy" in locals():
    import importlib
    importlib.reload(oscurart_files)
    importlib.reload(oscurart_meshes)
    importlib.reload(oscurart_objects)
    importlib.reload(oscurart_shapes)
    importlib.reload(oscurart_render)
    importlib.reload(oscurart_overrides)
    importlib.reload(oscurart_animation)

else:
    from . import oscurart_files
    from . import oscurart_meshes
    from . import oscurart_objects
    from . import oscurart_shapes
    from . import oscurart_render
    from . import oscurart_overrides
    from . import oscurart_animation

import bpy

from bpy.types import (
        AddonPreferences,
        Panel,
        PropertyGroup,
        )
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        PointerProperty,
        CollectionProperty,
        )


class View3DOscPanel(PropertyGroup):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'

    # Create Panels and Tools
    osc_object_tools = BoolProperty(default=False)
    osc_mesh_tools = BoolProperty(default=False)
    osc_shapes_tools = BoolProperty(default=False)
    osc_render_tools = BoolProperty(default=False)
    osc_files_tools = BoolProperty(default=False)
    osc_overrides_tools = BoolProperty(default=False)
    osc_animation_tools = BoolProperty(default=False)

    # For new Scenes
    overrides = StringProperty(default="[]")


class OscOverridesProp(PropertyGroup):
    matoverride = StringProperty()
    grooverride = StringProperty()


# Panels
class OscPanelControl(Panel):
    bl_idname = "Oscurart Panel Control"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Oscurart Tools"
    bl_label = "Oscurart Tools"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        oscurart = scene.oscurart

        col = layout.column(align=1)
        col.prop(oscurart, "osc_object_tools", text="Object", icon="OBJECT_DATAMODE")
        col.prop(oscurart, "osc_mesh_tools", text="Mesh", icon="EDITMODE_HLT")
        col.prop(oscurart, "osc_shapes_tools", text="Shapes", icon="SHAPEKEY_DATA")
        col.prop(oscurart, "osc_animation_tools", text="Animation", icon="POSE_DATA")
        col.prop(oscurart, "osc_render_tools", text="Render", icon="SCENE")
        col.prop(oscurart, "osc_files_tools", text="Files", icon="IMASEL")
        col.prop(oscurart, "osc_overrides_tools", text="Overrides", icon="GREASEPENCIL")


class OscPanelObject(Panel):
    bl_idname = "Oscurart Object Tools"
    bl_label = "Object Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_object_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)

        colrow = col.row(align=1)
        colrow.operator("object.relink_objects_between_scenes", icon="LINKED")
        colrow = col.row(align=1)
        colrow.operator("object.copy_objects_groups_layers", icon="LINKED")
        colrow.operator("object.set_layers_to_other_scenes", icon="LINKED")
        colrow = col.row(align=1)
        colrow.operator("object.objects_to_groups", icon="GROUP")
        colrow = col.row(align=1)
        colrow.prop(bpy.context.scene, "SearchAndSelectOt", text="")
        colrow.operator("object.search_and_select_osc", icon="ZOOM_SELECTED")
        colrow = col.row(align=1)
        colrow.prop(bpy.context.scene, "RenameObjectOt", text="")
        colrow.operator("object.rename_objects_osc", icon="SHORTDISPLAY")
        col.operator(
            "object.distribute_osc",
            icon="OBJECT_DATAMODE",
            text="Distribute")
        col.operator(
            "object.duplicate_object_symmetry_osc",
            icon="OUTLINER_OB_EMPTY",
            text="Duplicate Sym")
        colrow = col.row(align=1)
        colrow.operator(
            "object.modifiers_remove_osc",
            icon="MODIFIER",
            text="Remove Modifiers")
        colrow.operator(
            "object.modifiers_apply_osc",
            icon="MODIFIER",
            text="Apply Modifiers")
        colrow = col.row(align=1)
        colrow.operator(
            "group.group_in_out_camera",
            icon="RENDER_REGION",
            text="Make Groups in out Camera")


class OscPanelMesh(Panel):
    bl_idname = "Oscurart Mesh Tools"
    bl_label = "Mesh Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_mesh_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)

        col.operator("mesh.object_to_mesh_osc", icon="MESH_MONKEY")
        col.operator("mesh.select_side_osc", icon="VERTEXSEL")
        col.operator("mesh.select_doubles", icon="VERTEXSEL")
        colrow = col.row(align=1)
        colrow.operator("mesh.resym_save_map", icon="UV_SYNC_SELECT")
        colrow = col.row(align=1)
        colrow.operator(
            "mesh.resym_mesh",
            icon="UV_SYNC_SELECT",
            text="Resym Mesh")
        colrow.operator("mesh.resym_vertex_weights_osc", icon="UV_SYNC_SELECT")
        colrow = col.row(align=1)
        colrow.operator("mesh.reconst_osc", icon="UV_SYNC_SELECT")
        colrow = col.row(align=1)
        colrow.operator("mesh.overlap_uv_faces", icon="UV_FACESEL")
        colrow = col.row(align=1)
        colrow.operator("view3d.modal_operator", icon="STICKY_UVS_DISABLE")
        colrow = col.row(align=1)
        colrow.operator("lattice.mirror_selected", icon="LATTICE_DATA")


class OscPanelShapes(Panel):
    bl_idname = "Oscurart Shapes Tools"
    bl_label = "Shapes Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_shapes_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)

        col.operator("object.shape_key_to_objects_osc", icon="OBJECT_DATAMODE")
        col.operator("mesh.create_lmr_groups_osc", icon="GROUP_VERTEX")
        col.operator("mesh.split_lr_shapes_osc", icon="SHAPEKEY_DATA")
        colrow = col.row(align=1)
        colrow.operator("mesh.create_symmetrical_layout_osc", icon="SETTINGS")
        colrow.operator("mesh.create_asymmetrical_layout_osc", icon="SETTINGS")


class OscPanelRender(Panel):
    bl_idname = "Oscurart Render Tools"
    bl_label = "Render Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_render_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)

        colrow = col.row(align=1)
        col.operator(
            "file.create_batch_maker_osc",
            icon="LINENUMBERS_ON",
            text="Make Render Batch")
        col.operator(
            "file.create_batch_python",
            icon="LINENUMBERS_ON",
            text="Make Python Batch")
        colrow = col.row(align=1)
        colrow.operator(
            "render.render_all_scenes_osc",
            icon="RENDER_STILL",
            text="All Scenes").frametype = False
        colrow.operator(
            "render.render_all_scenes_osc",
            text="> Frame").frametype = True
        colrow = col.row(align=1)
        colrow.operator(
            "render.render_current_scene_osc",
            icon="RENDER_STILL",
            text="Active Scene").frametype = False
        colrow.operator(
            "render.render_current_scene_osc",
            text="> Frame").frametype = True

        colrow = col.row(align=1)
        colrow.operator("render.render_crop_osc", icon="RENDER_REGION")
        colrow.prop(bpy.context.scene, "rcPARTS", text="Parts")

        boxcol = layout.box().column(align=1)
        colrow = boxcol.row(align=1)
        colrow.operator(
            "render.render_selected_scenes_osc",
            icon="RENDER_STILL",
            text="Selected Scenes").frametype = False
        colrow.operator(
            "render.render_selected_scenes_osc",
            text="> Fame").frametype = True

        for sc in bpy.data.scenes[:]:
            boxcol.prop(sc, "use_render_scene", text=sc.name)


class OscPanelFiles(Panel):
    bl_idname = "Oscurart Files Tools"
    bl_label = "Files Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_files_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)
        col.operator("file.save_incremental_osc", icon="NEW")
        col.operator("image.reload_images_osc", icon="IMAGE_COL")
        col.operator("file.collect_all_images", icon="IMAGE_COL")
        col.operator("file.sync_missing_groups", icon="LINK_AREA")
        col = layout.column(align=1)
        colrow = col.row(align=1)
        colrow.prop(bpy.context.scene, "oscSearchText", text="")
        colrow.prop(bpy.context.scene, "oscReplaceText", text="")
        col.operator("file.replace_file_path_osc", icon="SHORTDISPLAY")


class OscPanelOverrides(Panel):
    bl_idname = "Oscurart Overrides"
    bl_label = "Overrides Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_overrides_tools

    def draw(self, context):
        layout = self.layout
        box = layout.box()
        col = box.column(align=1)

        # col.operator("render.overrides_set_list", text="Create Override
        # List", icon="GREASEPENCIL")
        col.label(text="Active Scene: " + bpy.context.scene.name)
        col.label(text="Example: [[Group,Material]]")
        col.prop(bpy.context.scene.oscurart, "overrides", text="")
        col.operator(
            "render.check_overrides",
            text="Check List",
            icon="ZOOM_ALL")
        col.operator("render.overrides_on", text="On / Off", icon="QUIT")
        col.label(
            text=str("OVERRIDES: ON" if bpy.use_overrides else "OVERRIDES: OFF"))

        box = layout.box()
        boxcol = box.column(align=1)
        boxcol.label(text="Danger Zone")
        boxcolrow = boxcol.row(align=1)
        boxcolrow.operator(
            "render.apply_overrides",
            text="Apply Overrides",
            icon="ERROR")
        boxcolrow.operator(
            "render.restore_overrides",
            text="Restore Overrides",
            icon="ERROR")


class OscPanelAnimation(Panel):
    bl_idname = "Oscurart Animation Tools"
    bl_label = "Animation Tools"
    bl_category = "Oscurart Tools"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.scene.oscurart.osc_animation_tools

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=1)
        row = col.row()

        col.operator("anim.quick_parent_osc", icon="OUTLINER_DATA_POSE")
        row = col.row(align=1)
        row.prop(bpy.context.scene, "quick_animation_in", text="")
        row.prop(bpy.context.scene, "quick_animation_out", text="")


# Addons Preferences Update Panel

# Define Panel classes for updating
panels = (
        OscPanelControl,
        OscPanelObject,
        OscPanelMesh,
        OscPanelShapes,
        OscPanelRender,
        OscPanelFiles,
        OscPanelOverrides,
        OscPanelAnimation,
        )


def update_panel(self, context):
    message = "Oscurart Tools: Updating Panel locations has failed"
    try:
        for panel in panels:
            if "bl_rna" in panel.__dict__:
                bpy.utils.unregister_class(panel)

        for panel in panels:
            panel.bl_category = context.user_preferences.addons[__name__].preferences.category
            bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class OscurartToolsAddonPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Category",
            description="Choose a name for the category of the panel",
            default="Tools",
            update=update_panel,
            )

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        col = row.column()
        col.label(text="Category:")
        col.prop(self, "category", text="")

# ========================= End of Scripts =========================


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.oscurart = PointerProperty(type=View3DOscPanel)

    bpy.types.Scene.ovlist = CollectionProperty(type=OscOverridesProp)

    bpy.types.Scene.quick_animation_in = IntProperty(default=1)
    bpy.types.Scene.quick_animation_out = IntProperty(default=250)

    # SETEO VARIABLE DE ENTORNO
    bpy.types.Scene.SearchAndSelectOt = StringProperty(
                                            default="Object name initials"
                                            )
    update_panel(None, bpy.context)


def unregister():
    del bpy.types.Scene.oscurart
    del bpy.types.Scene.quick_animation_in
    del bpy.types.Scene.quick_animation_out
    del bpy.types.Scene.SearchAndSelectOt

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
