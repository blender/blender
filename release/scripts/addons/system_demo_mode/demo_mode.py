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

"""
Even though this is in a package this can run as a stand alone scripts.

# --- example usage
blender --python release/scripts/addons/system_demo_mode/demo_mode.py

looks for demo.py textblock or file in the same path as the blend:
# --- example
config = [
    dict(anim_cycles=1, anim_render=False, anim_screen_switch=0.0, anim_time_max=10.0, anim_time_min=4.0, mode='AUTO', display_render=4.0, file='/l/19534_simplest_mesh_2.blend'),
    dict(anim_cycles=1, anim_render=False, anim_screen_switch=0.0, anim_time_max=10.0, anim_time_min=4.0, mode='AUTO', display_render=4.0, file='/l/252_pivotConstraint_01.blend'),
    ]
# ---
/data/src/blender/lib/tests/rendering/
"""

import bpy
import time
import tempfile
import os

DEMO_CFG = "demo.py"

# populate from script
global_config_files = []

global_config = dict(anim_cycles=1,
                     anim_render=False,
                     anim_screen_switch=0.0,
                     anim_time_max=60.0,
                     anim_time_min=4.0,
                     mode='AUTO',
                     display_render=4.0)

# switch to the next file in 2 sec.
global_config_fallback = dict(anim_cycles=1,
                              anim_render=False,
                              anim_screen_switch=0.0,
                              anim_time_max=60.0,
                              anim_time_min=4.0,
                              mode='AUTO',
                              display_render=4.0)


global_state = {
    "init_time": 0.0,
    "last_switch": 0.0,
    "reset_anim": False,
    "anim_cycles": 0,  # count how many times we played the anim
    "last_frame": -1,
    "is_render": False,
    "render_time": "",  # time render was finished.
    "timer": None,
    "basedir": "",  # demo.py is stored here
    "demo_index": 0,
    "exit": False,
}


# -----------------------------------------------------------------------------
# render handler - maintain "is_render"

def handle_render_clear():
    for ls in (bpy.app.handlers.render_complete, bpy.app.handlers.render_cancel):
        while handle_render_done_cb in ls:
            ls.remove(handle_render_done_cb)


def handle_render_done_cb(self):
    global_state["is_render"] = True


def handle_render_init():
    handle_render_clear()
    bpy.app.handlers.render_complete.append(handle_render_done_cb)
    bpy.app.handlers.render_cancel.append(handle_render_done_cb)
    global_state["is_render"] = False


def demo_mode_auto_select():

    play_area = 0
    render_area = 0

    totimg = 0

    for area in bpy.context.window.screen.areas:
        size = area.width * area.height
        if area.type in {'VIEW_3D', 'GRAPH_EDITOR', 'DOPESHEET_EDITOR', 'NLA_EDITOR', 'TIMELINE'}:
            play_area += size
        elif area.type in {'IMAGE_EDITOR', 'SEQUENCE_EDITOR', 'NODE_EDITOR'}:
            render_area += size

        if area.type == 'IMAGE_EDITOR':
            totimg += 1

    # since our test files have this as defacto standard
    scene = bpy.context.scene
    if totimg >= 2 and (scene.camera or scene.render.use_sequencer):
        mode = 'RENDER'
    else:
        if play_area >= render_area:
            mode = 'PLAY'
        else:
            mode = 'RENDER'

    if 0:
        return 'PLAY'

    return mode


def demo_mode_next_file(step=1):

    # support for temp
    if global_config_files[global_state["demo_index"]].get("is_tmp"):
        del global_config_files[global_state["demo_index"]]
        global_state["demo_index"] -= 1

    print(global_state["demo_index"])
    demo_index_next = (global_state["demo_index"] + step) % len(global_config_files)

    if global_state["exit"] and step > 0:
        # check if we cycled
        if demo_index_next < global_state["demo_index"]:
            import sys
            sys.exit(0)

    global_state["demo_index"] = demo_index_next
    print(global_state["demo_index"], "....")
    print("func:demo_mode_next_file", global_state["demo_index"])
    filepath = global_config_files[global_state["demo_index"]]["file"]
    bpy.ops.wm.open_mainfile(filepath=filepath)


