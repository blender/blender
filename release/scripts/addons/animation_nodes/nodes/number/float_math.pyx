import bpy
from bpy.props import *
from collections import OrderedDict
from ... base_types import VectorizedNode
from ... utils.handlers import validCallback

from ... data_structures cimport DoubleList
from ... math cimport min as minNumber
from ... math cimport max as maxNumber
from ... math cimport abs as absNumber
from ... math cimport (add, subtract, multiply, divide_Save, modulo_Save,
                       sin, cos, tan, asin_Save, acos_Save, atan, atan2, hypot,
                       power_Save, floor, ceil, sqrt_Save, invert, reciprocal_Save,
                       snap_Save, copySign, floorDivision_Save, logarithm_Save)

ctypedef double (*SingleInputFunction)(double a)
ctypedef double (*DoubleInputFunction)(double a, double b)

cdef class Operation:
    cdef:
        readonly str name
        readonly str label
        readonly str type
        readonly str expression
        void* function

    cdef setup(self, str name, str label, str type, str expression, void* function):
        self.name = name
        self.label = label
        self.type = type
        self.expression = expression
        self.function = function

    def execute_A(self, DoubleList a):
        cdef DoubleList result = DoubleList(length = a.length)
        cdef SingleInputFunction f = <SingleInputFunction>self.function
        cdef long i
        for i in range(result.length):
            result.data[i] = f(a.data[i])
        return result

    def execute_A_B(self, a, b):
        if isinstance(a, DoubleList) and isinstance(b, DoubleList):
            if len(a) == len(b):
                return self._execute_A_B_Both(a, b)
            else:
                raise ValueError("lists have different length")
        elif isinstance(a, DoubleList):
            return self._execute_A_B_Left(a, b)
        elif isinstance(b, DoubleList):
            return self._execute_A_B_Right(a, b)

    def _execute_A_B_Both(self, DoubleList a, DoubleList b):
        cdef DoubleList result = DoubleList(length = a.length)
        cdef DoubleInputFunction f = <DoubleInputFunction>self.function
        cdef long i
        for i in range(result.length):
            result.data[i] = f(a.data[i], b.data[i])
        return result

    def _execute_A_B_Left(self, DoubleList a, double b):
        cdef DoubleList result = DoubleList(length = a.length)
        cdef DoubleInputFunction f = <DoubleInputFunction>self.function
        cdef long i
        for i in range(result.length):
            result.data[i] = f(a.data[i], b)
        return result

    def _execute_A_B_Right(self, double a, DoubleList b):
        cdef DoubleList result = DoubleList(length = b.length)
        cdef DoubleInputFunction f = <DoubleInputFunction>self.function
        cdef long i
        for i in range(result.length):
            result.data[i] = f(a, b.data[i])
        return result

cdef new(str name, str label, str type, str expression, void* function):
    cdef Operation op = Operation()
    op.setup(name, label, type, expression, function)
    return op

operations = OrderedDict()

# Changing the order/indices can break existing files
operations[0] = new("Add", "A + B", "A_B",
    "result = a + b", <void*>add)
operations[1] = new("Subtract", "A - B", "A_B",
    "result = a - b", <void*>subtract)
operations[2] = new("Multiply", "A * B", "A_B",
    "result = a * b", <void*>multiply)
operations[3] = new("Divide", "A / B", "A_B",
    "result = a / b if b != 0 else 0", <void*>divide_Save)
operations[4] = new("Sin", "sin A", "A",
    "result = math.sin(a)", <void*>sin)
operations[5] = new("Cos", "cos A", "A",
    "result = math.cos(a)", <void*>cos)
operations[6] = new("Tangent", "tan A", "A",
    "result = math.tan(a)", <void*>tan)
operations[7] = new("Arcsin", "asin A", "A",
    "result = math.asin(min(max(a, -1), 1))", <void*>asin_Save)
operations[8] = new("Arccosine", "acos A", "A",
    "result = math.acos(min(max(a, -1), 1))", <void*>acos_Save)
operations[9] = new("Arctangent", "atan A", "A",
    "result = math.atan(a)", <void*>atan)
operations[10] = new("Power", "Base^Exponent", "Base_Exponent",
    "result = math.pow(base, exponent) if base >= 0 or exponent % 1 == 0 else 0",
    <void*>power_Save)
operations[11] = new("Logarithm", "log A with Base", "A_Base",
    "result = 0 if a <= 0 else math.log(a) if base <= 0 or base == 1 else math.log(a, base)",
    <void*>logarithm_Save)
operations[12] = new("Minimum", "min(A, B)", "A_B",
    "result = min(a, b)", <void*>minNumber)
operations[13] = new("Maximum", "max(A, B)", "A_B",
    "result = max(a, b)", <void*>maxNumber)
operations[15] = new("Modulo", "A mod B", "A_B",
    "result = a % b if b != 0 else 0", <void*>modulo_Save)
operations[16] = new("Absolute", "abs A)", "A",
    "result = abs(a)", <void*>absNumber)
operations[17] = new("Floor", "floor A", "A",
    "result = math.floor(a)", <void*>floor)
operations[18] = new("Ceiling", "ceil A", "A",
    "result = math.ceil(a)", <void*>ceil)
