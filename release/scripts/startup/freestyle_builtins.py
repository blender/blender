import bpy

from export_svg import svg_export_header, svg_export_animation

def register():
    bpy.app.handlers.render_init.append(svg_export_header)
    bpy.app.handlers.render_complete.append(svg_export_animation)

def unregister():
    bpy.app.handlers.render_init.remove(svg_export_header)
    bpy.app.handlers.render_complete.remove(svg_export_animation)

if __name__ == '__main__':
    register()
