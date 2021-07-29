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
import os
from bpy.types import Operator
from bpy.props import BoolProperty
from bpy_extras.object_utils import world_to_camera_view

# ------------------------ SEARCH AND SELECT ------------------------


class SearchAndSelectOt(bpy.types.Operator):
    """Search and select objects, by name"""
    bl_idname = "object.search_and_select_osc"
    bl_label = "Search And Select"
    bl_options = {"REGISTER", "UNDO"}

    start = BoolProperty(name="Start With", default=True)
    count = BoolProperty(name="Contain", default=True)
    end = BoolProperty(name="End", default=True)

    def execute(self, context):
        for objeto in bpy.context.scene.objects:
            variableNombre = bpy.context.scene.SearchAndSelectOt
            if self.start:
                if objeto.name.startswith(variableNombre):
                    objeto.select = True
            if self.count:
                if objeto.name.count(variableNombre):
                    objeto.select = True
            if self.end:
                if objeto.name.count(variableNombre):
                    objeto.select = True
        return {'FINISHED'}


# -------------------------RENAME OBJECTS----------------------------------

# CREO VARIABLE
bpy.types.Scene.RenameObjectOt = bpy.props.StringProperty(default="Type here")


class renameObjectsOt (Operator):
    """Batch rename objects, supports selection order"""
    bl_idname = "object.rename_objects_osc"
    bl_label = "Rename Objects"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        i = 0
        listaObj = bpy.selection_osc
        for objeto in listaObj:
            objeto.name = "%s_%04d" % (bpy.context.scene.RenameObjectOt, i)
            i += 1
        return {'FINISHED'}


# ---------------------------REMOVE MODIFIERS Y APPLY MODIFIERS-----------

class oscRemModifiers (Operator):
    """Removes all modifiers in the selected objects"""
    bl_idname = "object.modifiers_remove_osc"
    bl_label = "Remove modifiers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for objeto in bpy.context.selected_objects:
            for modificador in objeto.modifiers:
                print(modificador.type)
                bpy.context.scene.objects.active = objeto
                bpy.ops.object.modifier_remove(modifier=modificador.name)
        return {'FINISHED'}


class oscApplyModifiers (Operator):
    """Applys all the modifiers in the selected objects.(This does not work in objects with shapekeys)"""
    bl_idname = "object.modifiers_apply_osc"
    bl_label = "Apply modifiers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for objeto in bpy.context.selected_objects:
            bpy.ops.object.select_all(action='DESELECT')
            bpy.context.scene.objects.active = objeto
            objeto.select = True
            if objeto.data.users >= 2:
                bpy.ops.object.make_single_user(
                    type='SELECTED_OBJECTS',
                    object=True,
                    obdata=True,
                    material=False,
                    texture=False,
                    animation=False)
            for modificador in objeto.modifiers:
                try:
                    bpy.ops.object.modifier_apply(
                        apply_as="DATA",
                        modifier=modificador.name)
                except:
                    bpy.ops.object.modifier_remove(modifier=modificador.name)
                    print("* Modifier %s skipping apply" % (modificador.name))

        return {'FINISHED'}


# ------------------------------------ RELINK OBJECTS---------------------


def relinkObjects(self):


    LISTSCENE = []
    if bpy.selection_osc:
        for SCENE in bpy.data.scenes[:]:
            if SCENE.objects:
                if bpy.selection_osc[-1] in SCENE.objects[:]:
                    LISTSCENE.append(SCENE)


        if LISTSCENE:
            OBJECTS = bpy.selection_osc[:-1]
            ACTOBJ = bpy.selection_osc[-1]
            OBJSEL = bpy.selection_osc[:]

            LISTSCENE.remove(bpy.context.scene)

            bpy.ops.object.select_all(action='DESELECT')


            for OBJETO in OBJECTS:
                if OBJETO.users != len(bpy.data.scenes):
                    print(OBJETO.name)
                    OBJETO.select = True

            for SCENE in LISTSCENE:
                bpy.ops.object.make_links_scene(scene=SCENE.name)

            bpy.context.scene.objects.active = ACTOBJ
            for OBJ in OBJSEL:
                OBJ.select = True
        else:
            self.report({'INFO'}, message="Scenes are empty")