def demo_mode_timer_add():
    global_state["timer"] = bpy.context.window_manager.event_timer_add(0.8, bpy.context.window)


def demo_mode_timer_remove():
    if global_state["timer"]:
        bpy.context.window_manager.event_timer_remove(global_state["timer"])
        global_state["timer"] = None


def demo_mode_load_file():
    """ Take care, this can only do limited functions since its running
        before the file is fully loaded.
        Some operators will crash like playing an animation.
    """
    print("func:demo_mode_load_file")
    DemoMode.first_run = True
    bpy.ops.wm.demo_mode('EXEC_DEFAULT')


def demo_mode_temp_file():
    """ Initialize a temp config for the duration of the play time.
        Use this so we can initialize the demo intro screen but not show again.
    """
    assert(global_state["demo_index"] == 0)

    temp_config = global_config_fallback.copy()
    temp_config["anim_time_min"] = 0.0
    temp_config["anim_time_max"] = 60.0
    temp_config["anim_cycles"] = 0  # ensures we switch when hitting the end
    temp_config["mode"] = 'PLAY'
    temp_config["is_tmp"] = True

    global_config_files.insert(0, temp_config)


def demo_mode_init():
    print("func:demo_mode_init")
    DemoKeepAlive.ensure()

    if 1:
        global_config.clear()
        global_config.update(global_config_files[global_state["demo_index"]])

    print(global_config)

    demo_mode_timer_add()

    if global_config["mode"] == 'AUTO':
        global_config["mode"] = demo_mode_auto_select()

    if global_config["mode"] == 'PLAY':
        global_state["last_frame"] = -1
        global_state["anim_cycles"] = 0
        bpy.ops.screen.animation_play()

    elif global_config["mode"] == 'RENDER':
        print("  render")

        # setup scene.
        scene = bpy.context.scene
        scene.render.filepath = "TEMP_RENDER"
        scene.render.image_settings.file_format = 'AVI_JPEG' if global_config["anim_render"] else 'PNG'
        scene.render.use_file_extension = False
        scene.render.use_placeholder = False
        try:
            # XXX - without this rendering will crash because of a bug in blender!
            bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
            if global_config["anim_render"]:
                bpy.ops.render.render('INVOKE_DEFAULT', animation=True)
            else:
                bpy.ops.render.render('INVOKE_DEFAULT')  # write_still=True, no need to write now.

                handle_render_init()

        except RuntimeError:  # no camera for eg:
            import traceback
            traceback.print_exc()

    else:
        raise Exception("Unsupported mode %r" % global_config["mode"])

    global_state["init_time"] = global_state["last_switch"] = time.time()
    global_state["render_time"] = -1.0


def demo_mode_update():
    time_current = time.time()
    time_delta = time_current - global_state["last_switch"]
    time_total = time_current - global_state["init_time"]

    # --------------------------------------------------------------------------
    # ANIMATE MODE
    if global_config["mode"] == 'PLAY':
        frame = bpy.context.scene.frame_current
        # check for exit
        if time_total > global_config["anim_time_max"]:
            demo_mode_next_file()
            return
        # above cycles and minimum display time
        if  (time_total > global_config["anim_time_min"]) and \
            (global_state["anim_cycles"] > global_config["anim_cycles"]):

            # looped enough now.
            demo_mode_next_file()
            return

        # run update funcs
        if global_state["reset_anim"]:
            global_state["reset_anim"] = False
            bpy.ops.screen.animation_cancel(restore_frame=False)
            bpy.ops.screen.animation_play()

        # warning, switching the screen can switch the scene
        # and mess with our last-frame/cycles counting.
        if global_config["anim_screen_switch"]:
            # print(time_delta, 1)
            if time_delta > global_config["anim_screen_switch"]:

                screen = bpy.context.window.screen
                index = bpy.data.screens.keys().index(screen.name)
                screen_new = bpy.data.screens[(index if index > 0 else len(bpy.data.screens)) - 1]
                bpy.context.window.screen = screen_new

                global_state["last_switch"] = time_current

                # if we also switch scenes then reset last frame
                # otherwise it could mess up cycle calc.
                if screen.scene != screen_new.scene:
                    global_state["last_frame"] = -1

                #if global_config["mode"] == 'PLAY':
                if 1:
                    global_state["reset_anim"] = True

        # did we loop?
        if global_state["last_frame"] > frame:
            print("Cycle!")
            global_state["anim_cycles"] += 1

        global_state["last_frame"] = frame

    # --------------------------------------------------------------------------
    # RENDER MODE
    elif global_config["mode"] == 'RENDER':
        if global_state["is_render"]:
            # wait until the time has passed
            # XXX, todo, if rendering an anim we need some way to check its done.
            if global_state["render_time"] == -1.0:
                global_state["render_time"] = time.time()
            else:
                if time.time() - global_state["render_time"] > global_config["display_render"]:
                    handle_render_clear()
                    demo_mode_next_file()
                    return
    else:
        raise Exception("Unsupported mode %r" % global_config["mode"])

