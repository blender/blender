import bpy
from bpy.props import *
from ... base_types import VectorizedNode
from ... math cimport (Vector3, toVector3,
                       absoluteVec3, addVec3, subVec3, multVec3, divideVec3,
                       crossVec3, projectVec3, reflectVec3, normalizeLengthVec3,
                       scaleVec3, snapVec3)
from ... data_structures cimport Vector3DList, DoubleList

ctypedef void (*SingleVectorFunction)(Vector3* target, Vector3* source)
ctypedef void (*DoubleVectorFunction)(Vector3* target, Vector3* a, Vector3* b)
ctypedef void (*VectorFloatFunction)(Vector3* target, Vector3* a, float b)

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

    # Single Vector as Input
    #####################################################

    def execute_vA(self, Vector3DList a):
        cdef Vector3DList result = Vector3DList(length = a.length)
        cdef SingleVectorFunction f = <SingleVectorFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, a.data + i)
        return result

    # Two Vectors as Input
    #####################################################

    def execute_vA_vB(self, a, b):
        if isinstance(a, Vector3DList) and isinstance(b, Vector3DList):
            if len(a) == len(b):
                return self._execute_vA_vB_Both(a, b)
            else:
                raise ValueError("lists have different length")
        elif isinstance(a, Vector3DList):
            return self._execute_vA_vB_Left(a, b)
        elif isinstance(b, Vector3DList):
            return self._execute_vA_vB_Right(a, b)

    cdef _execute_vA_vB_Both(self, Vector3DList a, Vector3DList b):
        cdef Vector3DList result = Vector3DList(length = a.length)
        cdef DoubleVectorFunction f = <DoubleVectorFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, a.data + i, b.data + i)
        return result

    cdef _execute_vA_vB_Left(self, Vector3DList a, b):
        cdef Vector3 _b = toVector3(b)
        cdef Vector3DList result = Vector3DList(length = a.length)
        cdef DoubleVectorFunction f = <DoubleVectorFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, a.data + i, &_b)
        return result

    cdef _execute_vA_vB_Right(self, a, Vector3DList b):
        cdef Vector3 _a = toVector3(a)
        cdef Vector3DList result = Vector3DList(length = b.length)
        cdef DoubleVectorFunction f = <DoubleVectorFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, &_a, b.data + i)
        return result

    # Vector and Float as Input
    #####################################################

    def execute_vA_fB(self, a, b):
        if isinstance(a, Vector3DList) and isinstance(b, DoubleList):
            if len(a) == len(b):
                return self._execute_vA_fB_Both(a, b)
            else:
                raise ValueError("lists have different length")
        elif isinstance(a, Vector3DList):
            return self._execute_vA_fB_Left(a, b)
        elif isinstance(b, DoubleList):
            return self._execute_vA_fB_Right(a, b)

    cdef _execute_vA_fB_Both(self, Vector3DList a, DoubleList b):
        cdef Vector3DList result = Vector3DList(length = a.length)
        cdef VectorFloatFunction f = <VectorFloatFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, a.data + i, b.data[i])
        return result

    cdef _execute_vA_fB_Left(self, Vector3DList a, float b):
        cdef Vector3DList result = Vector3DList(length = a.length)
        cdef VectorFloatFunction f = <VectorFloatFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, a.data + i, b)
        return result

    cdef _execute_vA_fB_Right(self, a, DoubleList b):
        cdef Vector3 _a = toVector3(a)
        cdef Vector3DList result = Vector3DList(length = b.length)
        cdef VectorFloatFunction f = <VectorFloatFunction>self.function
        cdef long i
        for i in range(result.length):
            f(result.data + i, &_a, b.data[i])
        return result


cdef new(str name, str label, str type, str expression, void* function):
    cdef Operation op = Operation()
    op.setup(name, label, type, expression, function)
    return op

cdef dict operations = {}

operations[0] = new("Add", "A + B", "vA_vB",
    "result = a + b", <void*>addVec3)
operations[1] = new("Subtract", "A - B", "vA_vB",
    "result = a - b", <void*>subVec3)
operations[2] = new("Multiply", "A * B", "vA_vB",
    "result = Vector((a[0] * b[0], a[1] * b[1], a[2] * b[2]))", <void*>multVec3)
