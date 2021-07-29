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
    "name": "Bool Tool",
    "author": "Vitor Balbio, Mikhail Rachinskiy, TynkaTopi, Meta-Androcto",
    "version": (0, 3, 8),
    "blender": (2, 78, 0),
    "location": "View3D > Toolshelf",
    "description": "Bool Tools Hotkey: Ctrl Shift B",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Object/BoolTool",
    "category": "Object",
    }

import bpy
from bpy.app.handlers import persistent
from bpy.types import (
        AddonPreferences,
        Operator,
        Panel,
        Menu,
        )
from bpy.props import (
        BoolProperty,
        StringProperty,
        EnumProperty,
        )


# -------------------  Bool Tool FUNCTIONS -------------------------
# Utils:

# Hide boolean objects
def update_BoolHide(self, context):
    ao = context.scene.objects.active
    objs = [i.object for i in ao.modifiers if i.type == 'BOOLEAN']
    hide_state = context.scene.BoolHide

    for o in objs:
        o.hide = hide_state


# Object is a Canvas
def isCanvas(_obj):
    try:
        if _obj["BoolToolRoot"]:
            return True
    except:
        return False


# Object is a Brush Tool Bool
def isBrush(_obj):
    try:
        if _obj["BoolToolBrush"]:
            return True
    except:
        return False


# Object is a Poly Brush Tool Bool collection
def isPolyBrush(_obj):
    try:
        if _obj["BoolToolPolyBrush"]:
            return True
    except:
        return False


def BT_ObjectByName(obj):
    for ob in bpy.context.scene.objects:
        if isCanvas(ob) or isBrush(ob):
            if ob.name == obj:
                return ob


def FindCanvas(obj):
    for ob in bpy.context.scene.objects:
        if isCanvas(ob):
            for mod in ob.modifiers:
                if ("BTool_" in mod.name):
                    if (obj.name in mod.name):
                        return ob


def isFTransf():
    user_preferences = bpy.context.user_preferences
    addons = user_preferences.addons
    addon_prefs = addons[__name__].preferences
    if addon_prefs.fast_transform:
        return True
    else:
        return False


"""
# EXPERIMENTAL FEATURES
def isMakeVertexGroup():
    user_preferences = bpy.context.user_preferences
    addon_prefs = user_preferences.addons[__name__].preferences
    if addon_prefs.make_vertex_groups:
        return True
    else:
        return False

def isMakeBoundary():
    user_preferences = bpy.context.user_preferences
    addon_prefs = user_preferences.addons[__name__].preferences
    if addon_prefs.make_boundary:
        return True
    else:
        return False
"""


def ConvertToMesh(obj):
    act = bpy.context.scene.objects.active
    bpy.context.scene.objects.active = obj
    bpy.ops.object.convert(target="MESH")
    bpy.context.scene.objects.active = act


# Do the Union, Difference and Intersection Operations with a Brush
def Operation(context, _operation):

    prefs = bpy.context.user_preferences.addons[__name__].preferences
    useWire = prefs.use_wire
    solver = prefs.solver

    for selObj in bpy.context.selected_objects:
        if selObj != context.active_object and (selObj.type == "MESH" or selObj.type == "CURVE"):
            if selObj.type == "CURVE":
                ConvertToMesh(selObj)
            actObj = context.active_object
            selObj.hide_render = True
            cyclesVis = selObj.cycles_visibility
            """
            for obj in bpy.context.scene.objects:
                if isCanvas(obj):
                    for mod in obj.modifiers:
                        if(mod.name == "BTool_" + selObj.name):
                            obj.modifiers.remove(mod)
            """
            if useWire:
                selObj.draw_type = "WIRE"
            else:
                selObj.draw_type = "BOUNDS"

            cyclesVis.camera = False
            cyclesVis.diffuse = False
            cyclesVis.glossy = False
            cyclesVis.shadow = False
            cyclesVis.transmission = False
            if _operation == "SLICE":
                # copies dupli_group property(empty), but group property is empty (users_group = None)
                clone = context.active_object.copy()
                # clone.select = True
                context.scene.objects.link(clone)
                sliceMod = clone.modifiers.new("BTool_" + selObj.name, "BOOLEAN")  # add mod to clone obj
                sliceMod.object = selObj
                sliceMod.operation = "DIFFERENCE"
                clone["BoolToolRoot"] = True
            newMod = actObj.modifiers.new("BTool_" + selObj.name, "BOOLEAN")
            newMod.object = selObj
            newMod.solver = solver
            if _operation == "SLICE":
                newMod.operation = "INTERSECT"
            else:
                newMod.operation = _operation

            actObj["BoolToolRoot"] = True
            selObj["BoolToolBrush"] = _operation
            selObj["BoolTool_FTransform"] = "False"