# -----------------------------------------------------------------------------
# modal operator


class DemoKeepAlive:
    secret_attr = "_keepalive"

    @staticmethod
    def ensure():
        if DemoKeepAlive.secret_attr not in bpy.app.driver_namespace:
            bpy.app.driver_namespace[DemoKeepAlive.secret_attr] = DemoKeepAlive()

    @staticmethod
    def remove():
        if DemoKeepAlive.secret_attr in bpy.app.driver_namespace:
            del bpy.app.driver_namespace[DemoKeepAlive.secret_attr]

    def __del__(self):
        """ Hack, when the file is loaded the drivers namespace is cleared.
        """
        if DemoMode.enabled:
            demo_mode_load_file()


class DemoMode(bpy.types.Operator):
    bl_idname = "wm.demo_mode"
    bl_label = "Demo"

    enabled = False
    first_run = True

    def cleanup(self, disable=False):
        demo_mode_timer_remove()
        DemoMode.first_run = True

        if disable:
            DemoMode.enabled = False
            DemoKeepAlive.remove()

    def modal(self, context, event):
        # print("DemoMode.modal", global_state["anim_cycles"])
        if not DemoMode.enabled:
            self.cleanup(disable=True)
            return {'CANCELLED'}

        if event.type == 'ESC':
            self.cleanup(disable=True)
            # disable here and not in cleanup because this is a user level disable.
            # which should stay disabled until explicitly enabled again.
            return {'CANCELLED'}

        # print(event.type)
        if DemoMode.first_run:
            DemoMode.first_run = False

            demo_mode_init()
        else:
            demo_mode_update()

        return {'PASS_THROUGH'}

    def execute(self, context):
        print("func:DemoMode.execute:", len(global_config_files), "files")

        use_temp = False

        # load config if not loaded
        if not global_config_files:
            load_config()
            use_temp = True

        if not global_config_files:
            self.report({'INFO'}, "No configuration found with text or file: %s. Run File -> Demo Mode Setup" % DEMO_CFG)
            return {'CANCELLED'}

        if use_temp:
            demo_mode_temp_file()  # play this once through then never again

        # toggle
        if DemoMode.enabled and DemoMode.first_run is False:
            # this actually cancells the previous running instance
            # should never happen now, DemoModeControl is for this.
            return {'CANCELLED'}
        else:
            DemoMode.enabled = True

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}

    def cancel(self, context):
        print("func:DemoMode.cancel")
        # disable here means no running on file-load.
        self.cleanup()

    # call from DemoModeControl
    @classmethod
    def disable(cls):
        if cls.enabled and cls.first_run is False:
            # this actually cancells the previous running instance
            # should never happen now, DemoModeControl is for this.
            cls.enabled = False


class DemoModeControl(bpy.types.Operator):
    bl_idname = "wm.demo_mode_control"
    bl_label = "Control"

    mode = bpy.props.EnumProperty(items=(
            ('PREV', "Prev", ""),
            ('PAUSE', "Pause", ""),
            ('NEXT', "Next", ""),
            ),
                name="Mode")

    def execute(self, context):
        mode = self.mode
        if mode == 'PREV':
            demo_mode_next_file(-1)
        elif mode == 'NEXT':
            demo_mode_next_file(1)
        else:  # pause
            DemoMode.disable()
        return {'FINISHED'}