operations[3] = new("Divide", "A / B", "vA_vB",
    '''result = Vector((a[0] / b[0] if b[0] != 0 else 0,
                        a[1] / b[1] if b[1] != 0 else 0,
                        a[2] / b[2] if b[2] != 0 else 0))''', <void*>divideVec3)
operations[4] = new("Cross", "A x B", "vA_vB",
    "result = a.cross(b)", <void*>crossVec3)
operations[5] = new("Project", "A project B", "vA_vB",
    "result = a.project(b)", <void*>projectVec3)
operations[6] = new("Reflect", "A reflect B", "vA_vB",
    "result = a.reflect(b)", <void*>reflectVec3)
operations[7] = new("Normalize", "normalize A", "vA_fLength",
    "result = a.normalized() * length", <void*>normalizeLengthVec3)
operations[8] = new("Scale", "scale A", "vA_fFactor",
    "result = a * factor", <void*>scaleVec3)
operations[9] = new("Absolute", "abs A", "vA",
    "result = Vector((abs(a[0]), abs(a[1]), abs(a[2])))", <void*>absoluteVec3)
operations[10] = new("Snap", "snap A", "vA_vStep",
    '''result = Vector((round(a.x / step.x) * step.x if step.x != 0 else a.x,
                        round(a.y / step.y) * step.y if step.y != 0 else a.y,
                        round(a.z / step.z) * step.z if step.z != 0 else a.z))''',
    <void*>snapVec3)

operationItems = [(op.name, op.name, op.label, i) for i, op in operations.items()]
operationByName = {op.name : op for op in operations.values()}

dataTypeById = {
    "v": ("Vector", "Vector List", (0, 0, 0)),
    "f": ("Float", "Float List", 1) }

searchItems = {
    "Add Vectors" : "Add",
    "Scale Vector" : "Scale",
    "Subtract Vectors" : "Subtract",
    "Multiply Vectors" : "Multiply",
    "Normalize Vector" : "Normalize"}

class VectorMathNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_VectorMathNode"
    bl_label = "Vector Math"
    dynamicLabelType = "HIDDEN_ONLY"
    searchTags = [(name, {"operation" : repr(op)}) for name, op in searchItems.items()]

    operation = EnumProperty(name = "Operation", default = "Add",
        items = operationItems, update = VectorizedNode.refresh)

    useListA = VectorizedNode.newVectorizeProperty()
    useListB = VectorizedNode.newVectorizeProperty()
    useListLength = VectorizedNode.newVectorizeProperty()
    useListFactor = VectorizedNode.newVectorizeProperty()
    useListStep = VectorizedNode.newVectorizeProperty()

    errorMessage = StringProperty()

    def create(self):
        usedProperties = []

        for socketData in self._operation.type.split("_"):
            name = socketData[1:]
            listProperty = "useList" + name
            usedProperties.append(listProperty)
            baseType, *_ = dataTypeById[socketData[0]]

            self.newVectorizedInput(baseType, listProperty,
                (name, name.lower()), (name, name.lower()))

        self.newVectorizedOutput("Vector", [usedProperties],
            ("Result", "result"), ("Results", "results"))

    def draw(self, layout):
        layout.prop(self, "operation", text = "")
        if self.errorMessage != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def drawLabel(self):
        return self._operation.label

    def getExecutionCode(self):
        if self.generatesList:
            currentType = self._operation.type
            yield "try:"
            yield "    self.errorMessage = ''"
            if currentType == "vA":
                yield "    results = self._operation.execute_vA(a)"
            if currentType == "vA_vB":
                yield "    results = self._operation.execute_vA_vB(a, b)"
            elif currentType == "vA_fLength":
                yield "    results = self._operation.execute_vA_fB(a, length)"
            elif currentType == "vA_fFactor":
                yield "    results = self._operation.execute_vA_fB(a, factor)"
            elif currentType == "vA_vStep":
                yield "    results = self._operation.execute_vA_vB(a, step)"
            yield "except Exception as e:"
            yield "    self.errorMessage = str(e)"
            yield "    results = self.outputs[0].getDefaultValue()"
        else:
            yield self._operation.expression

    @property
    def _operation(self):
        return operationByName[self.operation]

    @property
    def generatesList(self):
        return any(socket.dataType in ("Float List", "Vector List") for socket in self.inputs)