# Remove Obejcts form the BoolTool System
def Remove(context, thisObj_name, Prop):
    # Find the Brush pointed in the Tree View and Restore it, active is the Canvas
    actObj = context.active_object

    # Restore the Brush
    def RemoveThis(_thisObj_name):
        for obj in bpy.context.scene.objects:
            # if it's the brush object
            if obj.name == _thisObj_name:
                cyclesVis = obj.cycles_visibility
                obj.draw_type = "TEXTURED"
                del obj["BoolToolBrush"]
                del obj["BoolTool_FTransform"]
                cyclesVis.camera = True
                cyclesVis.diffuse = True
                cyclesVis.glossy = True
                cyclesVis.shadow = True
                cyclesVis.transmission = True

                # Remove it from the Canvas
                for mod in actObj.modifiers:
                    if ("BTool_" in mod.name):
                        if (_thisObj_name in mod.name):
                            actObj.modifiers.remove(mod)

    if Prop == "THIS":
        RemoveThis(thisObj_name)

    # If the remove was called from the Properties:
    else:
        # Remove the Brush Property
        if Prop == "BRUSH":
            Canvas = FindCanvas(actObj)
            if Canvas:
                for mod in Canvas.modifiers:
                    if ("BTool_" in mod.name):
                        if (actObj.name in mod.name):
                            Canvas.modifiers.remove(mod)
            cyclesVis = actObj.cycles_visibility
            actObj.draw_type = "TEXTURED"
            del actObj["BoolToolBrush"]
            del actObj["BoolTool_FTransform"]
            cyclesVis.camera = True
            cyclesVis.diffuse = True
            cyclesVis.glossy = True
            cyclesVis.shadow = True
            cyclesVis.transmission = True

        if Prop == "CANVAS":
            for mod in actObj.modifiers:
                if ("BTool_" in mod.name):
                    RemoveThis(mod.object.name)


# Toggle the Enable the Brush Object Property
def EnableBrush(context, objList, canvas):
    for obj in objList:
        for mod in canvas.modifiers:
            if ("BTool_" in mod.name and mod.object.name == obj):

                if (mod.show_viewport):
                    mod.show_viewport = False
                    mod.show_render = False
                else:
                    mod.show_viewport = True
                    mod.show_render = True


# Find the Canvas and Enable this Brush
def EnableThisBrush(context, set):
    canvas = None
    for obj in bpy.context.scene.objects:
        if obj != bpy.context.active_object:
            if isCanvas(obj):
                for mod in obj.modifiers:
                    if ("BTool_" in mod.name):
                        if mod.object == bpy.context.active_object:
                            canvas = obj

    for mod in canvas.modifiers:
        if ("BTool_" in mod.name):
            if mod.object == bpy.context.active_object:
                if set == "None":
                    if (mod.show_viewport):
                        mod.show_viewport = False
                        mod.show_render = False
                    else:
                        mod.show_viewport = True
                        mod.show_render = True
                else:
                    if (set == "True"):
                        mod.show_viewport = True
                    else:
                        mod.show_viewport = False
                return


# Toggle the Fast Transform Property of the Active Brush
def EnableFTransf(context):
    actObj = bpy.context.active_object

    if actObj["BoolTool_FTransform"] == "True":
        actObj["BoolTool_FTransform"] = "False"
    else:
        actObj["BoolTool_FTransform"] = "True"
    return


# Apply All Brushes to the Canvas
def ApplyAll(context, list):
    objDeleteList = []
    for selObj in list:
        if isCanvas(selObj) and selObj == context.active_object:
            for mod in selObj.modifiers:
                if ("BTool_" in mod.name):
                    objDeleteList.append(mod.object)
                try:
                    bpy.ops.object.modifier_apply(modifier=mod.name)
                except:  # if fails the means it is multiuser data
                    context.active_object.data = context.active_object.data.copy()  # so just make data unique
                    bpy.ops.object.modifier_apply(modifier=mod.name)
            del selObj['BoolToolRoot']

    for obj in context.scene.objects:
        if isCanvas(obj):
            for mod in obj.modifiers:
                if mod.type == 'BOOLEAN':
                    if mod.object in objDeleteList:  # do not delete brush that is used by another canvas
                        objDeleteList.remove(mod.object)  # remove it from deletion

    bpy.ops.object.select_all(action='DESELECT')
    for obj in objDeleteList:
        obj.select = True
    bpy.ops.object.delete()


# Apply This Brush to the Canvas
def ApplyThisBrush(context, brush):
    for obj in context.scene.objects:
        if isCanvas(obj):
            for mod in obj.modifiers:
                if ("BTool_" + brush.name in mod.name):
                    """
                    # EXPERIMENTAL
                    if isMakeVertexGroup():
                        # Turn all faces of the Brush selected
                        bpy.context.scene.objects.active = brush
                        bpy.ops.object.mode_set(mode='EDIT')
                        bpy.ops.mesh.select_all(action='SELECT')
                        bpy.ops.object.mode_set(mode='OBJECT')

                        # Turn off al faces of the Canvas selected
                        bpy.context.scene.objects.active = canvas
                        bpy.ops.object.mode_set(mode='EDIT')
                        bpy.ops.mesh.select_all(action='DESELECT')
                        bpy.ops.object.mode_set(mode='OBJECT')
                    """

                    # Apply This Brush
                    context.scene.objects.active = obj
                    try:
                        bpy.ops.object.modifier_apply(modifier=mod.name)
                    except:  # if fails the means it is multiuser data
                        context.active_object.data = context.active_object.data.copy()  # so just make data unique
                        bpy.ops.object.modifier_apply(modifier=mod.name)
                    bpy.ops.object.select_all(action='TOGGLE')
                    bpy.ops.object.select_all(action='DESELECT')

                    """
                    # EXPERIMENTAL
                    if isMakeVertexGroup():
                        # Make Vertex Group
                        bpy.ops.object.mode_set(mode='EDIT')
                        bpy.ops.object.vertex_group_assign_new()
                        bpy.ops.mesh.select_all(action='DESELECT')
                        bpy.ops.object.mode_set(mode='OBJECT')

                        canvas.vertex_groups.active.name = "BTool_" + brush.name
                    """

    # Garbage Collector
    brush.select = True
    # bpy.ops.object.delete()


