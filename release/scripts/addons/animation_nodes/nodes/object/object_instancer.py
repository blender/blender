import bpy
from bpy.props import *
from ... events import propertyChanged
from ... base_types import AnimationNode
from ... utils.names import getRandomString
from ... utils.blender_ui import iterActiveSpacesByType
from ... utils.data_blocks import removeNotUsedDataBlock
from ... nodes.container_provider import getMainObjectContainer
from ... utils.names import (getPossibleMeshName,
                             getPossibleCameraName,
                             getPossibleLampName,
                             getPossibleCurveName)

lastSourceHashes = {}
lastSceneHashes = {}

objectTypeItems = [
    ("Mesh", "Mesh", "", "MESH_DATA", 0),
    ("Text", "Text", "", "FONT_DATA", 1),
    ("Camera", "Camera", "", "CAMERA_DATA", 2),
    ("Point Lamp", "Point Lamp", "", "LAMP_POINT", 3),
    ("Curve 2D", "Curve 2D", "", "FORCE_CURVE", 4),
    ("Curve 3D", "Curve 3D", "", "CURVE_DATA", 5),
    ("Empty", "Empty", "", "EMPTY_DATA", 6) ]

emptyDrawTypeItems = []
for item in bpy.types.Object.bl_rna.properties["empty_draw_type"].enum_items:
    emptyDrawTypeItems.append((item.identifier, item.name, ""))

class ObjectNamePropertyGroup(bpy.types.PropertyGroup):
    bl_idname = "an_ObjectNamePropertyGroup"
    objectName = StringProperty(name = "Object Name", default = "", update = propertyChanged)
    objectIndex = IntProperty(name = "Object Index", default = 0, update = propertyChanged)

class ObjectInstancerNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectInstancerNode"
    bl_label = "Object Instancer"
    bl_width_default = 160
    options = {"NOT_IN_SUBPROGRAM"}

    def copyFromSourceChanged(self, context):
        self.refresh()
        self.resetInstancesEvent(context)

    def resetInstancesEvent(self, context):
        self.resetInstances = True
        propertyChanged()

    linkedObjects = CollectionProperty(type = ObjectNamePropertyGroup)
    resetInstances = BoolProperty(default = False, update = propertyChanged)

    copyFromSource = BoolProperty(name = "Copy from Source",
        default = True, update = copyFromSourceChanged)

    deepCopy = BoolProperty(name = "Deep Copy", default = False, update = resetInstancesEvent,
        description = "Make the instances independent of the source object (e.g. copy mesh)")

    objectType = EnumProperty(name = "Object Type", default = "Mesh",
        items = objectTypeItems, update = resetInstancesEvent)

    copyObjectProperties = BoolProperty(name = "Copy Full Object", default = False,
        description = "Enable this to copy modifiers/constraints/... from the source object.",
        update = resetInstancesEvent)

    removeAnimationData = BoolProperty(name = "Remove Animation Data", default = True,
        description = "Remove the active action on the instance; This is useful when you want to animate the object yourself",
        update = resetInstancesEvent)

    parentInstances = BoolProperty(name = "Parent to Main Container",
        default = True, update = resetInstancesEvent)

    emptyDrawType = EnumProperty(name = "Empty Draw Type", default = "PLAIN_AXES",
        items = emptyDrawTypeItems, update = resetInstancesEvent)

    def create(self):
        self.newInput("Integer", "Instances", "instancesAmount", minValue = 0)
        if self.copyFromSource:
            self.newInput("Object", "Source", "sourceObject",
                defaultDrawType = "PROPERTY_ONLY", showHideToggle = True)
        self.newInput("Scene List", "Scenes", "scenes", hide = True)

        self.newOutput("an_ObjectListSocket", "Objects", "objects")

    def draw(self, layout):
        layout.prop(self, "copyFromSource")
        if self.copyFromSource:
            layout.prop(self, "copyObjectProperties", text = "Copy Full Object")
            layout.prop(self, "deepCopy")
        else:
            layout.prop(self, "objectType", text = "")
            if self.objectType == "Empty":
                layout.prop(self, "emptyDrawType", text = "")

    def drawAdvanced(self, layout):
        layout.prop(self, "parentInstances")
        layout.prop(self, "removeAnimationData")

        self.invokeFunction(layout, "resetObjectDataOnAllInstances",
            text = "Reset Source Data",
            description = "Reset the source data on all instances")
        self.invokeFunction(layout, "unlinkInstancesFromNode",
            confirm = True,
            text = "Unlink Instances from Node",
            description = "This will make sure that the objects won't be removed if you remove the Instancer Node.")

        layout.separator()
        self.invokeFunction(layout, "hideRelationshipLines",
            text = "Hide Relationship Lines",
            icon = "RESTRICT_VIEW_OFF")

    def getExecutionCode(self):
        # support for older nodes which didn't have a scene list input
        if "Scenes" in self.inputs: yield "_scenes = set(scenes)"
        else: yield "_scenes = {scene}"

        if self.copyFromSource:
            yield "objects = self.getInstances_WithSource(instancesAmount, sourceObject, _scenes)"
        else:
            yield "objects = self.getInstances_WithoutSource(instancesAmount, _scenes)"

    def getInstances_WithSource(self, instancesAmount, sourceObject, scenes):
        if sourceObject is None:
            self.removeAllObjects()
            return []
        else:
            sourceHash = hash(sourceObject)
            if self.identifier in lastSourceHashes:
                if lastSourceHashes[self.identifier] != sourceHash:
                    self.removeAllObjects()
            lastSourceHashes[self.identifier] = sourceHash

        return self.getInstances_Base(instancesAmount, sourceObject, scenes)

    def getInstances_WithoutSource(self, instancesAmount, scenes):
        return self.getInstances_Base(instancesAmount, None, scenes)

    def getInstances_Base(self, instancesAmount, sourceObject, scenes):
        instancesAmount = max(instancesAmount, 0)

        if not any(scenes):
            self.removeAllObjects()
            return []
        else:
            sceneHash = set(hash(scene) for scene in scenes)
            if self.identifier in lastSceneHashes:
                if lastSceneHashes[self.identifier] != sceneHash:
                    self.removeAllObjects()
            lastSceneHashes[self.identifier] = sceneHash

        if self.resetInstances:
            self.removeAllObjects()
            self.resetInstances = False

        while instancesAmount < len(self.linkedObjects):
            self.removeLastObject()

        return self.getOutputObjects(instancesAmount, sourceObject, scenes)


    def getOutputObjects(self, instancesAmount, sourceObject, scenes):
        objects = []

        self.updateFastListAccess()

        indicesToRemove = []
        for i, item in enumerate(self.linkedObjectsList):
            object = self.getObjectFromItem(item)
            if object is None: indicesToRemove.append(i)
            else: objects.append(object)

        self.removeObjectFromItemIndices(indicesToRemove)

        missingAmount = instancesAmount - len(objects)
        newObjects = self.createNewObjects(missingAmount, sourceObject, scenes)
        objects.extend(newObjects)

        return objects

    def updateFastListAccess(self):
        self.linkedObjectsList = list(self.linkedObjects)
        self.objectList = list(bpy.data.objects)
        self.objectNameList = None

    # at first try to get the object by index, because it's faster and then search by name
    def getObjectFromItem(self, item):
        if item.objectIndex < len(self.objectList):
            object = self.objectList[item.objectIndex]
            if object.name == item.objectName:
                return object

        try:
            if self.objectNameList is None:
                self.objectNameList = list(bpy.data.objects.keys())
            index = self.objectNameList.index(item.objectName)
            item.objectIndex = index
            return self.objectList[index]
        except:
            return None

    def removeAllObjects(self):
        objectNames = []
        for item in self.linkedObjects:
            objectNames.append(item.objectName)

        for name in objectNames:
            object = bpy.data.objects.get(name)
            if object is not None:
                self.removeObject(object)

        self.linkedObjects.clear()

    def removeLastObject(self):
        self.removeObjectFromItemIndex(len(self.linkedObjects)-1)

    def removeObjectFromItemIndices(self, indices):
        for offset, index in enumerate(indices):
            self.removeObjectFromItemIndex(index - offset)

    def removeObjectFromItemIndex(self, itemIndex):
        item = self.linkedObjects[itemIndex]
        objectName = item.objectName
        self.linkedObjects.remove(itemIndex)
        object = bpy.data.objects.get(objectName)
        if object is not None:
            self.removeObject(object)

    def removeObject(self, object):
        self.unlinkInstance(object)
        if object.users == 0:
            data = object.data
            type = object.type
            self.removeShapeKeys(object)
            bpy.data.objects.remove(object)
            self.removeObjectData(data, type)

    def removeObjectData(self, data, type):
        if data is None: return # the object was an empty
        if data.an_data.removeOnZeroUsers and data.users == 0:
            removeNotUsedDataBlock(data, type)

    def removeShapeKeys(self, object):
        # don't remove the shape key if it is used somewhere else
        if object.type not in ("MESH", "CURVE", "LATTICE"): return
        if object.data.shape_keys is None: return
        if object.data.shape_keys.user.users > 1: return

        object.active_shape_key_index = 0
        while object.active_shape_key is not None:
            object.shape_key_remove(object.active_shape_key)


    def createNewObjects(self, amount, sourceObject, scenes):
        objects = []
        nameSuffix = "instance_{}_".format(getRandomString(5))
        for i in range(amount):
            name = nameSuffix + str(i)
            newObject = self.appendNewObject(name, sourceObject, scenes)
            objects.append(newObject)
        return objects

    def appendNewObject(self, name, sourceObject, scenes):
        object = self.newInstance(name, sourceObject, scenes)
        for scene in scenes:
            if scene is not None: scene.objects.link(object)
        linkedItem = self.linkedObjects.add()
        linkedItem.objectName = object.name
        linkedItem.objectIndex = bpy.data.objects.find(object.name)
        return object

    def newInstance(self, name, sourceObject, scenes):
        instanceData = self.getSourceObjectData(sourceObject)
        if self.copyObjectProperties and self.copyFromSource:
            newObject = sourceObject.copy()
            newObject.data = instanceData
        else:
            newObject = self.createObject(name, instanceData)

        if self.parentInstances:
            for scene in scenes:
                if scene is not None:
                    newObject.parent = getMainObjectContainer(scene)
                    break
        if self.removeAnimationData and newObject.animation_data is not None:
            newObject.animation_data.action = None
        newObject.select = False
        newObject.hide = False
        newObject.hide_render = False
        if not self.copyFromSource and self.objectType == "Empty":
            newObject.empty_draw_type = self.emptyDrawType
        return newObject

    def createObject(self, name, instanceData):
        return bpy.data.objects.new(name, instanceData)

    def getSourceObjectData(self, sourceObject):
        data = None
        if self.copyFromSource:
            if self.deepCopy and sourceObject.data is not None:
                data = sourceObject.data.copy()
            else:
                return sourceObject.data
        else:
            if self.objectType == "Mesh":
                data = bpy.data.meshes.new(getPossibleMeshName("instance mesh"))
            elif self.objectType == "Text":
                data = bpy.data.curves.new(getPossibleCurveName("instance text"), type = "FONT")
            elif self.objectType == "Camera":
                data = bpy.data.cameras.new(getPossibleCameraName("instance camera"))
            elif self.objectType == "Point Lamp":
                data = bpy.data.lamps.new(getPossibleLampName("instance lamp"), type = "POINT")
            elif self.objectType.startswith("Curve"):
                data = bpy.data.curves.new(getPossibleCurveName("instance curve"), type = "CURVE")
                data.dimensions = self.objectType[-2:]

        if data is None:
            return None
        else:
            data.an_data.removeOnZeroUsers = True
            return data

    def unlinkInstance(self, object):
        if bpy.context.mode != "OBJECT" and bpy.context.active_object == object:
            bpy.ops.object.mode_set(mode = "OBJECT")
        for scene in bpy.data.scenes:
            if object.name in scene.objects:
                scene.objects.unlink(object)

    def resetObjectDataOnAllInstances(self):
        self.resetInstances = True

    def unlinkInstancesFromNode(self):
        self.linkedObjects.clear()
        self.inputs.get("Instances").number = 0

    def delete(self):
        self.removeAllObjects()

    def duplicate(self, sourceNode):
        self.linkedObjects.clear()

    def hideRelationshipLines(self):
        for space in iterActiveSpacesByType("VIEW_3D"):
            space.show_relationship_lines = False
