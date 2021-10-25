import bpy
from ... base_types import AnimationNode, VectorizedSocket

class ObjectTransformsInputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectTransformsInputNode"
    bl_label = "Object Transforms Input"
    bl_width_default = 160
    codeEffects = [VectorizedSocket.CodeEffect]

    useObjectList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects")))

        self.newOutput(VectorizedSocket("Vector", "useObjectList",
            ("Location", "location"), ("Locations", "locations")))

        self.newOutput(VectorizedSocket("Euler", "useObjectList",
            ("Rotation", "rotation"), ("Rotations", "rotations")))

        self.newOutput(VectorizedSocket("Vector", "useObjectList",
            ("Scale", "scale"), ("Scales", "scales")))

        self.newOutput(VectorizedSocket("Quaternion", "useObjectList",
            ("Quaternion", "quaternion", dict(hide = True)),
            ("Quaternions", "quaternions", dict(hide = True))))

    def drawAdvanced(self, layout):
        self.invokeFunction(layout, "createAutoExecutionTrigger", text = "Create Execution Trigger")

    def getExecutionCode(self, required):
        if len(required) == 0:
            return

        yield "if object is not None:"
        if "location" in required:   yield "    location = object.location"
        if "rotation" in required:   yield "    rotation = object.rotation_euler"
        if "scale" in required:      yield "    scale = object.scale"
        if "quaternion" in required: yield "    quaternion = object.rotation_quaternion"
        yield "else:"
        yield "    location = Vector((0, 0, 0))"
        yield "    rotation = Euler((0, 0, 0))"
        yield "    scale = Vector((1, 1, 1))"
        yield "    quaternion = Quaternion((1, 0, 0, 0))"

    def createAutoExecutionTrigger(self):
        isLinked = self.getLinkedOutputsDict()
        customTriggers = self.nodeTree.autoExecution.customTriggers

        objectName = self.inputs["Object"].objectName

        if isLinked["location"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "location", idObjectName = objectName)
        if isLinked["rotation"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "rotation_euler", idObjectName = objectName)
        if isLinked["scale"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "scale", idObjectName = objectName)
        if isLinked["quaternion"]:
            customTriggers.new("MONITOR_PROPERTY", idType = "OBJECT", dataPath = "rotation_quaternion", idObjectName = objectName)