class OscRelinkObjectsBetween (Operator):
    """Copies from the selected object the scenes where this is. Its similar to 'Objects to Scene'"""
    bl_idname = "object.relink_objects_between_scenes"
    bl_label = "Relink Objects Between Scenes"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        relinkObjects(self)
        return {'FINISHED'}


# ------------------------------------ COPY GROUPS AND LAYERS-------------


def CopyObjectGroupsAndLayers(self):

    OBSEL = bpy.selection_osc[:]

    if OBSEL:
        GLOBALLAYERS = list(OBSEL[-1].layers[:])
        ACTSCENE = bpy.context.scene
        GROUPS = OBSEL[-1].users_group
        ACTOBJ = OBSEL[-1]

        for OBJECT in OBSEL[:-1]:
            for scene in bpy.data.scenes[:]:

                # SI EL OBJETO ACTIVO ESTA EN LA ESCENA
                if ACTOBJ in scene.objects[:] and OBJECT in scene.objects[:]:
                    scene.object_bases[
                        OBJECT.name].layers[
                            :] = scene.object_bases[
                                ACTOBJ.name].layers[
                                    :]
                elif ACTOBJ not in scene.objects[:] and OBJECT in scene.objects[:]:
                    scene.object_bases[OBJECT.name].layers[:] = list(GLOBALLAYERS)

            # REMUEVO DE TODO GRUPO
            for GROUP in bpy.data.groups[:]:
                if GROUP in OBJECT.users_group[:]:
                    GROUP.objects.unlink(OBJECT)

            # INCLUYO OBJETO EN GRUPOS
            for GROUP in GROUPS:
                GROUP.objects.link(OBJECT)

        bpy.context.window.screen.scene = ACTSCENE
        bpy.context.scene.objects.active = ACTOBJ


class OscCopyObjectGAL (Operator):
    """Copies to scenes the layers setup in the active scene of the selected object"""
    bl_idname = "object.copy_objects_groups_layers"
    bl_label = "Copy Groups And Layers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        CopyObjectGroupsAndLayers(self)
        return {'FINISHED'}


# ------------------------------------ SELECTION -------------------------
bpy.selection_osc = []


def select_osc():
    if bpy.context.mode == "OBJECT":
        obj = bpy.context.object
        sel = len(bpy.context.selected_objects)

        if sel == 0:
            bpy.selection_osc = []
        else:
            if sel == 1:
                bpy.selection_osc = []
                bpy.selection_osc.append(obj)
            elif sel > len(bpy.selection_osc):
                for sobj in bpy.context.selected_objects:
                    if (sobj in bpy.selection_osc) is False:
                        bpy.selection_osc.append(sobj)

            elif sel < len(bpy.selection_osc):
                for it in bpy.selection_osc:
                    if (it in bpy.context.selected_objects) is False:
                        bpy.selection_osc.remove(it)


class OscSelection(bpy.types.Header):
    bl_label = "Selection Osc"
    bl_space_type = "VIEW_3D"

    def __init__(self):
        select_osc()

    def draw(self, context):
        """
        layout = self.layout
        row = layout.row()
        row.label("Sels: "+str(len(bpy.selection_osc)))
        """

# =============== DISTRIBUTE ======================


def ObjectDistributeOscurart(self, X, Y, Z):
    if len(bpy.selection_osc[:]) > 1:
        # VARIABLES
        dif = bpy.selection_osc[-1].location - bpy.selection_osc[0].location
        chunkglobal = dif / (len(bpy.selection_osc[:]) - 1)
        chunkx = 0
        chunky = 0
        chunkz = 0
        deltafst = bpy.selection_osc[0].location

        # ORDENA
        for OBJECT in bpy.selection_osc[:]:
            if X:
                OBJECT.location.x = deltafst[0] + chunkx
            if Y:
                OBJECT.location[1] = deltafst[1] + chunky
            if Z:
                OBJECT.location.z = deltafst[2] + chunkz
            chunkx += chunkglobal[0]
            chunky += chunkglobal[1]
            chunkz += chunkglobal[2]
    else:
        self.report({'INFO'}, "Needs at least two selected objects")


