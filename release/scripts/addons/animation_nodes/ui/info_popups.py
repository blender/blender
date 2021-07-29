import bpy

def showTextPopup(text, title = "", icon = "NONE"):
    bpy.context.window_manager.popup_menu(getPopupDrawer(text), title = title, icon = icon)

def getPopupDrawer(text):
    def drawPopup(menu, context):
        layout = menu.layout
        layout.label(text)
    return drawPopup
