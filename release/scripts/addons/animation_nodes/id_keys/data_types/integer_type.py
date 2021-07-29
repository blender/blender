import bpy
import random
from bpy.props import *
from mathutils import Vector
from . base import SingleIDKeyDataType
from ... data_structures import LongList
from ... algorithms.lists import naturalSortKey
from ... utils.blender_ui import getDpiFactor, redrawAll
from ... utils.selection import getSortedSelectedObjects

class IntegerDataType(SingleIDKeyDataType):
    identifier = "Integer"
    default = 0

    @classmethod
    def getList(cls, objects, name):
        default = cls.default
        path = cls.getPath(name)
        return LongList.fromValues(getattr(object, path, default) for object in objects)

    @classmethod
    def drawExtras(cls, layout, object, name):
        props = layout.operator("an.id_keys_from_sorted_objects", text = "Sort Objects", icon = "SORTSIZE")
        props.idKeyName = name

    @classmethod
    def drawCopyMenu(cls, layout, object, name):
        props = layout.operator("an.copy_id_key_to_attribute", "to Pass Index")
        props.dataType = "Integer"
        props.propertyName = name
        props.attribute = "pass_index"



# Sorting
###########################################

sortModeItems = [
    ("SELECTION_ORDER", "Selection Order", "", "BORDER_RECT", 0),
    ("NAME", "Object Name", "Sort objects alphanumerically", "SORTALPHA", 1),
    ("DISTANCE", "Distance", "Sort by distance to active object", "FULLSCREEN_ENTER", 2),
    ("RANDOM", "Random", "", "MOD_PARTICLES", 3),
    ("AXIS", "Axis", "", "MANIPUL", 4)
]

axisItems = [(axis, axis, "") for axis in ("X", "Y", "Z")]

locationModeItems = [
    ("ORIGIN", "Object Origin", ""),
    ("BOUDING_BOX_CENTER", "Bounding Box Center", "")
]

class IDKeysFromSortedObjects(bpy.types.Operator):
    bl_idname = "an.id_keys_from_sorted_objects"
    bl_label = "ID Keys from Sorted Objects"
    bl_description = "Assign ID Keys based on the selected sorting method."

    idKeyName = StringProperty()
    sortMode = EnumProperty(name = "Sorting Method", default = "SELECTION_ORDER",
        items = sortModeItems)

    offset = IntProperty(name = "Offset", default = 0)
    reverse = BoolProperty(name = "Reverse", default = False)

    axis = EnumProperty(name = "Axis", default = "X", items = axisItems)
    threshold = FloatProperty(name = "Threshold", default = 0.01,
        description = "Objects with similar location should get the same index")
    locationMode = EnumProperty(name = "Location Mode", default = "ORIGIN",
        items = locationModeItems)

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width = 250 * getDpiFactor())

    def draw(self, context):
        layout = self.layout
        layout.column().prop(self, "sortMode", text = "Method", expand = True)
        layout.separator()

        if self.sortMode == "SELECTION_ORDER":
            pass
        elif self.sortMode == "AXIS":
            layout.prop(self, "axis")
            layout.prop(self, "locationMode", text = "Location")
            layout.prop(self, "threshold")
        elif self.sortMode == "DISTANCE":
            if context.active_object is None:
                layout.label("No active object.", icon = "INFO")
            else:
                layout.prop(self, "locationMode", text = "Location")
                layout.prop(self, "threshold")
        elif self.sortMode == "RANDOM":
            pass
        elif self.sortMode == "NAME":
            pass

        layout.prop(self, "reverse")
        layout.prop(self, "offset")

    def check(self, context):
        return True

    def execute(self, context):
        if self.sortMode == "SELECTION_ORDER":
            iterSortedObjects = self.sort_SelectionOrder
        elif self.sortMode == "AXIS":
            iterSortedObjects = self.sort_Axis
        elif self.sortMode == "DISTANCE":
            iterSortedObjects = self.sort_Distance
        elif self.sortMode == "RANDOM":
            iterSortedObjects = self.sort_Random
        elif self.sortMode == "NAME":
            iterSortedObjects = self.sort_Name

        sortedObjects = list(iterSortedObjects())
        for i, objects in enumerate(sortedObjects):
            if not isinstance(objects, (list, tuple)):
                objects = [objects]
            for object in objects:
                if self.reverse:
                    index = len(sortedObjects) - i - 1
                else:
                    index = i
                object.id_keys.set("Integer", self.idKeyName, index + self.offset)

        redrawAll()
        return {"FINISHED"}

    def sort_SelectionOrder(self):
        return getSortedSelectedObjects()

    def sort_Axis(self):
        index = ["X", "Y", "Z"].index(self.axis)
        return self.sort_ByFunction(lambda x: self.getObjectLocation(x)[index])

    def sort_Distance(self):
        reference = bpy.context.active_object
        if reference is None:
            return []
        location = self.getObjectLocation(reference)
        return self.sort_ByFunction(lambda x: (self.getObjectLocation(x) - location).length)

    def sort_Random(self):
        objects = list(bpy.context.selected_objects)
        random.seed()
        random.shuffle(objects)
        return objects

    def sort_Name(self):
        return sorted(bpy.context.selected_objects, key = lambda x: naturalSortKey(x.name))

    def sort_ByFunction(self, keyFunc):
        sortedObjects = []
        threshold = self.threshold

        for object in sorted(bpy.context.selected_objects, key = keyFunc):
            if len(sortedObjects) == 0:
                sortedObjects.append([object])
            elif abs(keyFunc(sortedObjects[-1][0]) - keyFunc(object)) < threshold:
                sortedObjects[-1].append(object)
            else:
                sortedObjects.append([object])

        return sortedObjects

    def getObjectLocation(self, object):
        if self.locationMode == "ORIGIN":
            return object.location
        elif self.locationMode == "BOUDING_BOX_CENTER":
            p1 = Vector(object.bound_box[0])
            p2 = Vector(object.bound_box[6])
            return object.matrix_world * ((p1 + p2) / 2)