class DialogDistributeOsc(Operator):
    """Distribute evenly the selected objects in x y z"""
    bl_idname = "object.distribute_osc"
    bl_label = "Distribute Objects"
    Boolx = BoolProperty(name="X")
    Booly = BoolProperty(name="Y")
    Boolz = BoolProperty(name="Z")

    def execute(self, context):
        ObjectDistributeOscurart(self, self.Boolx, self.Booly, self.Boolz)
        return {'FINISHED'}

    def invoke(self, context, event):
        self.Boolx = True
        self.Booly = True
        self.Boolz = True
        return context.window_manager.invoke_props_dialog(self)


# ======================== SET LAYERS TO OTHER SCENES ====================


def DefSetLayersToOtherScenes():
    actsc = bpy.context.screen.scene
    for object in bpy.context.selected_objects[:]:
        bpy.context.screen.scene = actsc
        lyrs = object.layers[:]
        for scene in bpy.data.scenes[:]:
            if object in scene.objects[:]:
                bpy.context.screen.scene = scene
                object.layers = lyrs
            else:
                print("* %s is not in %s" % (object.name, scene.name))

    bpy.context.screen.scene = actsc


class SetLayersToOtherScenes (Operator):
    """Copies to scenes the layers setup in the active scene of the selected object"""
    bl_idname = "object.set_layers_to_other_scenes"
    bl_label = "Copy actual Layers to Other Scenes"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefSetLayersToOtherScenes()
        return {'FINISHED'}


# ======================== RENDER OBJECTS IN CAMERA ======================


def DefRenderOnlyInCamera():
    # crea grupos
    if "INCAMERA" not in bpy.data.groups:
        bpy.data.groups.new("INCAMERA")
    if "NOTINCAMERA" not in bpy.data.groups:
        bpy.data.groups.new("NOTINCAMERA")

    # limpio grupos
    for ob in bpy.data.objects:
        if ob.name in bpy.data.groups["INCAMERA"].objects:
            bpy.data.groups["INCAMERA"].objects.unlink(ob)
        if ob.name in bpy.data.groups["NOTINCAMERA"].objects:
            bpy.data.groups["NOTINCAMERA"].objects.unlink(ob)

    # ordeno grupos
    for ob in bpy.data.objects:
        obs = False
        if ob.type == "MESH":
            tm = ob.to_mesh(bpy.context.scene, True, "RENDER")
            for vert in tm.vertices:
                cam = world_to_camera_view(
                    bpy.context.scene,
                    bpy.context.scene.camera,
                    vert.co + ob.location)
                if cam[0] >= -0 and cam[0] <= 1 and cam[1] >= 0 and cam[1] <= 1:
                    obs = True
            del(tm)
        else:
            obs = True
        if obs:
            bpy.data.groups["INCAMERA"].objects.link(ob)
        else:
            bpy.data.groups["NOTINCAMERA"].objects.link(ob)


class RenderOnlyInCamera (Operator):
    """Create two different groups, one group contains the objetcs that are in the camera frame, """ \
    """those that camera can see, and then a second group that contains the object that the camera can`t see"""
    bl_idname = "group.group_in_out_camera"
    bl_label = "Make a group for objects in outer camera"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefRenderOnlyInCamera()
        return {'FINISHED'}


# ------------------------ DUPLICATE OBJECTS SYMMETRY ------------------------

