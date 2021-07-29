# gpl: author Nobuyuki Hirakata

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from . import crack_it


def check_object_cell_fracture():
    if "object_fracture_cell" in bpy.context.user_preferences.addons.keys():
        return True
    return False


# Access by bpy.ops.mesh.crackit_fracture
class FractureOperation(Operator):
    bl_idname = "mesh.crackit_fracture"
    bl_label = "Crack it!"
    bl_description = ("Make cracks using the cell fracture add-on\n"
                      "Needs only one Selected Mesh Object")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        sel_obj = len(context.selected_objects) == 1

        return (obj is not None and obj.type == "MESH" and sel_obj)

    def execute(self, context):
        if check_object_cell_fracture():
            crackit = context.scene.crackit
            try:
                crack_it.makeFracture(
                    child_verts=crackit.fracture_childverts,
                    division=crackit.fracture_div, scaleX=crackit.fracture_scalex,
                    scaleY=crackit.fracture_scaley, scaleZ=crackit.fracture_scalez,
                    margin=crackit.fracture_margin
                    )
                crack_it.addModifiers()
                crack_it.multiExtrude(
                    off=crackit.extrude_offset,
                    var2=crackit.extrude_random, var3=crackit.extrude_random
                    )
                bpy.ops.object.shade_smooth()

            except Exception as e:
                crack_it.error_handlers(
                        self, "mesh.crackit_fracture", e, "Crack It! could not be completed."
                        )
                return {"CANCELLED"}
        else:
            self.report({'WARNING'},
                        "Depends on Object: Cell Fracture addon. Please enable it first. "
                        "Operation Cancelled"
                        )
            return {"CANCELLED"}

        return {'FINISHED'}


# Apply material preset
# Access by bpy.ops.mesh.crackit_material
class MaterialOperation(Operator):
    bl_idname = "mesh.crackit_material"
    bl_label = "Apply Material"
    bl_description = ("Apply a preset material\n"
                      "The Material will be applied to the Active Object\n"
                      "from the type of Mesh, Curve, Surface, Font, Meta")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        # included - type that can have materials
        included = ['MESH', 'CURVE', 'SURFACE', 'FONT', 'META']
        return (obj is not None and obj.type in included)

    def execute(self, context):
        crackit = context.scene.crackit
        mat_name = crackit.material_preset
        mat_lib_name = crackit.material_lib_name
        mat_ui_name = crack_it.get_ui_mat_name(mat_name) if not mat_lib_name else mat_name

        try:
            crack_it.appendMaterial(
                    addon_path=crackit.material_addonpath,
                    material_name=mat_name,
                    mat_ui_names=mat_ui_name
                    )
        except Exception as e:
            crack_it.error_handlers(
                    self, "mesh.crackit_material", e,
                    "The active Object could not have the Material {} applied".format(mat_ui_name)
                    )
            return {"CANCELLED"}

        return {'FINISHED'}


# Menu settings
class crackitPanel(Panel):
    bl_label = "Crack it!"
    bl_idname = 'crack_it'
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Create"
    bl_context = 'objectmode'
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        crackit = context.scene.crackit
        layout = self.layout

        # Crack input
        box = layout.box()
        row = box.row()

        # Warning if the fracture cell addon is not enabled
        if not check_object_cell_fracture():
            col = box.column()
            col.label(text="Please enable Object: Cell Fracture addon", icon="INFO")
            col.separator()
            col.operator("wm.addon_userpref_show",
                         text="Go to Cell Fracture addon",
                         icon="PREFERENCES").module = "object_fracture_cell"

            layout.separator()
            return
        else:
            row.operator(FractureOperation.bl_idname, icon="SPLITSCREEN")

        row = box.row()
        row.prop(crackit, "fracture_childverts")

        col = box.column(align=True)
        col.prop(crackit, "fracture_scalex")
        col.prop(crackit, "fracture_scaley")
        col.prop(crackit, "fracture_scalez")

        col = box.column(align=True)
        col.label("Settings:")
        col.prop(crackit, "fracture_div")
        col.prop(crackit, "fracture_margin")

        col = box.column(align=True)
        col.label("Extrude:")
        col.prop(crackit, "extrude_offset")
        col.prop(crackit, "extrude_random")

        # material Preset:
        box = layout.box()
        row = box.row()
        row.label("Material Preset:")
        row_sub = row.row()
        row_sub.prop(crackit, "material_lib_name", text="",
                     toggle=True, icon="LONGDISPLAY")
        row = box.row()
        row.prop(crackit, "material_preset")

        row = box.row()
        row.operator(MaterialOperation.bl_idname)
