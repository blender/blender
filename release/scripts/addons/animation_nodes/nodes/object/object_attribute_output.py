import bpy
from bpy.props import *
from ... utils.code import isCodeValid
from ... events import executionCodeChanged
from ... base_types import AnimationNode, VectorizedSocket

class ObjectAttributeOutputNode(bpy.types.Node, AnimationNode):
    bl_idname = "an_ObjectAttributeOutputNode"
    bl_label = "Object Attribute Output"
    bl_width_default = 180
    errorHandlingType = "MESSAGE"

    attribute = StringProperty(name = "Attribute", default = "",
        update = executionCodeChanged)

    useObjectList = VectorizedSocket.newProperty()
    useValueList = BoolProperty(update = AnimationNode.refresh)


    def create(self):
        self.newInput(VectorizedSocket("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects")))

        if self.useValueList and self.useObjectList:
            self.newInput("Generic List", "Values", "values")
        else:
            self.newInput("Generic", "Value", "value")

        self.newOutput(VectorizedSocket("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects")))

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "attribute", text = "")
        if self.useObjectList:
            col.prop(self, "useValueList", text = "Multiple Values")

    def getExecutionCode(self, required):
        code = self.evaluationExpression

        if not isCodeValid(code):
            yield "self.setErrorMessage('Invalid Syntax', show = len(self.attribute.strip()) > 0)"
            return

        yield "try:"
        if self.useObjectList:
            if self.useValueList:
                yield "    _values = [None] if len(values) == 0 else values"
                yield "    _values = itertools.cycle(_values)"
                yield "    for object, value in zip(objects, _values):"
            else:
                yield "    for object in objects:"
            yield "        " + code
        else:
            yield "    " + code
        yield "except AttributeError:"
        yield "    if object: self.setErrorMessage('Attribute not found')"
        yield "except KeyError:"
        yield "    if object: self.setErrorMessage('Key not found')"
        yield "except IndexError:"
        yield "    if object: self.setErrorMessage('Index not found')"
        yield "except (ValueError, TypeError):"
        yield "    if object: self.setErrorMessage('Value has a wrong type')"
        yield "except:"
        yield "    if object:"
        yield "        self.setErrorMessage('Unknown error')"

    @property
    def evaluationExpression(self):
        if self.attribute.startswith("["): return "object" + self.attribute + " = value"
        else: return "object." + self.attribute + " = value"

    def getBakeCode(self):
        if not isCodeValid(self.attribute): return
        if self.useObjectList:
            yield "for object in objects:"
            yield "    if object is None: continue"
        else:
            yield "if object is not None:"
        yield "    try: object.keyframe_insert({})".format(repr(self.attribute))
        yield "    except: pass"
