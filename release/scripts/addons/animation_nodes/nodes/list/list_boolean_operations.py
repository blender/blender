import bpy
import itertools
from bpy.props import *
from ... sockets.info import isList
from ... events import executionCodeChanged
from ... base_types import AnimationNode, AutoSelectListDataType

operationItems = [
    ("UNION", "Union", "Elements that are at least in one of both lists", "NONE", 0),
    ("INTERSECTION", "Intersection", "Elements that are in both lists", "NONE", 1),
    ("DIFFERENCE", "Difference", "Elements that are in list 1 but not in list 2", "NONE", 2),
    ("SYMMETRIC_DIFFERENCE", "Symmetric Difference", "Elements that are in list 1 or in list 2 but not in both", "NONE", 3) ]

class ListBooleanOperationsNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ListBooleanOperationsNode"
    bl_label = "List Boolean Operations"

    operation = EnumProperty(name = "Operation", default = "UNION",
        items = operationItems, update = executionCodeChanged)

    assignedType = StringProperty(update = AnimationNode.refresh, default = "Object List")

    def create(self):
        self.newInput(self.assignedType, "List 1", "list1", dataIsModified = True)
        self.newInput(self.assignedType, "List 2", "list2", dataIsModified = True)
        self.newOutput(self.assignedType, "List", "outList")

        self.newSocketEffect(AutoSelectListDataType("assignedType", "LIST",
            [(self.inputs[0], "LIST"),
             (self.inputs[1], "LIST"),
             (self.outputs[0], "LIST")]
        ))

    def draw(self, layout):
        layout.prop(self, "operation", text = "")

    def getExecutionCode(self):
        op = self.operation
        # I don't use sets here to keep the order the elements come in
        # But we could speedup this node by using sets to see if an element is already inserted
        if op == "UNION": return "outList = self.execute_Union(list1, list2)"
        if op == "INTERSECTION": return "outList = self.execute_Intersection(list1, list2)"
        if op == "DIFFERENCE": return "outList = self.execute_Difference(list1, list2)"
        if op == "SYMMETRIC_DIFFERENCE": return "outList = self.execute_SymmetricDifference(list1, list2)"

    def execute_Union(self, list1, list2):
        outList = self.getEmptyStartList()
        append = outList.append
        for element in itertools.chain(list1, list2):
            if element not in outList:
                append(element)
        return outList

    def execute_Intersection(self, list1, list2):
        outList = self.getEmptyStartList()
        append = outList.append
        for element in list1:
            if element not in outList:
                if element in list2:
                    append(element)
        return outList

    def execute_Difference(self, list1, list2):
        outList = self.getEmptyStartList()
        append = outList.append
        for element in list1:
            if element not in outList:
                if element not in list2:
                    append(element)
        return outList

    def execute_SymmetricDifference(self, list1, list2):
        # I could combine this out of the methods above but it would
        # propably be slower. Maybe someone can test this later.
        outList = self.getEmptyStartList()
        append = outList.append
        for element in list1:
            if element not in outList:
                if element not in list2:
                    append(element)
        for element in list2:
            if element not in outList:
                if element not in list1:
                    append(element)
        return outList

    def getEmptyStartList(self):
        return self.outputs[0].getDefaultValue()

    def assignType(self, listDataType):
        if not isList(listDataType): return
        if listDataType == self.assignedType: return
        self.assignedType = listDataType
