from bpy.props import *
from ... events import propertyChanged, executionCodeChanged

parameterTypeItems = [
    ("RESOLUTION", "Resolution", ""),
    ("UNIFORM", "Uniform", "")]

class SplineEvaluationBase:

    def parameterTypeChanged(self, context):
        propertyChanged()
        executionCodeChanged()

    parameterType = EnumProperty(name = "Parameter Type", default = "UNIFORM",
        items = parameterTypeItems, update = parameterTypeChanged)

    resolution = IntProperty(name = "Resolution", min = 2, default = 5,
        description = "Increase to have a more accurate evaluation if the type is set to Uniform",
        update = propertyChanged)
