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
    "name": "Hotkey: 'Tab'",
    "description": "Switch between 3d view object/edit modes",
    "author": "pitiwazou, meta-androcto, italic",
    "version": (0, 1, 2),
    "blender": (2, 77, 0),
    "location": "3D View",
    "warning": "",
    "wiki_url": "",
    "category": "Mode Switch Pie"
}

import bpy
from bpy.types import (
    Menu,
    Operator
)


class ClassObject(Operator):
    bl_idname = "class.object"
    bl_label = "Class Object"
    bl_description = "Edit/Object Mode Switch"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode == "OBJECT":
            bpy.ops.object.mode_set(mode="EDIT")
        else:
            bpy.ops.object.mode_set(mode="OBJECT")
        return {'FINISHED'}


class ClassVertex(Operator):
    bl_idname = "class.vertex"
    bl_label = "Class Vertex"
    bl_description = "Vert Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='VERT')
        if bpy.ops.mesh.select_mode != "EDGE, FACE":
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='VERT')
            return {'FINISHED'}


class ClassEdge(Operator):
    bl_idname = "class.edge"
    bl_label = "Class Edge"
    bl_description = "Edge Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='EDGE')
        if bpy.ops.mesh.select_mode != "VERT, FACE":
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='EDGE')
            return {'FINISHED'}


class ClassFace(Operator):
    bl_idname = "class.face"
    bl_label = "Class Face"
    bl_description = "Face Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='FACE')
        if bpy.ops.mesh.select_mode != "VERT, EDGE":
            bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='FACE')
            return {'FINISHED'}


class ClassTexturePaint(Operator):
    bl_idname = "class.pietexturepaint"
    bl_label = "Class Texture Paint"
    bl_description = "Texture Paint"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode == "EDIT":
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.paint.texture_paint_toggle()
        else:
            bpy.ops.paint.texture_paint_toggle()
        return {'FINISHED'}


class ClassWeightPaint(Operator):
    bl_idname = "class.pieweightpaint"
    bl_label = "Class Weight Paint"
    bl_description = "Weight Paint"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode == "EDIT":
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.paint.weight_paint_toggle()
        else:
            bpy.ops.paint.weight_paint_toggle()
        return {'FINISHED'}


class ClassVertexPaint(Operator):
    bl_idname = "class.pievertexpaint"
    bl_label = "Class Vertex Paint"
    bl_description = "Vertex Paint"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode == "EDIT":
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.paint.vertex_paint_toggle()
        else:
            bpy.ops.paint.vertex_paint_toggle()
        return {'FINISHED'}


class ClassParticleEdit(Operator):
    bl_idname = "class.pieparticleedit"
    bl_label = "Class Particle Edit"
    bl_description = "Particle Edit (must have active particle system)"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode == "EDIT":
            bpy.ops.object.mode_set(mode="OBJECT")
            bpy.ops.particle.particle_edit_toggle()
        else:
            bpy.ops.particle.particle_edit_toggle()
        return {'FINISHED'}


# Set Mode Operator #
class SetObjectModePie(Operator):
    bl_idname = "object.set_object_mode_pie"
    bl_label = "Set the object interactive mode"
    bl_description = "I set the interactive mode of object"
    bl_options = {'REGISTER'}

    mode = bpy.props.StringProperty(name="Interactive mode", default="OBJECT")

    def execute(self, context):
        if (context.active_object):
            try:
                bpy.ops.object.mode_set(mode=self.mode)
            except TypeError:
                msg = context.active_object.name + " It is not possible to enter into the interactive mode"
                self.report(type={"WARNING"}, message=msg)
        else:
            self.report(type={"WARNING"}, message="There is no active object")
        return {'FINISHED'}


# Components Selection Mode
class VertsEdges(Operator):
    bl_idname = "verts.edges"
    bl_label = "Verts Edges"
    bl_description = "Vert/Edge Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            context.tool_settings.mesh_select_mode = (True, True, False)
        if context.object.mode == "EDIT":
            context.tool_settings.mesh_select_mode = (True, True, False)
            return {'FINISHED'}


class EdgesFaces(Operator):
    bl_idname = "edges.faces"
    bl_label = "EdgesFaces"
    bl_description = "Edge/Face Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            context.tool_settings.mesh_select_mode = (False, True, True)
        if context.object.mode == "EDIT":
            context.tool_settings.mesh_select_mode = (False, True, True)
            return {'FINISHED'}


