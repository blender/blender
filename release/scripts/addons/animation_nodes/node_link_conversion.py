import bpy
from . import tree_info
from mathutils import Vector
from . utils.nodes import idToSocket
from . tree_info import getAllDataLinkIDs, getDirectlyLinkedSocket
from . sockets.info import toBaseIdName, isList, getAllowedInputDataTypes

def correctForbiddenNodeLinks():
    allLinksCorrect = False
    while not allLinksCorrect:
        allLinksCorrect = correctNextForbiddenLink()
        tree_info.updateIfNecessary()

def correctNextForbiddenLink():
    dataOrigin, target = getNextInvalidLink()
    if dataOrigin is None: return True
    directOrigin = getDirectlyLinkedSocket(target)
    if not tryToCorrectLink(dataOrigin, directOrigin, target):
        removeLink(directOrigin, target)
    return False

approvedLinkTypes = set()

def getNextInvalidLink():
    for originID, targetID, originType, targetType in getAllDataLinkIDs():
        if (originType, targetType) in approvedLinkTypes:
            continue

        origin = idToSocket(originID)
        target = idToSocket(targetID)

        if isConnectionValid(origin, target):
            approvedLinkTypes.add((originType, targetType))
        else:
            return origin, target
    return None, None

def isConnectionValid(origin, target):
    return origin.dataType in getAllowedInputDataTypes(target.dataType)

def tryToCorrectLink(dataOrigin, directOrigin, target):
    for corrector in linkCorrectors:
        if corrector.check(dataOrigin, target):
            nodeTree = target.getNodeTree()
            corrector.insert(nodeTree, directOrigin, target, dataOrigin)
            return True
    return False

def removeLink(origin, target):
    nodeTree = origin.getNodeTree()
    for link in nodeTree.links:
        if link.from_socket == origin and link.to_socket == target:
            nodeTree.links.remove(link)


class LinkCorrection:
    # subclasses need a check and insert function
    pass

class SimpleConvert(LinkCorrection):
    rules = {
        ("Vector", "Matrix") : "an_TranslationMatrixNode",
        ("Text Block", "Text") : "an_TextBlockReaderNode",
        ("Vector", "Float") : "an_SeparateVectorNode",
        ("Float", "Vector") : "an_CombineVectorNode",
        ("Integer", "Vector") : "an_CombineVectorNode",
        ("Vector List", "Mesh Data") : "an_CombineMeshDataNode",
        ("Mesh Data", "Vector List") : "an_SeparateMeshDataNode",
        ("Mesh Data", "BMesh") : "an_CreateBMeshFromMeshDataNode",
        ("Integer", "Euler") : "an_CombineEulerNode",
        ("Float", "Euler") : "an_CombineEulerNode",
        ("Euler", "Float") : "an_SeparateEulerNode",
        ("Object", "Vector") : "an_ObjectTransformsInputNode",
        ("Object", "Matrix") : "an_ObjectMatrixInputNode",
        ("Object", "Shape Key List") : "an_ShapeKeysFromObjectNode",
        ("Text", "Float") : "an_ParseNumberNode",
        ("Text", "Integer") : "an_ParseNumberNode",
        ("Vector", "Euler") : "an_DirectionToRotationNode",
        ("Euler", "Vector") : "an_RotationToDirectionNode",
        ("Float", "Falloff") : "an_ConstantFalloffNode",
        ("Vector List", "Matrix List") : "an_TranslationMatrixNode",
        ("Vector List", "Spline") : "an_SplineFromPointsNode",
        ("Float List", "Falloff") : "an_CustomFalloffNode"
    }

    def check(self, origin, target):
        return (origin.dataType, target.dataType) in self.rules
    def insert(self, nodeTree, origin, target, dataOrigin):
        nodeIdName = self.rules[(dataOrigin.dataType, target.dataType)]
        node = insertLinkedNode(nodeTree, nodeIdName, origin, target)
        tree_info.updateIfNecessary()
        node.updateNode()