def GCollector(_obj):
    if isCanvas(_obj):
        BTRoot = False
        for mod in _obj.modifiers:
            if ("BTool_" in mod.name):
                BTRoot = True
                if mod.object is None:
                    _obj.modifiers.remove(mod)
        if not BTRoot:
            del _obj["BoolToolRoot"]


# Handle the callbacks when modifing things in the scene
@persistent
def HandleScene(scene):
    if bpy.data.objects.is_updated:
        for ob in bpy.data.objects:
            if ob.is_updated:
                GCollector(ob)


# ------------------ Bool Tool OPERATORS --------------------------------------

class BTool_DrawPolyBrush(Operator):
    bl_idname = "btool.draw_polybrush"
    bl_label = "Draw Poly Brush"
    bl_description = ("Draw Polygonal Mask, can be applied to Canvas > Brush or Directly\n"
                      "Note: ESC to Cancel, Enter to Apply, Right Click to erase the Lines")

    count = 0
    store_cont_draw = False

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def set_cont_draw(self, context, start=False):
        # store / restore GP continuous drawing (see T52321)
        scene = context.scene
        tool_settings = scene.tool_settings
        continuous = tool_settings.use_gpencil_continuous_drawing
        if start:
            self.store_cont_draw = continuous
            tool_settings.use_gpencil_continuous_drawing = True
        else:
            tool_settings.use_gpencil_continuous_drawing = self.store_cont_draw

    def modal(self, context, event):
        self.count += 1
        actObj = bpy.context.active_object
        if self.count == 1:
            actObj.select = True
            bpy.ops.gpencil.draw('INVOKE_DEFAULT', mode="DRAW_POLY")

        if event.type in {'RIGHTMOUSE'}:
            # use this to pass to the Grease Pencil eraser (see T52321)
            pass

        if event.type in {'RET', 'NUMPAD_ENTER'}:

            bpy.ops.gpencil.convert(type='POLY')
            self.set_cont_draw(context)

            for obj in context.selected_objects:
                if obj.type == "CURVE":
                    obj.name = "PolyDraw"
                    bpy.context.scene.objects.active = obj
                    bpy.ops.object.select_all(action='DESELECT')
                    obj.select = True
                    bpy.ops.object.convert(target="MESH")
                    bpy.ops.object.mode_set(mode='EDIT')
                    bpy.ops.mesh.select_all(action='SELECT')
                    bpy.ops.mesh.edge_face_add()
                    bpy.ops.mesh.flip_normals()
                    bpy.ops.object.mode_set(mode='OBJECT')
                    bpy.ops.object.origin_set(type='ORIGIN_CENTER_OF_MASS')
                    bpy.ops.object.modifier_add(type="SOLIDIFY")
                    for mod in obj.modifiers:
                        if mod.name == "Solidify":
                            mod.name = "BTool_PolyBrush"
                            mod.thickness = 1
                            mod.offset = 0
                    obj["BoolToolPolyBrush"] = True

                    bpy.ops.object.select_all(action='DESELECT')
                    bpy.context.scene.objects.active = actObj
                    bpy.context.scene.update()
                    actObj.select = True
                    obj.select = True

                    bpy.context.scene.grease_pencil.clear()
                    bpy.ops.gpencil.data_unlink()

            return {'FINISHED'}

        if event.type in {'ESC'}:
            bpy.ops.ed.undo()  # remove o Grease Pencil
            self.set_cont_draw(context)

            self.report({'INFO'},
                         "Draw Poly Brush: Operation Cancelled by User")
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.object:
            self.set_cont_draw(context, start=True)
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "No active object, could not finish")
            return {'CANCELLED'}


# Fast Transform
class BTool_FastTransform(Operator):
    bl_idname = "btool.fast_transform"
    bl_label = "Fast Transform"
    bl_description = "Enable Fast Transform"

    operator = StringProperty("")

    count = 0

    def modal(self, context, event):
        self.count += 1
        actObj = bpy.context.active_object
        useWire = bpy.context.user_preferences.addons[__name__].preferences.use_wire
        if self.count == 1:

            if isBrush(actObj) and actObj["BoolTool_FTransform"] == "True":
                EnableThisBrush(bpy.context, "False")
                if useWire:
                    actObj.draw_type = "WIRE"
                else:
                    actObj.draw_type = "BOUNDS"

            if self.operator == "Translate":
                bpy.ops.transform.translate('INVOKE_DEFAULT')
            if self.operator == "Rotate":
                bpy.ops.transform.rotate('INVOKE_DEFAULT')
            if self.operator == "Scale":
                bpy.ops.transform.resize('INVOKE_DEFAULT')

        if event.type == 'LEFTMOUSE':
            if isBrush(actObj):
                EnableThisBrush(bpy.context, "True")
                actObj.draw_type = "WIRE"
            return {'FINISHED'}

        if event.type in {'RIGHTMOUSE', 'ESC'}:
            if isBrush(actObj):
                EnableThisBrush(bpy.context, "True")
                actObj.draw_type = "WIRE"
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.object:
            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "No active object, could not finish")
            return {'CANCELLED'}


# -------------------  Bool Tool OPERATOR CLASSES --------------------------------------------------------

# Brush Operators --------------------------------------------

# Boolean Union Operator
class BTool_Union(Operator):
    bl_idname = "btool.boolean_union"
    bl_label = "Brush Union"
    bl_description = "This operator add a union brush to a canvas"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Operation(context, "UNION")
        return {'FINISHED'}


