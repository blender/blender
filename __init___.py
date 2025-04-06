


bl_info = {
    "name": "Aqua render engine",
    "author": "Mizu",
    "version": (1, 0, 0),
    "blender": (4, 5, 0),
    "description": "A brand new render engine for Blender",
    "tracker_url": "",
    "doc_url": "",
    "community": "",
    "downloads": "",
    "main_web": "",
    "support": 'OFFICIAL',
    "catergory": "Render"
}


from . import engine, properties, ui


def register():
    engine.register()
    properties.register()
    ui.register()


def unregister():
    ui.unregister()
    properties.unregister()
    engine.unregister()