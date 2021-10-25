import bpy
from ... base_types import AnimationNode, VectorizedSocket
from ... utils.data_blocks import removeNotUsedDataBlock

class CopyObjectDataNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_CopyObjectDataNode"
    bl_label = "Copy Object Data"
    codeEffects = [VectorizedSocket.CodeEffect]

    useFromList = VectorizedSocket.newProperty()
    useToList = VectorizedSocket.newProperty()

    def create(self):
        self.newInput(VectorizedSocket("Object", ["useFromList", "useToList"],
            ("From", "fromObject"), ("From", "fromObjects")))

        self.newInput(VectorizedSocket("Object", "useToList",
            ("To", "toObject"), ("To", "toObjects"),
            codeProperties = dict(allowListExtension = False)))

        self.newOutput(VectorizedSocket("Object", "useToList",
            ("To", "outObject"), ("To", "outObjects")))

    def getExecutionCode(self, required):
        return "outObject = self.copyObjectData(fromObject, toObject)"

    def copyObjectData(self, fromObject, toObject):
        if fromObject is None or toObject is None: return toObject
        if toObject.data == fromObject.data: return toObject

        if toObject.type == fromObject.type:
            oldData = toObject.data
            toObject.data = fromObject.data

            if oldData.users == 0 and oldData.an_data.removeOnZeroUsers:
                removeNotUsedDataBlock(oldData, toObject.type)

        return toObject