class VertsFaces(Operator):
    bl_idname = "verts.faces"
    bl_label = "Verts Faces"
    bl_description = "Vert/Face Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            context.tool_settings.mesh_select_mode = (True, False, True)
        if context.object.mode == "EDIT":
            context.tool_settings.mesh_select_mode = (True, False, True)
            return {'FINISHED'}


class VertsEdgesFaces(Operator):
    bl_idname = "verts.edgesfaces"
    bl_label = "Verts Edges Faces"
    bl_description = "Vert/Edge/Face Select"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")
            context.tool_settings.mesh_select_mode = (True, True, True)
        if context.object.mode == "EDIT":
            context.tool_settings.mesh_select_mode = (True, True, True)
            return {'FINISHED'}


# Grease Pencil Interactive Mode
class PieInteractiveModeGreasePencil(Operator):
    bl_idname = "view3d.pie_interactive_mode_grease_pencil"
    bl_label = "Edit Strokes"
    bl_description = "Toggle Edit Strokes for Grease Pencil"

    @classmethod
    def poll(cls, context):
        return (context.gpencil_data is not None)

    def execute(self, context):
        try:
            bpy.ops.gpencil.editmode_toggle()
        except:
            self.report({'WARNING'},
                        "It is not possible to enter into the interactive mode")
        return {'FINISHED'}


# Menus
class PieObjectEditotherModes(Menu):
    """Edit/Object Others modes"""
    bl_idname = "menu.objecteditmodeothermodes"
    bl_label = "Edit Selection Modes"

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        box = pie.split().column()

        box.operator("class.vertex", text="Vertex", icon='VERTEXSEL')
        box.operator("class.edge", text="Edge", icon='EDGESEL')
        box.operator("class.face", text="Face", icon='FACESEL')
        box.operator("verts.faces", text="Vertex/Faces", icon='LOOPSEL')
        box.operator("edges.faces", text="Edges/Faces", icon='FACESEL')
        box.operator("verts.edges", text="Vertex/Edges", icon='VERTEXSEL')
        box.operator("verts.edgesfaces", text="Vertex/Edges/Faces", icon='OBJECT_DATAMODE')
        box.operator("wm.context_toggle", text="Limit to Visible",
                     icon="ORTHO").data_path = "space_data.use_occlude_geometry"


