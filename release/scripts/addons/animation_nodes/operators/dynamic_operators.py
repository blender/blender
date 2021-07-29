'''
This module can create and register operators dynamically based on a description.
'''

import bpy
from bpy.props import *
from .. utils.handlers import eventHandler

operatorsByDescription = {}
missingDescriptions = set()

def getInvokeFunctionOperator(description):
    if description in operatorsByDescription:
        return operatorsByDescription[description]
    missingDescriptions.add(description)
    return fallbackOperator.bl_idname


@eventHandler("SCENE_UPDATE_POST")
def createMissingOperators(scene):
    while len(missingDescriptions) > 0:
        description = missingDescriptions.pop()
        operator = createOperatorWithDescription(description)
        operatorsByDescription[description] = operator.bl_idname
        bpy.utils.register_class(operator)

def createOperatorWithDescription(description):
    operatorID = str(len(operatorsByDescription))
    idName = "an.invoke_function_" + operatorID

    operator = type("InvokeFunction_" + operatorID, (bpy.types.Operator, ), {
        "bl_idname" : idName,
        "bl_label" : "Are you sure?",
        "bl_description" : description,
        "invoke" : invoke_InvokeFunction,
        "execute" : execute_InvokeFunction })
    operator.callback = StringProperty()
    operator.invokeWithData = BoolProperty(default = False)
    operator.confirm = BoolProperty()
    operator.data = StringProperty()
    operator.passEvent = BoolProperty()

    return operator

def invoke_InvokeFunction(self, context, event):
    self._event = event
    if self.confirm:
        return context.window_manager.invoke_confirm(self, event)
    return self.execute(context)

def execute_InvokeFunction(self, context):
    args = []
    if self.invokeWithData: args.append(self.data)
    if self.passEvent: args.append(self._event)
    self.an_executeCallback(self.callback, *args)

    bpy.context.area.tag_redraw()
    return {"FINISHED"}

fallbackOperator = createOperatorWithDescription("")


# Register
##################################

def register():
    try: bpy.utils.register_class(fallbackOperator)
    except: pass

def unregister():
    try: bpy.utils.unregister_class(fallbackOperator)
    except: pass
