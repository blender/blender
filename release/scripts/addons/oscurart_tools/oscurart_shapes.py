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
from bpy.types import Operator
from bpy.props import (
            BoolProperty,
            FloatProperty,
            )
import math


# ---------------------CREATE SHAPES----------------

def DefSplitShapes(self, ACTIVESHAPE, LAYOUTCOMPAT):
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    ACTOBJ = bpy.context.active_object
    has_keys = hasattr(getattr(ACTOBJ.data, "shape_keys", None), "key_blocks")
    if has_keys:
        INDEX = ACTOBJ.active_shape_key_index

        if LAYOUTCOMPAT:
            for SHAPE in ACTOBJ.data.shape_keys.key_blocks:
                if len(SHAPE.name) > 7:
                    SHAPE.name = SHAPE.name[:8]

        if ACTIVESHAPE:
            ACTOBJ.active_shape_key_index = INDEX
            AS = ACTOBJ.active_shape_key
            AS.value = 1
            SHAPE = ACTOBJ.shape_key_add(name=AS.name[:8] + "_L", from_mix=True)
            SHAPE.vertex_group = "_L"
            SHAPE2 = ACTOBJ.shape_key_add(name=AS.name[:8] + "_R", from_mix=True)
            SHAPE2.vertex_group = "_R"
            bpy.ops.object.shape_key_clear()
        else:
            for SHAPE in ACTOBJ.data.shape_keys.key_blocks[1:]:
                SHAPE.value = 1
                SHAPE1 = ACTOBJ.shape_key_add(
                    name=SHAPE.name[:8] + "_L",
                    from_mix=True)
                SHAPE1.vertex_group = "_L"
                SHAPE2 = ACTOBJ.shape_key_add(
                    name=SHAPE.name[:8] + "_R",
                    from_mix=True)
                SHAPE2.vertex_group = "_R"
                bpy.ops.object.shape_key_clear()
            ACTOBJ.active_shape_key_index = INDEX

    return has_keys


class CreaShapes(Operator):
    """Divide on left and right the diffenrent Shapekeys. “Create Mix Groups” its required"""
    bl_idname = "mesh.split_lr_shapes_osc"
    bl_label = "Split LR Shapes"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.active_object.type in
                {'MESH', 'SURFACE', 'CURVE'})

    activeshape = BoolProperty(
            name="Only Active Shape",
            default=False
            )
    layoutcompat = BoolProperty(
            name="Layout Compatible",
            default=True
            )

    def execute(self, context):

        is_done = DefSplitShapes(self, self.activeshape,
                                 self.layoutcompat)
        if not is_done:
            self.report({'INFO'}, message="Active object doesn't have shape keys")
            return {'CANCELLED'}

        return {'FINISHED'}


# ----------------------------SHAPES LAYOUT-----------------------

