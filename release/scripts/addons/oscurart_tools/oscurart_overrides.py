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

import bpy
from bpy.types import Operator, Panel
import os


# ------------------APPLY AND RESTORE OVERRIDES ------------------

def DefOscApplyOverrides(self):
    types = {'MESH', 'META', 'CURVE'}
    for ob in bpy.data.objects:
        if ob.type in types:
            if not len(ob.material_slots):
                ob.data.materials.append(None)
    slotlist = {ob: [sl.material for sl in ob.material_slots]
                for ob in bpy.data.objects if ob.type in types if len(ob.material_slots)}
    with open("%s_override.txt" % (os.path.join(os.path.dirname(bpy.data.filepath),
                                                bpy.context.scene.name)), mode="w") as file:
        file.write(str(slotlist))
    scene = bpy.context.scene
    proptolist = list(eval(scene.oscurart.overrides))
    for group, material in proptolist:
        for object in bpy.data.groups[group].objects:
            lenslots = len(object.material_slots)
            if object.type in types:
                if len(object.data.materials):
                    object.data.materials.clear()
                    for newslot in range(lenslots):
                        object.data.materials.append(
                            bpy.data.materials[material])


def DefOscRestoreOverrides(self):
    with open("%s_override.txt" % (os.path.join(os.path.dirname(bpy.data.filepath),
                                                bpy.context.scene.name)), mode="r") as file:
        slotlist = eval(file.read())
        for ob, slots in slotlist.items():
            ob.data.materials.clear()
            for slot in slots:
                ob.data.materials.append(slot)


# HAND OPERATOR

class OscApplyOverrides(Operator):
    """>>>Danger Option<<<  Apply and restore override materials, """ \
    """similar as ON/OFF its basically the same, save before try it"""
    bl_idname = "render.apply_overrides"
    bl_label = "Apply Overrides in this Scene"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefOscApplyOverrides(self)
        return {'FINISHED'}


class OscRestoreOverrides(Operator):
    """>>>Danger Option<<<  Apply and restore override materials, """ \
    """similar as ON/OFF its basically the same, save before try it"""
    bl_idname = "render.restore_overrides"
    bl_label = "Restore Overrides in this Scene"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefOscRestoreOverrides(self)
        return {'FINISHED'}

bpy.use_overrides = False


class OscOverridesOn(Operator):
    """>>>Danger Option<<< its recommended to save before try it, """ \
    """it replace all materials by the override materials, """ \
    """its possible once active to see the objects rendering as override render by pressing F12"""
    bl_idname = "render.overrides_on"
    bl_label = "Turn On Overrides"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        if bpy.use_overrides is False:
            bpy.app.handlers.render_pre.append(DefOscApplyOverrides)
            bpy.app.handlers.render_post.append(DefOscRestoreOverrides)
            bpy.use_overrides = True
            print("Overrides on!")
        else:
            bpy.app.handlers.render_pre.remove(DefOscApplyOverrides)
            bpy.app.handlers.render_post.remove(DefOscRestoreOverrides)
            bpy.use_overrides = False
            print("Overrides off!")
        return {'FINISHED'}


# -------------------- CHECK OVERRIDES -------------------

class OscCheckOverrides(Operator):
    """Check all overrides to verify if there is all set up properly, info its display in the console"""
    bl_idname = "render.check_overrides"
    bl_label = "Check Overrides"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        GROUPI = False
        GLOBAL = 0
        GLOBALERROR = 0

        print("==== STARTING CHECKING ====")
        print("")

        for SCENE in bpy.data.scenes[:]:
            MATLIST = []
            MATI = False

            for MATERIAL in bpy.data.materials[:]:
                MATLIST.append(MATERIAL.name)

            GROUPLIST = []
            for GROUP in bpy.data.groups[:]:
                if GROUP.users > 0:
                    GROUPLIST.append(GROUP.name)

            print("   %s Scene is checking" % (SCENE.name))

            for OVERRIDE in list(eval(SCENE.oscurart.overrides)):
                # REVISO OVERRIDES EN GRUPOS
                if OVERRIDE[0] in GROUPLIST:
                    pass
                else:
                    print("** %s group are in conflict." % (OVERRIDE[0]))
                    GROUPI = True
                    GLOBALERROR += 1
                # REVISO OVERRIDES EN GRUPOS
                if OVERRIDE[1] in MATLIST:
                    pass
                else:
                    print("** %s material are in conflict." % (OVERRIDE[1]))
                    MATI = True
                    GLOBALERROR += 1

            if MATI is False:
                print("-- Materials are ok.")
            else:
                GLOBAL += 1
            if GROUPI is False:
                print("-- Groups are ok.")
            else:
                GLOBAL += 1

        if GLOBAL < 1:
            self.report({'INFO'}, "Materials And Groups are Ok")
        if GLOBALERROR > 0:
            self.report({'WARNING'}, "Override Error: Look in the Console")
        print("")

        return {'FINISHED'}