# Boolean Intersection Operator
class BTool_Inters(Operator):
    bl_idname = "btool.boolean_inters"
    bl_label = "Brush Intersection"
    bl_description = "This operator add a intersect brush to a canvas"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Operation(context, "INTERSECT")
        return {'FINISHED'}


# Boolean Difference Operator
class BTool_Diff(Operator):
    bl_idname = "btool.boolean_diff"
    bl_label = "Brush Difference"
    bl_description = "This operator add a difference brush to a canvas"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Operation(context, "DIFFERENCE")
        return {'FINISHED'}


# Boolean Slices Operator
class BTool_Slice(Operator):
    bl_idname = "btool.boolean_slice"
    bl_label = "Brush Slice"
    bl_description = "This operator add a intersect brush to a canvas"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Operation(context, "SLICE")
        return {'FINISHED'}


# Auto Boolean operators (maintainer Mikhail Rachinskiy) -------------------------------

class AutoBoolean:

    solver = EnumProperty(
            name="Boolean Solver",
            items=(('BMESH', "BMesh", "BMesh solver is faster, but less stable "
                                      "and cannot handle coplanar geometry"),
                   ('CARVE', "Carve", "Carve solver is slower, but more stable "
                                      "and can handle simple cases of coplanar geometry")),
            description="Specify solver for boolean operation",
            options={'SKIP_SAVE'},
            )

    def __init__(self):
        self.solver = bpy.context.user_preferences.addons[__name__].preferences.solver

    def objects_prepare(self):
        for ob in bpy.context.selected_objects:
            if ob.type != 'MESH':
                ob.select = False
        bpy.ops.object.make_single_user(object=True, obdata=True)
        bpy.ops.object.convert(target='MESH')

    def mesh_selection(self, ob, select_action):
        scene = bpy.context.scene
        obj = bpy.context.active_object

        scene.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')

        bpy.ops.mesh.reveal()
        bpy.ops.mesh.select_all(action=select_action)

        bpy.ops.object.mode_set(mode='OBJECT')
        scene.objects.active = obj

    def boolean_operation(self):
        obj = bpy.context.active_object
        obj.select = False
        obs = bpy.context.selected_objects

        self.mesh_selection(obj, 'DESELECT')
        for ob in obs:
            self.mesh_selection(ob, 'SELECT')
            self.boolean_mod(obj, ob, self.mode)
        obj.select = True

    def boolean_mod(self, obj, ob, mode, ob_delete=True):
        md = obj.modifiers.new("Auto Boolean", 'BOOLEAN')
        md.show_viewport = False
        md.operation = mode
        md.solver = self.solver
        md.object = ob

        bpy.ops.object.modifier_apply(modifier="Auto Boolean")
        if not ob_delete:
            return
        bpy.context.scene.objects.unlink(ob)
        bpy.data.objects.remove(ob)


class Auto_Union(AutoBoolean, Operator):
    bl_idname = "btool.auto_union"
    bl_label = "Union"
    bl_description = "Combine selected objects"
    bl_options = {'REGISTER', 'UNDO'}

    mode = 'UNION'

    def execute(self, context):
        self.objects_prepare()
        self.boolean_operation()
        return {'FINISHED'}


class Auto_Difference(AutoBoolean, Operator):
    bl_idname = "btool.auto_difference"
    bl_label = "Difference"
    bl_description = "Subtract selected objects from active object"
    bl_options = {'REGISTER', 'UNDO'}

    mode = 'DIFFERENCE'

    def execute(self, context):
        self.objects_prepare()
        self.boolean_operation()
        return {'FINISHED'}


class Auto_Intersect(AutoBoolean, Operator):
    bl_idname = "btool.auto_intersect"
    bl_label = "Intersect"
    bl_description = "Keep only intersecting geometry"
    bl_options = {'REGISTER', 'UNDO'}

    mode = 'INTERSECT'

    def execute(self, context):
        self.objects_prepare()
        self.boolean_operation()
        return {'FINISHED'}


class Auto_Slice(AutoBoolean, Operator):
    bl_idname = "btool.auto_slice"
    bl_label = "Slice"
    bl_description = ("Slice active object along the selected object\n"
                      "(can handle only two objects at a time)")
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        self.objects_prepare()

        scene = context.scene
        obj = context.active_object
        obj.select = False
        ob = context.selected_objects[0]

        self.mesh_selection(obj, 'DESELECT')
        self.mesh_selection(ob, 'SELECT')

        obj_copy = obj.copy()
        obj_copy.data = obj.data.copy()
        scene.objects.link(obj_copy)

        self.boolean_mod(obj, ob, 'DIFFERENCE', ob_delete=False)
        scene.objects.active = obj_copy
        self.boolean_mod(obj_copy, ob, 'INTERSECT')
        obj_copy.select = True

        return {'FINISHED'}


class Auto_Subtract(AutoBoolean, Operator):
    bl_idname = "btool.auto_subtract"
    bl_label = "Subtract"
    bl_description = ("Subtract selected object from active object, subtracted object not removed\n"
                      "(can handle only two objects at a time)")
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        self.objects_prepare()

        obj = context.active_object
        obj.select = False
        ob = context.selected_objects[0]

        self.mesh_selection(obj, 'DESELECT')
        self.mesh_selection(ob, 'SELECT')
        self.boolean_mod(obj, ob, 'DIFFERENCE', ob_delete=False)

        return {'FINISHED'}