class CreaShapesLayout(Operator):
    """Creates an interface to control the Shapekeys of symmetrical Objects. “Create Mix Groups” its required"""
    bl_idname = "mesh.create_symmetrical_layout_osc"
    bl_label = "Symmetrical Layout"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.active_object.type in
                {'MESH', 'SURFACE', 'CURVE'})

    def execute(self, context):

        SEL_OBJ = bpy.context.active_object
        has_keys = hasattr(getattr(SEL_OBJ.data, "shape_keys", None), "key_blocks")
        if has_keys:
            LISTA_KEYS = bpy.context.active_object.data.shape_keys.key_blocks[1:]

            EDITMODE = "bpy.ops.object.mode_set(mode='EDIT')"
            OBJECTMODE = "bpy.ops.object.mode_set(mode='OBJECT')"
            POSEMODE = "bpy.ops.object.mode_set(mode='POSE')"

            amt = bpy.data.armatures.new("ArmatureData")
            ob = bpy.data.objects.new("RIG_LAYOUT_" + SEL_OBJ.name, amt)

            scn = bpy.context.scene
            scn.objects.link(ob)
            scn.objects.active = ob
            ob.select = True

            verticess = [(-1, 1, 0), (1, 1, 0), (1, -1, 0), (-1, -1, 0)]
            edgess = [(0, 1), (1, 2), (2, 3), (3, 0)]
            mesh = bpy.data.meshes.new("%s_data_container" % (SEL_OBJ))
            object = bpy.data.objects.new("GRAPHIC_CONTAINER", mesh)
            bpy.context.scene.objects.link(object)
            mesh.from_pydata(verticess, edgess, [])

            gx = 0
            gy = 0

            for keyblock in LISTA_KEYS:
                if keyblock.name[-2:] != "_L":
                    if keyblock.name[-2:] != "_R":

                        scn.objects.active = ob
                        eval(EDITMODE)

                        bone = amt.edit_bones.new(keyblock.name)
                        bone.head = (gx, 0, 0)
                        bone.tail = (gx, 0, 1)

                        bonectrl = amt.edit_bones.new(keyblock.name + "_CTRL")
                        bonectrl.head = (gy, 0, 0)
                        bonectrl.tail = (gy, 0, 0.2)

                        ob.data.edit_bones[
                            bonectrl.name].parent = ob.data.edit_bones[
                                bone.name]
                        bpy.context.scene.objects.active = ob

                        for SIDE in ["L", "R"]:
                            DR = SEL_OBJ.data.shape_keys.key_blocks[
                                keyblock.name + "_" + SIDE].driver_add("value")
                            if SIDE == "L":
                                DR.driver.expression = "var+var_001"
                            else:
                                DR.driver.expression = "-var+var_001"
                            VAR1 = DR.driver.variables.new()
                            VAR2 = DR.driver.variables.new()

                            VAR1.targets[0].id = ob
                            VAR1.type = 'TRANSFORMS'
                            VAR1.targets[0].bone_target = bonectrl.name
                            VAR1.targets[0].transform_space = "LOCAL_SPACE"
                            VAR1.targets[0].transform_type = "LOC_X"
                            VAR2.targets[0].id = ob
                            VAR2.type = 'TRANSFORMS'
                            VAR2.targets[0].bone_target = bonectrl.name
                            VAR2.targets[0].transform_space = "LOCAL_SPACE"
                            VAR2.targets[0].transform_type = "LOC_Y"

                        eval(POSEMODE)

                        ob.pose.bones[keyblock.name].custom_shape = object
                        ob.pose.bones[
                            keyblock.name +
                            "_CTRL"].custom_shape = object
                        CNS = ob.pose.bones[
                            keyblock.name +
                            "_CTRL"].constraints.new(
                                type='LIMIT_LOCATION')
                        CNS.min_x = -1
                        CNS.use_min_x = 1
                        CNS.min_z = 0
                        CNS.use_min_z = 1
                        CNS.min_y = -1
                        CNS.use_min_y = 1
                        CNS.max_x = 1
                        CNS.use_max_x = 1
                        CNS.max_z = 0
                        CNS.use_max_z = 1
                        CNS.max_y = 1
                        CNS.use_max_y = 1
                        CNS.owner_space = "LOCAL"
                        CNS.use_transform_limit = True

                        eval(OBJECTMODE)

                        bpy.ops.object.text_add(location=(gx, 0, 0))
                        gx = gx + 2.2
                        gy = gy + 2.2
                        texto = bpy.context.object

                        texto.data.body = keyblock.name
                        texto.name = "TEXTO_" + keyblock.name

                        texto.rotation_euler[0] = math.pi / 2
                        texto.location.x = -1
                        texto.location.z = -1
                        texto.data.size = .2

                        CNS = texto.constraints.new(type="COPY_LOCATION")
                        CNS.target = ob
                        CNS.subtarget = ob.pose.bones[keyblock.name].name
                        CNS.use_offset = True
        else:
            self.report({'INFO'}, message="Active object doesn't have shape keys")
            return {'CANCELLED'}

        return {'FINISHED'}


# ----------------------------CREATE LMR GROUPS-------------------