operations[19] = new("Square Root", "sqrt A", "A",
    "result = math.sqrt(a) if a >= 0 else 0", <void*>sqrt_Save)
operations[20] = new("Invert", "- A", "A",
    "result = -a", <void*>invert)
operations[21] = new("Reciprocal", "1 / A", "A",
    "result = 1 / a if a != 0 else 0", <void*>reciprocal_Save)
operations[22] = new("Snap", "snap A to Step", "A_Step",
    "result = round(a / step) * step if step != 0 else a", <void*>snap_Save)
operations[23] = new("Arctangent B/A", "atan2 (B / A)", "A_B",
    "result = math.atan2(b, a)", <void*>atan2)
operations[24] = new("Hypotenuse", "hypot A, B", "A_B",
    "result = math.hypot(a, b)", <void*>hypot)
operations[25] = new("Copy Sign", "A gets sign of B", "A_B",
    "result = math.copysign(a, b)", <void*>copySign)
operations[26] = new("Floor Division", "floor(A / B)", "A_B",
    "result = a // b if b != 0 else 0", <void*>floorDivision_Save)

operationItems = [(op.name, op.name, op.label, "NONE", i) for i, op in operations.items()]
operationByName = {op.name : op for op in operations.values()}

searchItems = {
    "Add Numbers" : "Add",
    "Subtract Numbers" : "Subtract",
    "Multiply Numbers" : "Multiply",
    "Divide Numbers" : "Divide",
    "Invert Number" : "Invert",
    "Reciprocal Number" : "Reciprocal"}

justCopiedIdentifiers = set()

class FloatMathNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_FloatMathNode"
    bl_label = "Float Math"
    dynamicLabelType = "ALWAYS"
    searchTags = [(name, {"operation" : repr(op)}) for name, op in searchItems.items()]

    @validCallback
    def operationChanged(self, context):
        if self.identifier in justCopiedIdentifiers:
            justCopiedIdentifiers.remove(self.identifier)
        self.refresh()

    operation = EnumProperty(name = "Operation", default = "Multiply",
        description = "Operation to perform on the inputs",
        items = operationItems, update = operationChanged)

    errorMessage = StringProperty()

    useListA = VectorizedNode.newVectorizeProperty()
    useListB = VectorizedNode.newVectorizeProperty()
    useListBase = VectorizedNode.newVectorizeProperty()
    useListExponent = VectorizedNode.newVectorizeProperty()
    useListStep = VectorizedNode.newVectorizeProperty()

    def create(self):
        usedProperties = []

        for name in self._operation.type.split("_"):
            listProperty = "useList" + name
            usedProperties.append(listProperty)

            self.newVectorizedInput("Float", listProperty,
                (name, name.lower()), (name, name.lower()))

        self.newVectorizedOutput("Float", [usedProperties],
            ("Result", "result"), ("Results", "results"))

    def draw(self, layout):
        showQuickOptions = self.identifier in justCopiedIdentifiers

        col = layout.column()
        row = col.row(align = True)
        row.prop(self, "operation", text = "")

        if showQuickOptions:
            self.invokeFunction(row, "setOperation", "", data = self.operation, icon = "FILE_TICK")

            subcol = col.column(align = True)
            row = subcol.row(align = True)
            self.invokeFunction(row, "setOperation", "Add", data = "Add")
            self.invokeFunction(row, "setOperation", "Sub", data = "Subtract")
            row = subcol.row(align = True)
            self.invokeFunction(row, "setOperation", "Mul", data = "Multiply")
            self.invokeFunction(row, "setOperation", "Div", data = "Divide")

        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawAdvanced(self, layout):
        self.invokeFunction(layout, "removeQuickSettings", "Remove Quick Settings",
            description = "Remove quick settings from all Float Math nodes.")

    def drawLabel(self):
        if not self.hide:
            return "Math"

        operation = self._operation
        label = operation.label
        for socket in self.inputs:
            if not socket.isLinked:
                label = label.replace(socket.name, str(round(socket.value, 5)))
        return label

    def setOperation(self, operation):
        self.operation = operation

    def getExecutionCode(self):
        if self.generatesList:
            currentType = self._operation.type
            yield "try:"
            yield "    self.errorMessage = ''"
            if currentType == "A":
                yield "    results = self._operation.execute_A(a)"
            elif currentType == "A_B":
                yield "    results = self._operation.execute_A_B(a, b)"
            elif currentType == "Base_Exponent":
                yield "    results = self._operation.execute_A_B(base, exponent)"
            elif currentType == "A_Step":
                yield "    results = self._operation.execute_A_B(a, step)"
            elif currentType == "A_Base":
                yield "    results = self._operation.execute_A_B(a, base)"
            yield "except Exception as e:"
            yield "    self.errorMessage = str(e)"
            yield "    results = self.outputs[0].getDefaultValue()"
        else:
            yield self._operation.expression

    def getUsedModules(self):
        return ["math"]

    def duplicate(self, sourceNode):
        if len(bpy.context.selected_nodes) == 2: # this and source node
            justCopiedIdentifiers.add(self.identifier)

    def removeQuickSettings(self):
        justCopiedIdentifiers.clear()

    @property
    def _operation(self):
        return operationByName[self.operation]

    @property
    def generatesList(self):
        return any(socket.dataType == "Float List" for socket in self.inputs)