class PieObjectEditMode(Menu):
    """Modes Switch"""
    bl_idname = "pie.objecteditmode"
    bl_label = "Modes Menu (Tab)"

    def draw(self, context):
        layout = self.layout
        ob = context.object

        if ob and ob.type == 'MESH' and ob.mode in {'OBJECT', 'SCULPT', 'VERTEX_PAINT',
                                                    'WEIGHT_PAINT', 'TEXTURE_PAINT',
                                                    'PARTICLE_EDIT', 'GPENCIL_EDIT'}:
            pie = layout.menu_pie()
            # 4 - LEFT
            pie.operator("class.pievertexpaint", text="Vertex Paint", icon='VPAINT_HLT')
            # 6 - RIGHT
            pie.operator("class.pietexturepaint", text="Texture Paint", icon='TPAINT_HLT')
            # 2 - BOTTOM
            pie.menu("menu.objecteditmodeothermodes", text="Vert,Edge,Face Modes", icon='EDITMODE_HLT')
            # 8 - TOP
            pie.operator("class.object", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            # 7 - TOP - LEFT
            pie.operator("sculpt.sculptmode_toggle", text="Sculpt", icon='SCULPTMODE_HLT')
            # 9 - TOP - RIGHT
            pie.operator("class.pieweightpaint", text="Weight Paint", icon='WPAINT_HLT')
            # 1 - BOTTOM - LEFT
            if context.object.particle_systems:
                pie.operator("class.pieparticleedit", text="Particle Edit", icon='PARTICLEMODE')
            else:
                pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'MESH' and ob.mode in {'EDIT'}:
            pie = layout.menu_pie()
            # 4 - LEFT
            pie.operator("class.pievertexpaint", text="Vertex Paint", icon='VPAINT_HLT')
            # 6 - RIGHT
            pie.operator("class.pietexturepaint", text="Texture Paint", icon='TPAINT_HLT')
            # 2 - BOTTOM
            pie.menu("menu.objecteditmodeothermodes", text="Edit Modes", icon='EDITMODE_HLT')
            # 8 - TOP
            pie.operator("class.object", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            # 7 - TOP - LEFT
            pie.operator("sculpt.sculptmode_toggle", text="Sculpt", icon='SCULPTMODE_HLT')
            # 9 - TOP - RIGHT
            pie.operator("class.pieweightpaint", text="Weight Paint", icon='WPAINT_HLT')
            # 1 - BOTTOM - LEFT
            if context.object.particle_systems:
                pie.operator("class.pieparticleedit", text="Particle Edit", icon='PARTICLEMODE')
            else:
                pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'CURVE':
            pie = layout.menu_pie()
            # 4 - LEFT
            pie.separator()
            # 6 - RIGHT
            pie.separator()
            # 2 - BOTTOM
            pie.separator()
            # 8 - TOP
            pie.operator("object.editmode_toggle", text="Edit/Object", icon='OBJECT_DATAMODE')
            # 7 - TOP - LEFT
            pie.separator()
            # 9 - TOP - RIGHT
            pie.separator()
            # 1 - BOTTOM - LEFT
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'ARMATURE':
            pie = layout.menu_pie()
            # 4 - LEFT
            pie.operator(SetObjectModePie.bl_idname, text="Object", icon="OBJECT_DATAMODE").mode = "OBJECT"
            # 6 - RIGHT
            pie.operator(SetObjectModePie.bl_idname, text="Pose", icon="POSE_HLT").mode = "POSE"
            # 2 - BOTTOM
            pie.operator(SetObjectModePie.bl_idname, text="Edit", icon="EDITMODE_HLT").mode = "EDIT"
            # 8 - TOP
            pie.operator("object.editmode_toggle", text="Edit Mode", icon='OBJECT_DATAMODE')
            # 7 - TOP - LEFT
            pie.separator()
            # 9 - TOP - RIGHT
            pie.separator()
            # 1 - BOTTOM - LEFT
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'FONT':
            pie = layout.menu_pie()
            pie.separator()
            pie.separator()
            pie.separator()
            pie.operator("object.editmode_toggle", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            pie.separator()
            pie.separator()
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'SURFACE':
            pie = layout.menu_pie()
            pie.separator()
            pie.separator()
            pie.separator()
            pie.operator("object.editmode_toggle", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            pie.separator()
            pie.separator()
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'META':
            pie = layout.menu_pie()
            pie.separator()
            pie.separator()
            pie.separator()
            pie.operator("object.editmode_toggle", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            pie.separator()
            pie.separator()
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")

        elif ob and ob.type == 'LATTICE':
            pie = layout.menu_pie()
            pie.separator()
            pie.separator()
            pie.separator()
            pie.operator("object.editmode_toggle", text="Edit/Object Toggle", icon='OBJECT_DATAMODE')
            pie.separator()
            pie.separator()
            pie.separator()
            # 3 - BOTTOM - RIGHT
            if context.gpencil_data:
                pie.operator("view3d.pie_interactive_mode_grease_pencil", icon="GREASEPENCIL")
        else:
            message = "Active Object has only Object Mode available" if ob \
                    and ob.type in {"LAMP", "CAMERA", "EMPTY", "SPEAKER"} else \
                    "No active object found. Please select one first"
            pie = layout.menu_pie()
            pie.separator()
            pie.separator()
            pie.separator()
            box = pie.box()
            box.label(text=message, icon="INFO")


classes = (
    PieObjectEditMode,
    ClassObject,
    ClassVertex,
    ClassEdge,
    ClassFace,
    PieObjectEditotherModes,
    ClassTexturePaint,
    ClassWeightPaint,
    ClassVertexPaint,
    ClassParticleEdit,
    PieInteractiveModeGreasePencil,
    VertsEdges,
    EdgesFaces,
    VertsFaces,
    VertsEdgesFaces,
    SetObjectModePie,
    )

addon_keymaps = []


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    wm = bpy.context.window_manager
    if wm.keyconfigs.addon:
        # Select Mode
        km = wm.keyconfigs.addon.keymaps.new(name='Object Non-modal')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'TAB', 'PRESS')
        kmi.properties.name = "pie.objecteditmode"
        addon_keymaps.append((km, kmi))

        km = wm.keyconfigs.addon.keymaps.new(name='Grease Pencil Stroke Edit Mode')
        kmi = km.keymap_items.new('wm.call_menu_pie', 'TAB', 'PRESS')
        kmi.properties.name = "pie.objecteditmode"
        addon_keymaps.append((km, kmi))


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        for km, kmi in addon_keymaps:
            km.keymap_items.remove(kmi)
    addon_keymaps.clear()


if __name__ == "__main__":
    register()