# Utils Class ---------------------------------------------------------------

# Find the Brush Selected in Three View
class BTool_FindBrush(Operator):
    bl_idname = "btool.find_brush"
    bl_label = ""
    bl_description = "Find the selected brush"

    obj = StringProperty("")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        for ob in bpy.context.scene.objects:
            if (ob.name == self.obj):
                bpy.ops.object.select_all(action='TOGGLE')
                bpy.ops.object.select_all(action='DESELECT')
                bpy.context.scene.objects.active = ob
                ob.select = True
        return {'FINISHED'}


# Move The Modifier in The Stack Up or Down
class BTool_MoveStack(Operator):
    bl_idname = "btool.move_stack"
    bl_label = ""
    bl_description = "Move this Brush Up/Down in the Stack"

    modif = StringProperty("")
    direction = StringProperty("")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        if (self.direction == "UP"):
            bpy.ops.object.modifier_move_up(modifier=self.modif)
        if (self.direction == "DOWN"):
            bpy.ops.object.modifier_move_down(modifier=self.modif)
        return {'FINISHED'}


# Enable or Disable a Brush in the Three View
class BTool_EnableBrush(Operator):
    bl_idname = "btool.enable_brush"
    bl_label = ""
    bl_description = "Removes all BoolTool config assigned to it"

    thisObj = StringProperty("")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        # in this case is just one object but the function accept more than one at once
        EnableBrush(context, [self.thisObj], context.active_object)
        return {'FINISHED'}


# Enable or Disable a Brush Directly
class BTool_EnableThisBrush(Operator):
    bl_idname = "btool.enable_this_brush"
    bl_label = ""
    bl_description = "Toggles this brush"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        EnableThisBrush(context, "None")
        return {'FINISHED'}


# Enable or Disable a Brush Directly
class BTool_EnableFTransform(Operator):
    bl_idname = "btool.enable_ftransf"
    bl_label = ""
    bl_description = "Use Fast Transformations to improve speed"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        EnableFTransf(context)
        return {'FINISHED'}


# Other Operations -------------------------------------------------------

# Remove a Brush or a Canvas
class BTool_Remove(Operator):
    bl_idname = "btool.remove"
    bl_label = ""
    bl_description = "Removes all BoolTool config assigned to it"
    bl_options = {'UNDO'}

    thisObj = StringProperty("")
    Prop = StringProperty("")

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Remove(context, self.thisObj, self.Prop)
        return {'FINISHED'}


# Apply All to Canvas
class BTool_AllBrushToMesh(Operator):
    bl_idname = "btool.to_mesh"
    bl_label = "Apply All Canvas"
    bl_description = "Apply all brushes of this canvas"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        lists = bpy.context.selected_objects
        ApplyAll(context, lists)
        return {'FINISHED'}


# Apply This Brush to the Canvas
class BTool_BrushToMesh(Operator):
    bl_idname = "btool.brush_to_mesh"
    bl_label = "Apply this Brush to Canvas"
    bl_description = "Apply this brush to the canvas"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):

        if isBrush(context.active_object):
            return True
        else:
            return False

    def execute(self, context):
        ApplyThisBrush(context, bpy.context.active_object)
        return {'FINISHED'}


# TODO
# Apply This Brush To Mesh


# ------------------- MENU CLASSES ------------------------------

# 3Dview Header Menu
class BoolTool_Menu(Menu):
    bl_label = "BoolTool Operators"
    bl_idname = "OBJECT_MT_BoolTool_Menu"

    def draw(self, context):
        layout = self.layout

        layout.label("Auto Boolean:")
        layout.operator(Auto_Difference.bl_idname, icon="ROTACTIVE")
        layout.operator(Auto_Union.bl_idname, icon="ROTATECOLLECTION")
        layout.operator(Auto_Intersect.bl_idname, icon="ROTATECENTER")
        layout.operator(Auto_Slice.bl_idname, icon="ROTATECENTER")
        layout.operator(Auto_Subtract.bl_idname, icon="ROTACTIVE")
        layout.separator()

        layout.label("Brush Boolean:")
        layout.operator(BTool_Diff.bl_idname, icon="ROTACTIVE")
        layout.operator(BTool_Union.bl_idname, icon="ROTATECOLLECTION")
        layout.operator(BTool_Inters.bl_idname, icon="ROTATECENTER")
        layout.operator(BTool_Slice.bl_idname, icon="ROTATECENTER")

        if (isCanvas(context.active_object)):
            layout.separator()
            layout.operator(BTool_AllBrushToMesh.bl_idname, icon="MOD_LATTICE", text="Apply All")
            Rem = layout.operator(BTool_Remove.bl_idname, icon="CANCEL", text="Remove All")
            Rem.thisObj = ""
            Rem.Prop = "CANVAS"

        if (isBrush(context.active_object)):
            layout.separator()
            layout.operator(BTool_BrushToMesh.bl_idname, icon="MOD_LATTICE", text="Apply Brush")
            Rem = layout.operator(BTool_Remove.bl_idname, icon="CANCEL", text="Remove Brush")
            Rem.thisObj = ""
            Rem.Prop = "BRUSH"


def VIEW3D_BoolTool_Menu(self, context):
    self.layout.menu(BoolTool_Menu.bl_idname)


# ---------------- Toolshelf: Tools ---------------------

