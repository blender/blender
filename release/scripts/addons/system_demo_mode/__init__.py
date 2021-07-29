# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

bl_info = {
    "name": "Demo Mode",
    "author": "Campbell Barton",
    "blender": (2, 57, 0),
    "location": "Demo Menu",
    "description": "Demo mode lets you select multiple blend files and loop over them.",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/System/Demo_Mode#Running_Demo_Mode",
    "support": 'OFFICIAL',
    "category": "System"}

# To support reload properly, try to access a package var, if it's there, reload everything
if "bpy" in locals():
    import importlib
    if "config" in locals():
        importlib.reload(config)


import bpy
from bpy.props import (
        StringProperty,
        BoolProperty,
        IntProperty,
        FloatProperty,
        EnumProperty,
        )


class DemoModeSetup(bpy.types.Operator):
    """Create a demo script and optionally execute it"""
    bl_idname = "wm.demo_mode_setup"
    bl_label = "Demo Mode (Setup)"
    bl_options = {'PRESET'}

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    # these are used to create the file list.
    directory = StringProperty(
            name="Search Path",
            description="Directory used for importing the file",
            maxlen=1024,
            subtype='DIR_PATH',
            )
    random_order = BoolProperty(
            name="Random Order",
            description="Select files randomly",
            default=False,
            )
    mode = EnumProperty(
            name="Method",
            items=(('AUTO', "Auto", ""),
                   ('PLAY', "Play", ""),
                   ('RENDER', "Render", ""),
                   ),
            )

    run = BoolProperty(
            name="Run Immediately!",
            description="Run demo immediately",
            default=True,
            )
    exit = BoolProperty(
            name="Exit",
            description="Run once and exit",
            default=False,
            )

    # these are mapped directly to the config!
    #
    # anim
    # ====
    anim_cycles = IntProperty(
            name="Cycles",
            description="Number of times to play the animation",
            min=1, max=1000,
            default=2,
            )
    anim_time_min = FloatProperty(
            name="Time Min",
            description="Minimum number of seconds to show the animation for "
                        "(for small loops)",
            min=0.0, max=1000.0,
            soft_min=1.0, soft_max=1000.0,
            default=4.0,
            )
    anim_time_max = FloatProperty(
            name="Time Max",
            description="Maximum number of seconds to show the animation for "
                        "(in case the end frame is very high for no reason)",
            min=0.0, max=100000000.0,
            soft_min=1.0, soft_max=100000000.0,
            default=8.0,
            )
    anim_screen_switch = FloatProperty(
            name="Screen Switch",
            description="Time between switching screens (in seconds) "
                        "or 0 to disable",
            min=0.0, max=100000000.0,
            soft_min=1.0, soft_max=60.0,
            default=0.0,
            )
    #
    # render
    # ======
    display_render = FloatProperty(
            name="Render Delay",
            description="Time to display the rendered image before moving on "
                        "(in seconds)",
            min=0.0, max=60.0,
            default=4.0,
            )
    anim_render = BoolProperty(
            name="Render Anim",
            description="Render entire animation (render mode only)",
            default=False,
            )

    def execute(self, context):
        from . import config

        keywords = self.as_keywords(ignore=("directory", "random_order", "run", "exit"))
        cfg_str, dirpath = config.as_string(self.directory,
                                            self.random_order,
                                            self.exit,
                                            **keywords)
        text = bpy.data.texts.get("demo.py")
        if text:
            text.name += ".back"

        text = bpy.data.texts.new("demo.py")
        text.from_string(cfg_str)

        if self.run:
            extern_demo_mode_run()

        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def check(self, context):
        return True  # lazy

    def draw(self, context):
        layout = self.layout

        box = layout.box()
        box.label("Search *.blend recursively")
        box.label("Writes: demo.py config text")

        layout.prop(self, "run")
        layout.prop(self, "exit")

        layout.label("Generate Settings:")
        row = layout.row()
        row.prop(self, "mode", expand=True)
        layout.prop(self, "random_order")

        mode = self.mode

        layout.separator()
        sub = layout.column()
        sub.active = (mode in {'AUTO', 'PLAY'})
        sub.label("Animate Settings:")
        sub.prop(self, "anim_cycles")
        sub.prop(self, "anim_time_min")
        sub.prop(self, "anim_time_max")
        sub.prop(self, "anim_screen_switch")

        layout.separator()
        sub = layout.column()
        sub.active = (mode in {'AUTO', 'RENDER'})
        sub.label("Render Settings:")
        sub.prop(self, "display_render")


class DemoModeRun(bpy.types.Operator):
    bl_idname = "wm.demo_mode_run"
    bl_label = "Demo Mode (Start)"

    def execute(self, context):
        if extern_demo_mode_run():
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "Cant load demo.py config, run: File -> Demo Mode (Setup)")
            return {'CANCELLED'}


# --- call demo_mode.py funcs
def extern_demo_mode_run():
    # this accesses demo_mode.py which is kept standalone
    # and can be run direct.
    from . import demo_mode
    if demo_mode.load_config():
        demo_mode.demo_mode_load_file()  # kick starts the modal operator
        return True
    else:
        return False


def extern_demo_mode_register():
    # this accesses demo_mode.py which is kept standalone
    # and can be run direct.
    from . import demo_mode
    demo_mode.register()


def extern_demo_mode_unregister():
    # this accesses demo_mode.py which is kept standalone
    # and can be run direct.
    from . import demo_mode
    demo_mode.unregister()

# --- intergration


def menu_func(self, context):
    layout = self.layout
    layout.operator(DemoModeSetup.bl_idname, icon='PREFERENCES')
    layout.operator(DemoModeRun.bl_idname, icon='PLAY')
    layout.separator()


def register():
    bpy.utils.register_class(DemoModeSetup)
    bpy.utils.register_class(DemoModeRun)

    bpy.types.INFO_MT_file.prepend(menu_func)

    extern_demo_mode_register()


def unregister():
    bpy.utils.unregister_class(DemoModeSetup)
    bpy.utils.unregister_class(DemoModeRun)

    bpy.types.INFO_MT_file.remove(menu_func)

    extern_demo_mode_unregister()

if __name__ == "__main__":
    register()
