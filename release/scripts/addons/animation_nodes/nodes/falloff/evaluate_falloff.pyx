import bpy
from bpy.props import *
from ... base_types import AnimationNode
from ... utils.clamp cimport clampLong
from ... data_structures cimport Falloff, FalloffEvaluator, DoubleList, CList

falloffTypeItems = [
    ("None", "None", "", "", 0),
    ("Location", "Location", "", "", 1),
    ("Transformation Matrix", "Transformation Matrix", "", "", 2)
]

invalidFalloffMessage = "invalid falloff type"

class EvaluateFalloffNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_EvaluateFalloffNode"
    bl_label = "Evaluate Falloff"

    falloffType = EnumProperty(name = "Falloff Type",
        items = falloffTypeItems, update = AnimationNode.refresh)

    useList = BoolProperty(name = "Use List", default = False,
        update = AnimationNode.refresh)

    errorMessage = StringProperty()

    def create(self):
        self.newInput("Falloff", "Falloff", "falloff")

        if self.useList:
            if self.falloffType == "None":
                self.newInput("Integer", "Amount", "amount", value = 10, minValue = 0)
            elif self.falloffType == "Location":
                self.newInput("Vector List", "Locations", "locations")
            elif self.falloffType == "Transformation Matrix":
                self.newInput("Matrix List", "Matrices", "matrices")
            self.newOutput("Float List", "Strengths", "strengths")
        else:
            if self.falloffType == "Location":
                self.newInput("Vector", "Location", "location")
            elif self.falloffType == "Transformation Matrix":
                self.newInput("Matrix", "Matrix", "matrix")
            self.newInput("Integer", "Index", "index")
            self.newOutput("Float", "Strength", "strength")

    def draw(self, layout):
        row = layout.row(align = True)
        row.prop(self, "falloffType", text = "")
        row.prop(self, "useList", text = "", icon = "LINENUMBERS_ON")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionFunctionName(self):
        if self.useList:
            if self.falloffType == "None":
                return "execute_List_None"
            elif self.falloffType in ("Location", "Transformation Matrix"):
                return "execute_List_CList"
        else:
            if self.falloffType == "None":
                return "execute_Single_None"
            elif self.falloffType in ("Location", "Transformation Matrix"):
                return "execute_Single_Object"

    def execute_Single_None(self, Falloff falloff, index):
        cdef long _index = clampLong(index)
        cdef FalloffEvaluator evaluator

        self.errorMessage = ""
        try: evaluator = falloff.getEvaluator("")
        except:
            self.errorMessage = invalidFalloffMessage
            return 0

        return evaluator.evaluate(NULL, _index)

    def execute_Single_Object(self, Falloff falloff, object, index):
        cdef long _index = clampLong(index)
        cdef FalloffEvaluator evaluator

        self.errorMessage = ""
        try: evaluator = falloff.getEvaluator(self.falloffType)
        except:
            self.errorMessage = invalidFalloffMessage
            return 0

        return evaluator(object, _index)

    def execute_List_None(self, falloff, amount):
        cdef FalloffEvaluator _falloff
        cdef DoubleList strengths
        cdef int i

        self.errorMessage = ""
        try: _falloff = falloff.getEvaluator("")
        except:
            self.errorMessage = invalidFalloffMessage
            return DoubleList()

        amount = max(amount, 0)
        strengths = DoubleList(length = amount)
        for i in range(amount):
            strengths.data[i] = _falloff.evaluate(NULL, i)
        return strengths

    def execute_List_CList(self, falloff, myList):
        cdef FalloffEvaluator _falloff
        self.errorMessage = ""
        try: _falloff = falloff.getEvaluator(self.falloffType)
        except:
            self.errorMessage = invalidFalloffMessage
            return DoubleList()

        return self.evaluate_CList(_falloff, myList)

    def evaluate_CList(self, FalloffEvaluator _falloff, CList myList):
        cdef long i
        cdef char *data = <char*>myList.getPointer()
        cdef int elementSize = myList.getElementSize()
        cdef DoubleList strengths = DoubleList(length = myList.getLength())
        for i in range(myList.getLength()):
            strengths.data[i] = _falloff.evaluate(data + i * elementSize, i)
        return strengths