# --------------------------------- OVERRIDES PANEL ----------------------


class OscOverridesGUI(Panel):
    bl_label = "Oscurart Material Overrides"
    bl_idname = "Oscurart Overrides List"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):

        layout = self.layout
        col = layout.column(align=1)
        colrow = col.row(align=1)
        colrow.operator("render.overrides_add_slot", icon="ZOOMIN")
        colrow.operator("render.overrides_remove_slot", icon="ZOOMOUT")
        col.operator("render.overrides_transfer", icon="SHORTDISPLAY")

        for i, m in enumerate(bpy.context.scene.ovlist):
            colrow = col.row(align=1)
            colrow.prop_search(m, "grooverride", bpy.data, "groups", text="")
            colrow.prop_search(
                m,
                "matoverride",
                bpy.data,
                "materials",
                text="")
            if i != len(bpy.context.scene.ovlist) - 1:
                pa = colrow.operator(
                    "ovlist.move_down",
                    text="",
                    icon="TRIA_DOWN")
                pa.index = i
            if i > 0:
                p = colrow.operator("ovlist.move_up", text="", icon="TRIA_UP")
                p.index = i
            pb = colrow.operator("ovlist.kill", text="", icon="X")
            pb.index = i


class OscOverridesUp(Operator):
    """Move override slot up"""
    bl_idname = 'ovlist.move_up'
    bl_label = 'Move Override up'
    bl_options = {'INTERNAL'}

    index = bpy.props.IntProperty(min=0)

    @classmethod
    def poll(self, context):
        return len(context.scene.ovlist)

    def execute(self, context):
        ovlist = context.scene.ovlist
        ovlist.move(self.index, self.index - 1)

        return {'FINISHED'}


class OscOverridesDown(Operator):
    """Move override slot down"""
    bl_idname = 'ovlist.move_down'
    bl_label = 'Move Override down'
    bl_options = {'INTERNAL'}

    index = bpy.props.IntProperty(min=0)

    @classmethod
    def poll(self, context):
        return len(context.scene.ovlist)

    def execute(self, context):
        ovlist = context.scene.ovlist
        ovlist.move(self.index, self.index + 1)
        return {'FINISHED'}


class OscOverridesKill(Operator):
    """Remove override slot"""
    bl_idname = 'ovlist.kill'
    bl_label = 'Kill Override'
    bl_options = {'INTERNAL'}

    index = bpy.props.IntProperty(min=0)

    @classmethod
    def poll(self, context):
        return len(context.scene.ovlist)

    def execute(self, context):
        ovlist = context.scene.ovlist
        ovlist.remove(self.index)
        return {'FINISHED'}


class OscTransferOverrides(Operator):
    """Applies the previously configured slots (Groups < Material) to the Scene. """ \
    """This should be transfer once the override groups are set"""
    bl_idname = "render.overrides_transfer"
    bl_label = "Transfer Overrides"

    def execute(self, context):
        # CREO LISTA
        OSCOV = [[OVERRIDE.grooverride, OVERRIDE.matoverride]
                 for OVERRIDE in bpy.context.scene.ovlist[:]
                 if OVERRIDE.matoverride != "" and OVERRIDE.grooverride != ""]

        bpy.context.scene.oscurart.overrides = str(OSCOV)
        return {'FINISHED'}


class OscAddOverridesSlot(Operator):
    """Add override slot"""
    bl_idname = "render.overrides_add_slot"
    bl_label = "Add Override Slot"

    def execute(self, context):
        prop = bpy.context.scene.ovlist.add()
        prop.matoverride = ""
        prop.grooverride = ""
        return {'FINISHED'}


class OscRemoveOverridesSlot(Operator):
    """Remove override slot"""
    bl_idname = "render.overrides_remove_slot"
    bl_label = "Remove Override Slot"

    def execute(self, context):
        context.scene.ovlist.remove(len(bpy.context.scene.ovlist) - 1)
        return {'FINISHED'}
