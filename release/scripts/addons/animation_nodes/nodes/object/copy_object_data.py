import bpy
from ... base_types import VectorizedNode
from ... utils.data_blocks import removeNotUsedDataBlock

class CopyObjectDataNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_CopyObjectDataNode"
    bl_label = "Copy Object Data"
    autoVectorizeExecution = True

    useFromList = VectorizedNode.newVectorizeProperty()
    useToList = VectorizedNode.newVectorizeProperty()

    def create(self):
        self.newVectorizedInput("Object", ("useFromList", ["useToList"]),
            ("From", "fromObject"), ("From", "fromObjects"))

        self.newVectorizedInput("Object", "useToList",
            ("To", "toObject"), ("To", "toObjects"))

        self.newVectorizedOutput("Object", "useToList",
            ("To", "outObject"), ("To", "outObjects"))

    def getExecutionCode(self):
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
