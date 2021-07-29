import bpy
from collections import defaultdict

drawHandlers = []
registeredHandlersPerEditor = defaultdict(list)

def drawHandler(editorName, regionName, drawType = "POST_PIXEL"):
    def drawHandlerDecorator(function):
        drawHandlers.append((function, editorName, regionName, drawType))
        return function

    return drawHandlerDecorator

def register():
    for function, editorName, regionName, drawType in drawHandlers:
        editor = getattr(bpy.types, editorName)
        handler = editor.draw_handler_add(function, (), regionName, drawType)
        registeredHandlersPerEditor[editor].append((handler, regionName))

def unregister():
    for editor, handlers in registeredHandlersPerEditor.items():
        for handler, regionName in handlers:
            editor.draw_handler_remove(handler, regionName)
    registeredHandlersPerEditor.clear()