class BoolTool_Tools(Panel):
    bl_category = "Tools"
    bl_label = "Bool Tools"
    bl_idname = "BoolTool_Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = 'objectmode'

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def draw(self, context):
        layout = self.layout
        obj = context.active_object
        obs_len = len(context.selected_objects)

        row = layout.split(0.7)
        row.label("Bool Tools:")
        row.operator("help.bool_tool", text="", icon="QUESTION")

        main = layout.column(align=True)
        main.enabled = obj.type == 'MESH' and obs_len > 0

        main.separator()

        col = main.column(align=True)
        col.enabled = obs_len > 1
        col.label("Auto Boolean:", icon="MODIFIER")
        col.separator()
        col.operator(Auto_Difference.bl_idname, icon="ROTACTIVE")
        col.operator(Auto_Union.bl_idname, icon="ROTATECOLLECTION")
        col.operator(Auto_Intersect.bl_idname, icon="ROTATECENTER")

        main.separator()

        col = main.column(align=True)
        col.enabled = obs_len == 2
        col.operator(Auto_Slice.bl_idname, icon="ROTATECENTER")
        col.operator(Auto_Subtract.bl_idname, icon="ROTACTIVE")

        main.separator()

        col = main.column(align=True)
        col.enabled = obs_len > 1
        col.label("Brush Boolean:", icon="MODIFIER")
        col.separator()
        col.operator(BTool_Diff.bl_idname, text="Difference", icon="ROTACTIVE")
        col.operator(BTool_Union.bl_idname, text="Union", icon="ROTATECOLLECTION")
        col.operator(BTool_Inters.bl_idname, text="Intersect", icon="ROTATECENTER")
        col.operator(BTool_Slice.bl_idname, text="Slice", icon="ROTATECENTER")

        main.separator()

        col = main.column(align=True)
        col.label("Draw:", icon="MESH_CUBE")
        col.separator()
        col.operator(BTool_DrawPolyBrush.bl_idname, icon="LINE_DATA")


# ---------- Toolshelf: Properties --------------------------------------------------------

class BoolTool_Config(Panel):
    bl_category = "Tools"
    bl_label = "Properties"
    bl_idname = "BoolTool_BConfig"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"

    @classmethod
    def poll(cls, context):

        result = False
        actObj = bpy.context.active_object
        if (isCanvas(actObj) or isBrush(actObj) or isPolyBrush(actObj)):
            result = True
        return result

    def draw(self, context):
        actObj = bpy.context.active_object
        icon = ""

        layout = self.layout
        row = layout.row(True)

        # CANVAS ---------------------------------------------------
        if isCanvas(actObj):
            row.label("CANVAS", icon="MESH_GRID")
            row = layout.row()
            row.prop(context.scene, 'BoolHide', text="Hide Bool objects")
            row = layout.row(True)
            row.operator(BTool_AllBrushToMesh.bl_idname, icon="MOD_LATTICE", text="Apply All")

            row = layout.row(True)
            Rem = row.operator(BTool_Remove.bl_idname, icon="CANCEL", text="Remove All")
            Rem.thisObj = ""
            Rem.Prop = "CANVAS"

            if isBrush(actObj):
                layout.separator()

        # BRUSH ------------------------------------------------------
        if isBrush(actObj):

            if (actObj["BoolToolBrush"] == "UNION"):
                icon = "ROTATECOLLECTION"
            if (actObj["BoolToolBrush"] == "DIFFERENCE"):
                icon = "ROTATECENTER"
            if (actObj["BoolToolBrush"] == "INTERSECT"):
                icon = "ROTACTIVE"
            if (actObj["BoolToolBrush"] == "SLICE"):
                icon = "ROTATECENTER"

            row = layout.row(True)
            row.label("BRUSH", icon=icon)

            icon = ""
            if actObj["BoolTool_FTransform"] == "True":
                icon = "PMARKER_ACT"
            else:
                icon = "PMARKER"
            if isFTransf():
                pass

            if isFTransf():
                row = layout.row(True)
                row.operator(BTool_EnableFTransform.bl_idname, text="Fast Vis", icon=icon)
                row.operator(BTool_EnableThisBrush.bl_idname, text="Enable", icon="VISIBLE_IPO_ON")
                row = layout.row(True)
            else:
                row.operator(BTool_EnableThisBrush.bl_idname, icon="VISIBLE_IPO_ON")
                row = layout.row(True)

        if isPolyBrush(actObj):
            row = layout.row(False)
            row.label("POLY BRUSH", icon="LINE_DATA")
            mod = actObj.modifiers["BTool_PolyBrush"]
            row = layout.row(False)
            row.prop(mod, "thickness", text="Size")
            layout.separator()

        if isBrush(actObj):
            row = layout.row(True)
            row.operator(BTool_BrushToMesh.bl_idname, icon="MOD_LATTICE", text="Apply Brush")
            row = layout.row(True)
            Rem = row.operator(BTool_Remove.bl_idname, icon="CANCEL", text="Remove Brush")
            Rem.thisObj = ""
            Rem.Prop = "BRUSH"

        layout.separator()


# ---------- Toolshelf: Brush Viewer -------------------------------------------------------

