import bpy
import inspect
from bpy.props import *
from . blender_ui import redrawAll

createdOperators = []

def makeOperator(idName, label, arguments = [], *, redraw = False, confirm = False,
                 description = "", options = {"REGISTER", "INTERNAL", "UNDO"}):
    def makeOperatorDecorator(function):
        operator = getOperatorForFunction(function, idName, label, arguments, redraw,
                                          confirm, description, options)
        bpy.utils.register_class(operator)
        createdOperators.append(operator)
        return function
    return makeOperatorDecorator

def getOperatorForFunction(function, idName, label, arguments, redraw, confirm, description, options):
    def invoke(self, context, event):
        if confirm:
            return context.window_manager.invoke_confirm(self, event)
        else:
            return self.execute(context)

    def execute(self, context):
        parameters = list(iterParameterNamesAndDefaults(function))
        function(*[getattr(self, name) for name, _ in parameters])
        if redraw:
            redrawAll()
        return {"FINISHED"}

    operator = type(idName, (bpy.types.Operator, ), {
        "bl_idname" : idName,
        "bl_label" : label,
        "bl_description" : description,
        "bl_options" : options,
        "invoke" : invoke,
        "execute" : execute })

    parameters = list(iterParameterNamesAndDefaults(function))
    for argument, (name, default) in zip(arguments, parameters):
        if argument == "Int": propertyType = IntProperty
        elif argument == "String": propertyType = StringProperty
        else: raise ValueError("cannot create property of this type")

        if default is None: setattr(operator, name, propertyType())
        else: setattr(operator, name, propertyType(default = default))

    return operator

def iterParameterNamesAndDefaults(function):
    for parameter in inspect.signature(function).parameters.values():
        default = parameter.default if isinstance(parameter.default, (int, float, str)) else None
        yield (parameter.name, default)

def register():
    for operator in createdOperators:
        try: bpy.utils.register_class(operator)
        except: pass

def unregister():
    for operator in createdOperators:
        bpy.utils.unregister_class(operator)