def createLMRGroups(self, FACTORVG, ADDVG):
    bpy.context.window.screen.scene.tool_settings.mesh_select_mode = (
        True, False, False)

    ACTOBJ = bpy.context.active_object
    bpy.ops.object.mode_set(mode="EDIT", toggle=False)
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode="OBJECT")
    GRUPOS = ["_L", "_R"]
    MIRRORINDEX = 0

    for LADO in GRUPOS:
        if MIRRORINDEX == 0:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.object.vertex_group_assign()
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.object.mode_set(mode='WEIGHT_PAINT', toggle=False)
            for VERTICE in ACTOBJ.data.vertices:
                VERTICE.groups[-1].weight = (VERTICE.co[0] * FACTORVG) + ADDVG
            ACTOBJ.vertex_groups[-1].name = LADO
        else:
            bpy.ops.object.vertex_group_add()
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.object.vertex_group_assign()
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.object.mode_set(mode='WEIGHT_PAINT', toggle=False)
            for VERTICE in ACTOBJ.data.vertices:
                VERTICE.groups[-1].weight = (-VERTICE.co[0] * FACTORVG) + ADDVG
            ACTOBJ.vertex_groups[-1].name = LADO
        MIRRORINDEX += 1

    ACTOBJ.vertex_groups.active_index = len(ACTOBJ.vertex_groups)


class CreaGrupos(Operator):
    """It creates a vertex group in symmetrical objects, ideal for smoothly mixing shapekeys"""
    bl_idname = "mesh.create_lmr_groups_osc"
    bl_label = "Create Mix groups"
    bl_options = {'REGISTER', 'UNDO'}

    FACTORVG = FloatProperty(
            name="Factor",
            default=1,
            min=0,
            max=1000
            )
    ADDVG = FloatProperty(
            name="Addition",
            default=.5,
            min=0,
            max=1000
            )

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.active_object.type == 'MESH')

    def execute(self, context):

        createLMRGroups(self, self.FACTORVG, self.ADDVG)

        return {'FINISHED'}


# ------------------------ SHAPES LAYOUT SYMMETRICA ------------------------