class BoolTool_BViwer(Panel):
    bl_label = "Brush Viewer"
    bl_idname = "BoolTool_BViwer"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Tools"
    bl_context = "objectmode"

    @classmethod
    def poll(cls, context):
        actObj = bpy.context.active_object

        if isCanvas(actObj):
            return True
        else:
            return False

    def draw(self, context):

        actObj = bpy.context.active_object
        icon = ""

        if isCanvas(actObj):

            for mod in actObj.modifiers:
                container = self.layout.box()
                row = container.row(True)
                icon = ""
                if ("BTool_" in mod.name):
                    if (mod.operation == "UNION"):
                        icon = "ROTATECOLLECTION"
                    if (mod.operation == "DIFFERENCE"):
                        icon = "ROTATECENTER"
                    if (mod.operation == "INTERSECT"):
                        icon = "ROTACTIVE"
                    if (mod.operation == "SLICE"):
                        icon = "ROTATECENTER"

                    objSelect = row.operator("btool.find_brush", text=mod.object.name, icon=icon, emboss=False)
                    objSelect.obj = mod.object.name

                    EnableIcon = "RESTRICT_VIEW_ON"
                    if (mod.show_viewport):
                        EnableIcon = "RESTRICT_VIEW_OFF"
                    Enable = row.operator(BTool_EnableBrush.bl_idname, icon=EnableIcon, emboss=False)
                    Enable.thisObj = mod.object.name

                    Remove = row.operator("btool.remove", icon="CANCEL", emboss=False)
                    Remove.thisObj = mod.object.name
                    Remove.Prop = "THIS"

                    # Stack Changer
                    Up = row.operator("btool.move_stack", icon="TRIA_UP", emboss=False)
                    Up.modif = mod.name
                    Up.direction = "UP"

                    Dw = row.operator("btool.move_stack", icon="TRIA_DOWN", emboss=False)
                    Dw.modif = mod.name
                    Dw.direction = "DOWN"

                else:
                    row.label(mod.name)
                    # Stack Changer
                    Up = row.operator("btool.move_stack", icon="TRIA_UP", emboss=False)
                    Up.modif = mod.name
                    Up.direction = "UP"

                    Dw = row.operator("btool.move_stack", icon="TRIA_DOWN", emboss=False)
                    Dw.modif = mod.name
                    Dw.direction = "DOWN"


# ------------------ BOOL TOOL Help ----------------------------
class BoolTool_help(Operator):
    bl_idname = "help.bool_tool"
    bl_label = "Help"
    bl_description = "Tool Help - click to read some basic information"

    def draw(self, context):
        layout = self.layout
        layout.label("To use:")
        layout.label("Select two or more objects,")
        layout.label("choose one option from the panel")
        layout.label("or from the Ctrl + Shift + B menu")

        layout.label("Auto Boolean:")
        layout.label("Apply Boolean operation directly.")

        layout.label("Brush Boolean:")
        layout.label("Create a Boolean brush setup.")

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_popup(self, width=220)


# ------------------ BOOL TOOL ADD-ON PREFERENCES ----------------------------

def UpdateBoolTool_Pref(self, context):
    if self.fast_transform:
        RegisterFastT()
    else:
        UnRegisterFastT()


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        BoolTool_Tools,
        BoolTool_Config,
        BoolTool_BViwer,
        )


def update_panel(self, context):
    message = "Bool Tool: Updating Panel locations has failed"
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


class BoolTool_Pref(AddonPreferences):
    bl_idname = __name__

    fast_transform = BoolProperty(
            name="Fast Transformations",
            default=False,
            update=UpdateBoolTool_Pref,
            description="Replace the Transform HotKeys (G,R,S)\n"
                        "for a custom version that can optimize the visualization of Brushes",
            )
    make_vertex_groups = BoolProperty(
            name="Make Vertex Groups",
            default=False,
            description="When Applying a Brush to the Object it will create\n"
                        "a new vertex group for the new faces",
            )
    make_boundary = BoolProperty(
            name="Make Boundary",
            default=False,
            description="When Apply a Brush to the Object it will create a\n"
                        "new vertex group of the bondary boolean area",
            )
    use_wire = BoolProperty(
            name="Use Bmesh",
            default=False,
            description="Use The Wireframe Instead of Bounding Box for visualization",
            )
    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="Tools",
            update=update_panel,
            )
    solver = EnumProperty(
            name="Boolean Solver",
            items=(('BMESH', "BMesh", "BMesh solver is faster, but less stable "
                                      "and cannot handle coplanar geometry"),
                   ('CARVE', "Carve", "Carve solver is slower, but more stable "
                                      "and can handle simple cases of coplanar geometry")),
            default='BMESH',
            description="Specify solver for boolean operations",
            )
    Enable_Tab_01 = BoolProperty(
            default=False
            )

    def draw(self, context):
        layout = self.layout
        split_percent = 0.3

        split = layout.split(percentage=split_percent)
        col = split.column()
        col.label(text="Tab Category:")
        col = split.column()
        colrow = col.row()
        colrow.prop(self, "category", text="")

        split = layout.split(percentage=split_percent)
        col = split.column()
        col.label("Boolean Solver:")
        col = split.column()
        colrow = col.row()
        colrow.prop(self, "solver", expand=True)

        split = layout.split(percentage=split_percent)
        col = split.column()
        col.label("Experimental Features:")
        col = split.column()
        colrow = col.row(align=True)
        colrow.prop(self, "fast_transform", toggle=True)
        colrow.prop(self, "use_wire", text="Use Wire Instead Of Bbox", toggle=True)
        layout.separator()
        """
        # EXPERIMENTAL
        col.prop(self, "make_vertex_groups")
        col.prop(self, "make_boundary")
        """
        layout.prop(self, "Enable_Tab_01", text="Hot Keys", icon="KEYINGSET")
        if self.Enable_Tab_01:
            row = layout.row()

            col = row.column()
            col.label("Hotkey List:")
            col.label("Menu: Ctrl Shift B")

            row = layout.row()
            col = row.column()
            col.label("Brush Operators:")
            col.label("Union: Ctrl Num +")
            col.label("Diff: Ctrl Num -")
            col.label("Intersect: Ctrl Num *")
            col.label("Slice: Ctrl Num /")

            row = layout.row()
            col = row.column()
            col.label("Auto Operators:")
            col.label("Difference: Ctrl Shift Num -")
            col.label("Union: Ctrl Shift Num +")
            col.label("Intersect: Ctrl Shift Num *")
            col.label("Slice: Ctrl Shift Num /")
            col.label("BTool Brush To Mesh: Ctrl Num Enter")
            col.label("BTool All Brush To Mesh: Ctrl Shift Num Enter")