def duplicateSymmetrical(self, disconect):
    for objeto in bpy.context.selected_objects:

        bpy.ops.object.select_all(action='DESELECT')
        objeto.select = 1
        bpy.context.scene.objects.active = objeto
        bpy.ops.object.duplicate(linked=1)
        OBDUP = bpy.context.active_object
        print(OBDUP)
        OBDUP.driver_add("location")
        OBDUP.animation_data.drivers[0].driver.expression = "-var"
        OBDUP.animation_data.drivers[0].driver.variables.new()
        OBDUP.animation_data.drivers[0].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            0].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            0].driver.variables[
                0].targets[
            0].transform_type = 'LOC_X'
        OBDUP.animation_data.drivers[1].driver.expression = "var"
        OBDUP.animation_data.drivers[1].driver.variables.new()
        OBDUP.animation_data.drivers[1].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            1].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            1].driver.variables[
                0].targets[
            0].transform_type = 'LOC_Y'
        OBDUP.animation_data.drivers[2].driver.expression = "var"
        OBDUP.animation_data.drivers[2].driver.variables.new()
        OBDUP.animation_data.drivers[2].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            2].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            2].driver.variables[
                0].targets[
            0].transform_type = 'LOC_Z'
        OBDUP.driver_add("scale")
        OBDUP.animation_data.drivers[3].driver.expression = "-var"
        OBDUP.animation_data.drivers[3].driver.variables.new()
        OBDUP.animation_data.drivers[3].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            3].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            3].driver.variables[
                0].targets[
            0].transform_type = 'SCALE_X'
        OBDUP.animation_data.drivers[4].driver.expression = "var"
        OBDUP.animation_data.drivers[4].driver.variables.new()
        OBDUP.animation_data.drivers[4].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            4].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            4].driver.variables[
                0].targets[
            0].transform_type = 'SCALE_Y'
        OBDUP.animation_data.drivers[5].driver.expression = "var"
        OBDUP.animation_data.drivers[5].driver.variables.new()
        OBDUP.animation_data.drivers[5].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            5].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            5].driver.variables[
                0].targets[
            0].transform_type = 'SCALE_Z'
        OBDUP.driver_add("rotation_euler")
        OBDUP.animation_data.drivers[6].driver.expression = "var"
        OBDUP.animation_data.drivers[6].driver.variables.new()
        OBDUP.animation_data.drivers[6].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            6].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            6].driver.variables[
                0].targets[
            0].transform_type = 'ROT_X'
        OBDUP.animation_data.drivers[7].driver.expression = "-var"
        OBDUP.animation_data.drivers[7].driver.variables.new()
        OBDUP.animation_data.drivers[7].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            7].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            7].driver.variables[
                0].targets[
            0].transform_type = 'ROT_Y'
        OBDUP.animation_data.drivers[8].driver.expression = "-var"
        OBDUP.animation_data.drivers[8].driver.variables.new()
        OBDUP.animation_data.drivers[8].driver.variables[0].type = "TRANSFORMS"
        OBDUP.animation_data.drivers[
            8].driver.variables[
                0].targets[
            0].id = objeto
        OBDUP.animation_data.drivers[
            8].driver.variables[
                0].targets[
            0].transform_type = 'ROT_Z'

        if disconect is not True:
            bpy.ops.object.make_single_user(obdata=True, object=True)
            bpy.context.active_object.driver_remove("location")
            bpy.context.active_object.driver_remove("rotation_euler")
            bpy.context.active_object.driver_remove("scale")


class oscDuplicateSymmetricalOp (Operator):
    """Creates a symmetrical copy on the X axys, also Links by drivers position, rotation and scale"""
    bl_idname = "object.duplicate_object_symmetry_osc"
    bl_label = "Oscurart Duplicate Symmetrical"
    bl_options = {"REGISTER", "UNDO"}

    desconecta = BoolProperty(name="Keep Connection", default=True)

    def execute(self, context):

        duplicateSymmetrical(self, self.desconecta)

        return {'FINISHED'}


# ------------------------ OBJECTS TO GROUPS ------------------------

def DefObjectToGroups():
    try:
        "%s_MSH" % (os.path.basename(bpy.data.filepath).replace(".blend", ""))
        scgr = bpy.data.groups["%s_MSH" % (os.path.basename(bpy.data.filepath).replace(".blend", ""))]
    except:    
        scgr = bpy.data.groups.new(
            "%s_MSH" %
            (os.path.basename(bpy.data.filepath).replace(".blend", "")))
    for ob in bpy.data.objects:
        if ob.select:
            if ob.type == "MESH":
                gr = bpy.data.groups.new(ob.name)
                gr.objects.link(ob)
                scgr.objects.link(ob)


class ObjectsToGroups (Operator):
    """Creates a group("_MESH") containing all the mesh type objects in the scene. """ \
    """Creates a group(“object_name”) per mesh type object"""
    bl_idname = "object.objects_to_groups"
    bl_label = "Objects to Groups"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        DefObjectToGroups()
        return {'FINISHED'}