def menu_func(self, context):
    # print("func:menu_func - DemoMode.enabled:", DemoMode.enabled, "bpy.app.driver_namespace:", DemoKeepAlive.secret_attr not in bpy.app.driver_namespace, 'global_state["timer"]:', global_state["timer"])
    layout = self.layout
    layout.operator_context = 'EXEC_DEFAULT'
    row = layout.row(align=True)
    row.label("Demo Mode:")
    if not DemoMode.enabled:
        row.operator("wm.demo_mode", icon='PLAY', text="")
    else:
        row.operator("wm.demo_mode_control", icon='REW', text="").mode = 'PREV'
        row.operator("wm.demo_mode_control", icon='PAUSE', text="").mode = 'PAUSE'
        row.operator("wm.demo_mode_control", icon='FF', text="").mode = 'NEXT'


def register():
    bpy.utils.register_class(DemoMode)
    bpy.utils.register_class(DemoModeControl)
    bpy.types.INFO_HT_header.append(menu_func)


def unregister():
    bpy.utils.unregister_class(DemoMode)
    bpy.utils.unregister_class(DemoModeControl)
    bpy.types.INFO_HT_header.remove(menu_func)


# -----------------------------------------------------------------------------
# parse args

def load_config(cfg_name=DEMO_CFG):
    namespace = {}
    del global_config_files[:]
    basedir = os.path.dirname(bpy.data.filepath)

    text = bpy.data.texts.get(cfg_name)
    if text is None:
        demo_path = os.path.join(basedir, cfg_name)
        if os.path.exists(demo_path):
            print("Using config file: %r" % demo_path)
            demo_file = open(demo_path, "r")
            demo_data = demo_file.read()
            demo_file.close()
        else:
            demo_data = ""
    else:
        print("Using config textblock: %r" % cfg_name)
        demo_data = text.as_string()
        demo_path = os.path.join(bpy.data.filepath, cfg_name)  # fake

    if not demo_data:
        print("Could not find %r textblock or %r file." % (DEMO_CFG, demo_path))
        return False

    namespace["__file__"] = demo_path

    exec(demo_data, namespace, namespace)

    demo_config = namespace["config"]
    demo_search_path = namespace.get("search_path")
    global_state["exit"] = namespace.get("exit", False)

    if demo_search_path is None:
        print("reading: %r, no search_path found, missing files wont be searched." % demo_path)
    if demo_search_path.startswith("//"):
        demo_search_path = bpy.path.abspath(demo_search_path)
    if not os.path.exists(demo_search_path):
        print("reading: %r, search_path %r does not exist." % (demo_path, demo_search_path))
        demo_search_path = None

    blend_lookup = {}
    # initialize once, case insensitive dict

    def lookup_file(filepath):
        filename = os.path.basename(filepath).lower()

        if not blend_lookup:
            # ensure only ever run once.
            blend_lookup[None] = None

            def blend_dict_items(path):
                for dirpath, dirnames, filenames in os.walk(path):
                    # skip '.git'
                    dirnames[:] = [d for d in dirnames if not d.startswith(".")]
                    for filename in filenames:
                        if filename.lower().endswith(".blend"):
                            filepath = os.path.join(dirpath, filename)
                            yield (filename.lower(), filepath)

            blend_lookup.update(dict(blend_dict_items(demo_search_path)))

        # fallback to orginal file
        return blend_lookup.get(filename, filepath)
    # done with search lookup

    for filecfg in demo_config:
        filepath_test = filecfg["file"]
        if not os.path.exists(filepath_test):
            filepath_test = os.path.join(basedir, filecfg["file"])
        if not os.path.exists(filepath_test):
            filepath_test = lookup_file(filepath_test)  # attempt to get from searchpath
        if not os.path.exists(filepath_test):
            print("Cant find %r or %r, skipping!")
            continue

        filecfg["file"] = os.path.normpath(filepath_test)

        # sanitize
        filecfg["file"] = os.path.abspath(filecfg["file"])
        filecfg["file"] = os.path.normpath(filecfg["file"])
        print("  Adding: %r" % filecfg["file"])
        global_config_files.append(filecfg)

    print("found %d files" % len(global_config_files))

    global_state["basedir"] = basedir

    return bool(global_config_files)


# support direct execution
if __name__ == "__main__":
    register()

    demo_mode_load_file()  # kick starts the modal operator