# ------------------- Class List ------------------------------------------------

classes = (
    BoolTool_Pref,
    BoolTool_Menu,
    BoolTool_Tools,
    BoolTool_Config,
    BoolTool_BViwer,

    Auto_Union,
    Auto_Difference,
    Auto_Intersect,
    Auto_Slice,
    Auto_Subtract,

    BTool_Union,
    BTool_Diff,
    BTool_Inters,
    BTool_Slice,
    BTool_DrawPolyBrush,
    BTool_Remove,
    BTool_AllBrushToMesh,
    BTool_BrushToMesh,
    BTool_FindBrush,
    BTool_MoveStack,
    BTool_EnableBrush,
    BTool_EnableThisBrush,
    BTool_EnableFTransform,
    BTool_FastTransform,

    BoolTool_help,
    )


# ------------------- REGISTER ------------------------------------------------

addon_keymaps = []
addon_keymapsFastT = []


# Fast Transform HotKeys Register
def RegisterFastT():
    wm = bpy.context.window_manager
    km = wm.keyconfigs.addon.keymaps.new(name='Object Mode', space_type='EMPTY')

    kmi = km.keymap_items.new(BTool_FastTransform.bl_idname, 'G', 'PRESS')
    kmi.properties.operator = "Translate"
    addon_keymapsFastT.append((km, kmi))

    kmi = km.keymap_items.new(BTool_FastTransform.bl_idname, 'R', 'PRESS')
    kmi.properties.operator = "Rotate"
    addon_keymapsFastT.append((km, kmi))

    kmi = km.keymap_items.new(BTool_FastTransform.bl_idname, 'S', 'PRESS')
    kmi.properties.operator = "Scale"
    addon_keymapsFastT.append((km, kmi))


# Fast Transform HotKeys UnRegister
def UnRegisterFastT():
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        for km, kmi in addon_keymapsFastT:
            km.keymap_items.remove(kmi)

    addon_keymapsFastT.clear()


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    update_panel(None, bpy.context)

    # Scene variables
    bpy.types.Scene.BoolHide = BoolProperty(
            default=False,
            description="Hide boolean objects",
            update=update_BoolHide,
            )
    # Handlers
    bpy.app.handlers.scene_update_post.append(HandleScene)

    bpy.types.VIEW3D_MT_object.append(VIEW3D_BoolTool_Menu)
    try:
        bpy.types.VIEW3D_MT_Object.prepend(VIEW3D_BoolTool_Menu)
    except:
        pass

    wm = bpy.context.window_manager

    # create the boolean menu hotkey
    km = wm.keyconfigs.addon.keymaps.new(name='Object Mode')

    kmi = km.keymap_items.new('wm.call_menu', 'B', 'PRESS', ctrl=True, shift=True)
    kmi.properties.name = 'OBJECT_MT_BoolTool_Menu'
    addon_keymaps.append((km, kmi))

    # Brush Operators
    kmi = km.keymap_items.new(BTool_Union.bl_idname, 'NUMPAD_PLUS', 'PRESS', ctrl=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(BTool_Diff.bl_idname, 'NUMPAD_MINUS', 'PRESS', ctrl=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(BTool_Inters.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', ctrl=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(BTool_Slice.bl_idname, 'NUMPAD_SLASH', 'PRESS', ctrl=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(BTool_BrushToMesh.bl_idname, 'NUMPAD_ENTER', 'PRESS', ctrl=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(BTool_AllBrushToMesh.bl_idname, 'NUMPAD_ENTER', 'PRESS', ctrl=True, shift=True)
    addon_keymaps.append((km, kmi))

    # Auto Operators
    kmi = km.keymap_items.new(Auto_Union.bl_idname, 'NUMPAD_PLUS', 'PRESS', ctrl=True, shift=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(Auto_Difference.bl_idname, 'NUMPAD_MINUS', 'PRESS', ctrl=True, shift=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(Auto_Intersect.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', ctrl=True, shift=True)
    addon_keymaps.append((km, kmi))
    kmi = km.keymap_items.new(Auto_Slice.bl_idname, 'NUMPAD_SLASH', 'PRESS', ctrl=True, shift=True)
    addon_keymaps.append((km, kmi))


def unregister():
    # Keymapping
    # remove keymaps when add-on is deactivated
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc:
        for km, kmi in addon_keymaps:
            km.keymap_items.remove(kmi)

    addon_keymaps.clear()
    UnRegisterFastT()

    bpy.types.VIEW3D_MT_object.remove(VIEW3D_BoolTool_Menu)
    try:
        bpy.types.VIEW3D_MT_Object.remove(VIEW3D_BoolTool_Menu)
    except:
        pass

    del bpy.types.Scene.BoolHide

    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
