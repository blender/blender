import bpy
from bpy.props import *
from ... utils.code import isCodeValid
from ... base_types import VectorizedNode
from ... events import executionCodeChanged

class ObjectAttributeOutputNode(bpy.types.Node, VectorizedNode):
    bl_idname = "an_ObjectAttributeOutputNode"
    bl_label = "Object Attribute Output"
    bl_width_default = 175

    attribute = StringProperty(name = "Attribute", default = "",
        update = executionCodeChanged)

    useObjectList = VectorizedNode.newVectorizeProperty()
    useValueList = BoolProperty(update = VectorizedNode.refresh)

    errorMessage = StringProperty()

    def create(self):
        self.newVectorizedInput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

        self.newInputGroup(self.useValueList and self.useObjectList,
            ("Generic", "Value", "value"),
            ("Generic List", "Values", "values"))

        self.newVectorizedOutput("Object", "useObjectList",
            ("Object", "object", dict(defaultDrawType = "PROPERTY_ONLY")),
            ("Objects", "objects"))

    def draw(self, layout):
        col = layout.column()
        col.prop(self, "attribute", text = "")
        if self.useObjectList:
            col.prop(self, "useValueList", text = "Multiple Values")
        if self.errorMessage != "" and self.attribute != "":
            layout.label(self.errorMessage, icon = "ERROR")

    def getExecutionCode(self):
        code = self.evaluationExpression

        if not isCodeValid(code):
            self.errorMessage = "Invalid Syntax"
            return
        else: self.errorMessage = ""

        yield "try:"
        yield "    self.errorMessage = ''"
        if self.useObjectList:
            if self.useValueList:
                yield "    if len(objects) != len(values):"
                yield "        self.errorMessage = 'Lists have different length'"
                yield "        raise Exception()"
                yield "    for object, value in zip(objects, values):"
            else:
                yield "    for object in objects:"
            yield "        " + code
        else:
            yield "    " + code
        yield "except AttributeError:"
        yield "    if object: self.errorMessage = 'Attribute not found'"
        yield "except KeyError:"
        yield "    if object: self.errorMessage = 'Key not found'"
        yield "except IndexError:"
        yield "    if object: self.errorMessage = 'Index not found'"
        yield "except (ValueError, TypeError):"
        yield "    if object: self.errorMessage = 'Value has a wrong type'"
        yield "except:"
        yield "    if object and self.errorMessage == '':"
        yield "        self.errorMessage = 'Unknown error'"

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