class CreateLayoutAsymmetrical(Operator):
    """Creates an interface to control the Shapekeys of symmetrical Objects. “Create Mix Groups” its required"""
    bl_idname = "mesh.create_asymmetrical_layout_osc"
    bl_label = "Asymmetrical Layout"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.active_object.type in
                {'MESH', 'SURFACE', 'CURVE'})

    def execute(self, context):

        SEL_OBJ = bpy.context.active_object

        has_keys = hasattr(getattr(SEL_OBJ.data, "shape_keys", None), "key_blocks")
        if has_keys:
            LISTA_KEYS = bpy.context.active_object.data.shape_keys.key_blocks[1:]

            EDITMODE = "bpy.ops.object.mode_set(mode='EDIT')"
            OBJECTMODE = "bpy.ops.object.mode_set(mode='OBJECT')"
            POSEMODE = "bpy.ops.object.mode_set(mode='POSE')"

            amtas = bpy.data.armatures.new("ArmatureData")
            obas = bpy.data.objects.new("RIG_LAYOUT_" + SEL_OBJ.name, amtas)

            scn = bpy.context.scene
            scn.objects.link(obas)
            scn.objects.active = obas
            obas.select = True

            verticess = [(-.1, 1, 0), (.1, 1, 0), (.1, 0, 0), (-.1, 0, 0)]
            edgess = [(0, 1), (1, 2), (2, 3), (3, 0)]
            mesh = bpy.data.meshes.new("%s_data_container" % (SEL_OBJ))
            object = bpy.data.objects.new("GRAPHIC_CONTAINER_AS", mesh)
            bpy.context.scene.objects.link(object)
            mesh.from_pydata(verticess, edgess, [])

            eval(EDITMODE)
            gx = 0
            gy = 0

            for keyblock in LISTA_KEYS:
                if keyblock.name[-2:] != "_L":
                    if keyblock.name[-2:] != "_R":
                        scn.objects.active = obas
                        eval(EDITMODE)
                        bone = amtas.edit_bones.new(keyblock.name)
                        bone.head = (gx, 0, 0)
                        bone.tail = (gx, 0, 1)

                        bonectrl = amtas.edit_bones.new(keyblock.name + "_CTRL")
                        bonectrl.head = (gy, 0, 0)
                        bonectrl.tail = (gy, 0, 0.2)

                        obas.data.edit_bones[
                            bonectrl.name].parent = obas.data.edit_bones[
                                bone.name]
                        bpy.context.scene.objects.active = obas

                        bpy.ops.armature.select_all(action="DESELECT")

                        DR1 = keyblock.driver_add("value")
                        DR1.driver.expression = "var"
                        VAR2 = DR1.driver.variables.new()
                        VAR2.targets[0].id = obas
                        VAR2.targets[0].bone_target = bonectrl.name
                        VAR2.type = 'TRANSFORMS'
                        VAR2.targets[0].transform_space = "LOCAL_SPACE"
                        VAR2.targets[0].transform_type = "LOC_Y"

                        eval(POSEMODE)

                        obas.pose.bones[keyblock.name].custom_shape = object
                        obas.pose.bones[
                            keyblock.name +
                            "_CTRL"].custom_shape = object

                        bpy.data.objects[
                            "RIG_LAYOUT_" +
                            SEL_OBJ.name].data.bones.active = bpy.data.objects[
                                "RIG_LAYOUT_" +
                                SEL_OBJ.name].data.bones[
                                    keyblock.name +
                                    "_CTRL"]

                        eval(POSEMODE)
                        CNS = obas.pose.bones[
                            keyblock.name +
                            "_CTRL"].constraints.new(
                                type='LIMIT_LOCATION')
                        CNS.min_x = 0
                        CNS.use_min_x = 1
                        CNS.min_z = 0
                        CNS.use_min_z = 1
                        CNS.min_y = 0
                        CNS.use_min_y = 1

                        CNS.max_x = 0
                        CNS.use_max_x = 1
                        CNS.max_z = 0
                        CNS.use_max_z = 1
                        CNS.max_y = 1
                        CNS.use_max_y = 1

                        CNS.owner_space = "LOCAL"
                        CNS.use_transform_limit = True

                        eval(OBJECTMODE)

                        bpy.ops.object.text_add(location=(0, 0, 0))
                        gx = gx + 2.2
                        gy = gy + 2.2
                        texto = bpy.context.object
                        texto.data.body = keyblock.name
                        texto.name = "TEXTO_" + keyblock.name

                        texto.rotation_euler[0] = math.pi / 2
                        texto.location.x = -.15
                        texto.location.z = -.15
                        texto.data.size = .2

                        CNS = texto.constraints.new(type="COPY_LOCATION")
                        CNS.target = obas
                        CNS.subtarget = obas.pose.bones[keyblock.name].name
                        CNS.use_offset = True
        else:
            self.report({'INFO'}, message="Active object doesn't have shape keys")
            return {'CANCELLED'}

        return {'FINISHED'}


# ---------------------------SHAPES TO OBJECTS------------------

class ShapeToObjects(Operator):
    """It creates a new object for every shapekey in the selected object, ideal to export to other 3D software Apps"""
    bl_idname = "object.shape_key_to_objects_osc"
    bl_label = "Shapes To Objects"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.active_object.type in
                {'MESH', 'SURFACE', 'CURVE'})

    def execute(self, context):
        OBJACT = bpy.context.active_object
        has_keys = hasattr(getattr(OBJACT.data, "shape_keys", None), "key_blocks")
        if has_keys:
            for SHAPE in OBJACT.data.shape_keys.key_blocks[:]:
                print(SHAPE.name)
                bpy.ops.object.shape_key_clear()
                SHAPE.value = 1
                mesh = OBJACT.to_mesh(bpy.context.scene, True, 'PREVIEW')
                object = bpy.data.objects.new(SHAPE.name, mesh)
                bpy.context.scene.objects.link(object)
        else:
            self.report({'INFO'}, message="Active object doesn't have shape keys")
            return {'CANCELLED'}

        return {'FINISHED'}