class ConvertMeshDataListToMeshData(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Mesh Data List" and target.dataType == "Mesh Data"
    def insert(self, nodeTree, origin, target, dataOrigin):
        insertLinkedNode(nodeTree, "an_JoinMeshDataListNode", origin, target)

class ConvertFloatToScale(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType in ("Float", "Integer") and target.dataType == "Vector" and "scale" in target.name.lower()
    def insert(self, nodeTree, origin, target, dataOrigin):
        insertLinkedNode(nodeTree, "an_VectorFromValueNode", origin, target)

class ConvertNormalToEuler(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Vector" and origin.name == "Normal" and target.dataType == "Euler"
    def insert(self, nodeTree, origin, target, dataOrigin):
        insertLinkedNode(nodeTree, "an_DirectionToRotationNode", origin, target)

class ConvertEulerToQuaternion(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Euler" and target.dataType == "Quaternion"
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_ConvertRotationsNode", origin, target)
        node.conversionType = "EULER_TO_QUATERNION"
        node.inputs[0].linkWith(origin)
        node.outputs[0].linkWith(target)

class ConvertQuaternionToEuler(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Quaternion" and target.dataType == "Euler"
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_ConvertRotationsNode", origin, target)
        node.conversionType = "QUATERNION_TO_EULER"
        node.inputs[0].linkWith(origin)
        node.outputs[0].linkWith(target)

class ConvertElementToList(LinkCorrection):
    def check(self, origin, target):
        return origin.bl_idname == toBaseIdName(target.bl_idname)
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertNode(nodeTree, "an_CreateListNode", origin, target)
        node.assignBaseDataType(dataOrigin.dataType, inputAmount = 1)
        insertBasicLinking(nodeTree, origin, node, target)

class ConvertObjectToShapeKey(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Object" and target.dataType == "Shape Key"
    def insert(self, nodeTree, origin, target, dataOrigin):
        getShapeKeys, getListElement = insertNodes(nodeTree, ["an_ShapeKeysFromObjectNode", "an_GetListElementNode"], origin, target)
        getListElement.inputs[1].value = 1
        getListElement.assignedType = "Shape Key"

        origin.linkWith(getShapeKeys.inputs[0])
        getShapeKeys.outputs[0].linkWith(getListElement.inputs[0])
        getListElement.outputs[0].linkWith(target)

class ConvertSeparatedMeshDataToBMesh(LinkCorrection):
    separatedMeshDataTypes = ["Vector List", "Edge Indices List", "Polygon Indices List"]
    def check(self, origin, target):
        return origin.dataType in self.separatedMeshDataTypes and target.dataType == "BMesh"
    def insert(self, nodeTree, origin, target, dataOrigin):
        toMeshData, toMesh = insertNodes(nodeTree, ["an_CombineMeshDataNode", "an_CreateBMeshFromMeshDataNode"], origin, target)
        nodeTree.links.new(toMeshData.inputs[self.separatedMeshDataTypes.index(origin.dataType)], origin)
        nodeTree.links.new(toMesh.inputs[0], toMeshData.outputs[0])
        nodeTree.links.new(toMesh.outputs[0], target)

class ConvertObjectToMeshData(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Object" and target.dataType == "Mesh Data"
    def insert(self, nodeTree, origin, target, dataOrigin):
        objectMeshData, toMeshData = insertNodes(nodeTree, ["an_ObjectMeshDataNode", "an_CombineMeshDataNode"], origin, target)
        origin.linkWith(objectMeshData.inputs[0])
        for i in range(3):
            objectMeshData.outputs[i].linkWith(toMeshData.inputs[i])
        toMeshData.outputs[0].linkWith(target)

class ConvertListToLength(LinkCorrection):
    def check(self, origin, target):
        return "List" in origin.dataType and target.dataType == "Integer"
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_GetListLengthNode", origin, target)
        node.hide = True
        node.width_hidden = 60

class ConvertToText(LinkCorrection):
    def check(self, origin, target):
        return target.dataType == "Text"
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_ConvertToTextNode", origin, target)
        node.hide = True

class ConvertFromGenericList(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Generic List" and isList(target.dataType)
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_ConvertNode", origin, target)
        node.hide = True

class ConvertFromGeneric(LinkCorrection):
    def check(self, origin, target):
        return origin.dataType == "Generic"
    def insert(self, nodeTree, origin, target, dataOrigin):
        node = insertLinkedNode(nodeTree, "an_ConvertNode", origin, target)
        node.hide = True
        tree_info.update()
        node.assignOutputType(target.dataType)


def insertLinkedNode(nodeTree, nodeType, origin, target):
    node = insertNode(nodeTree, nodeType, origin, target)
    insertBasicLinking(nodeTree, origin, node, target)
    return node

def insertNode(nodeTree, nodeType, leftSocket, rightSocket):
    nodes = insertNodes(nodeTree, [nodeType], leftSocket, rightSocket)
    return nodes[0]

def insertNodes(nodeTree, nodeTypes, leftSocket, rightSocket):
    center = getSocketCenter(leftSocket, rightSocket)
    amount = len(nodeTypes)
    nodes = []
    for i, nodeType in enumerate(nodeTypes):
        node = nodeTree.nodes.new(nodeType)
        node.select = False
        node.location = center + Vector((180 * (i - (amount - 1) / 2), 0))
        node.parent = leftSocket.node.parent
        nodes.append(node)
    return nodes

def insertBasicLinking(nodeTree, originSocket, node, targetSocket):
    nodeTree.links.new(node.inputs[0], originSocket)
    nodeTree.links.new(targetSocket, node.outputs[0])

def getSocketCenter(socket1, socket2):
    return (socket1.node.viewLocation + socket2.node.viewLocation) / 2

linkCorrectors = [
    ConvertMeshDataListToMeshData(),
    ConvertNormalToEuler(),
    ConvertObjectToMeshData(),
    ConvertSeparatedMeshDataToBMesh(),
    ConvertEulerToQuaternion(),
    ConvertQuaternionToEuler(),
    ConvertFloatToScale(),
    ConvertElementToList(),
    ConvertObjectToShapeKey(),
	ConvertListToLength(),
    SimpleConvert(),
    ConvertToText(),
    ConvertFromGenericList(),
    ConvertFromGeneric() ]
