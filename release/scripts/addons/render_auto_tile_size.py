# BEGIN GPL LICENSE BLOCK #####
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
# END GPL LICENSE BLOCK #####

bl_info = {
    "name": "Auto Tile Size",
    "description": "Estimate and set the tile size that will render the fastest",
    "author": "Greg Zaal",
    "version": (3, 1, 2),
    "blender": (2, 74, 0),
    "location": "Render Settings > Performance",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php?title=Extensions:2.6/Py/"
                "Scripts/Render/Auto_Tile_Size",
    "category": "Render",
}


import bpy
from bpy.types import (
        Operator,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatVectorProperty,
        IntProperty,
        IntVectorProperty,
        StringProperty,
        PointerProperty,
        )
from bpy.app.handlers import persistent
from math import (
        ceil, floor,
        sqrt,
        )


SUPPORTED_RENDER_ENGINES = {'CYCLES', 'BLENDER_RENDER'}
TILE_SIZES = (
    ('16', "16", "16 x 16"),
    ('32', "32", "32 x 32"),
    ('64', "64", "64 x 64"),
    ('128', "128", "128 x 128"),
    ('256', "256", "256 x 256"),
    ('512', "512", "512 x 512"),
    ('1024', "1024", "1024 x 1024"),
)


def _update_tile_size(self, context):
    do_set_tile_size(context)


class AutoTileSizeSettings(PropertyGroup):
    gpu_choice = EnumProperty(
            name="Target GPU Tile Size",
            items=TILE_SIZES,
            default='256',
            description="Square dimensions of tiles for GPU rendering",
            update=_update_tile_size
            )
    cpu_choice = EnumProperty(
            name="Target CPU Tile Size",
            items=TILE_SIZES,
            default='32',
            description="Square dimensions of tiles for CPU rendering",
            update=_update_tile_size
            )
    bi_choice = EnumProperty(
            name="Target CPU Tile Size",
            items=TILE_SIZES,
            default='64',
            description="Square dimensions of tiles",
            update=_update_tile_size
            )
    gpu_custom = IntProperty(
            name="Target Size",
            default=256,
            min=8,  # same as blender's own limits
            max=65536,
            description="Custom target tile size for GPU rendering",
            update=_update_tile_size
            )
    cpu_custom = IntProperty(
            name="Target Size",
            default=32,
            min=8,  # same as blender's own limits
            max=65536,
            description="Custom target tile size for CPU rendering",
            update=_update_tile_size
            )
    bi_custom = IntProperty(
            name="Target Size",
            default=64,
            min=8,  # same as blender's own limits
            max=65536,
            description="Custom target tile size",
            update=_update_tile_size
            )
    target_type = EnumProperty(
            name="Target tile size",
            items=(
                ('po2', "Po2", "A choice between powers of 2 (16, 32, 64...)"),
                ('custom', "Custom", "Choose any number as the tile size target")),
            default='po2',
            description="Method of choosing the target tile size",
            update=_update_tile_size
            )
    use_optimal = BoolProperty(
            name="Optimal Tiles",
            default=True,
            description="Try to find a similar tile size for best performance, "
                        "instead of using exact selected one",
            update=_update_tile_size
            )
    is_enabled = BoolProperty(
            name="Auto Tile Size",
            default=True,
            description="Calculate the best tile size based on factors of the "
                        "render size and the chosen target",
            update=_update_tile_size
            )
    use_advanced_ui = BoolProperty(
            name="Advanced Settings",
            default=False,
            description="Show extra options for more control over the calculated tile size"
            )
    thread_error_correct = BoolProperty(
            name="Fix",
            default=True,
            description="Reduce the tile size so that all your available threads are used",
            update=_update_tile_size
            )

    # Internally used props (not for GUI)
    first_run = BoolProperty(
            default=True,
            options={'HIDDEN'}
            )
    threads_error = BoolProperty(
            options={'HIDDEN'}
            )
    num_tiles = IntVectorProperty(
            default=(0, 0),
            size=2,
            options={'HIDDEN'}
            )
    prev_choice = StringProperty(
            default='',
            options={'HIDDEN'}
            )
    prev_engine = StringProperty(
            default='',
            options={'HIDDEN'}
            )
    prev_device = StringProperty(
            default='',
            options={'HIDDEN'}
            )
    prev_res = IntVectorProperty(
            default=(0, 0),
            size=2,
            options={'HIDDEN'}
            )
    prev_border = BoolProperty(
            default=False,
            options={'HIDDEN'}
            )
    prev_border_res = FloatVectorProperty(
            default=(0, 0, 0, 0),
            size=4,
            options={'HIDDEN'}
            )
    prev_actual_tile_size = IntVectorProperty(
            default=(0, 0),
            size=2,
            options={'HIDDEN'}
            )
    prev_threads = IntProperty(
            default=0,
            options={'HIDDEN'}
            )


def ats_poll(context):
    scene = context.scene
    if scene.render.engine not in SUPPORTED_RENDER_ENGINES or not scene.ats_settings.is_enabled:
        return False
    return True


def engine_is_gpu(engine, device, userpref):
    if engine == 'CYCLES' and device == 'GPU':
        return userpref.addons['cycles'].preferences.has_active_device()
    return False


def get_tilesize_prop(engine, device, userpref):
    target_type = "_choice" if bpy.context.scene.ats_settings.target_type == 'po2' else "_custom"
    if engine_is_gpu(engine, device, userpref):
        return ("gpu" + target_type)
    elif engine == 'CYCLES':
        return ("cpu" + target_type)
    return ("bi" + target_type)


@persistent
def on_scene_update(scene):
    context = bpy.context

    if not ats_poll(context):
        return

    userpref = context.user_preferences

    settings = scene.ats_settings
    render = scene.render
    engine = render.engine

    # scene.cycles might not always exist (Cycles is an addon)...
    device = scene.cycles.device if engine == 'CYCLES' else settings.prev_device
    border = render.use_border
    threads = get_threads(context, device)

    choice = getattr(settings, get_tilesize_prop(engine, device, userpref))

    res = get_actual_res(render)
    actual_ts = (render.tile_x, render.tile_y)
    border_res = (render.border_min_x, render.border_min_y, render.border_max_x, render.border_max_y)

    # detect relevant changes in scene
    do_change = (engine != settings.prev_engine or
                 device != settings.prev_device or
                 border != settings.prev_border or
                 threads != settings.prev_threads or
                 str(choice) != settings.prev_choice or
                 res != settings.prev_res[:] or
                 border_res != settings.prev_border_res[:] or
                 actual_ts != settings.prev_actual_tile_size[:])
    if do_change:
        do_set_tile_size(context)


def get_actual_res(render):
    rend_percent = render.resolution_percentage * 0.01
    # floor is implicitly done by int conversion...
    return (int(render.resolution_x * rend_percent), int(render.resolution_y * rend_percent))


def get_threads(context, device):
    render = context.scene.render
    engine = render.engine
    userpref = context.user_preferences

    if engine_is_gpu(engine, device, userpref):
        threads = userpref.addons['cycles'].preferences.get_num_gpu_devices()
    else:
        threads = render.threads

    return threads


def max_tile_size(threads, xres, yres):
    ''' Give the largest tile size that will still use all threads '''

    render_area = xres * yres
    tile_area = render_area / threads
    tile_length = sqrt(tile_area)

    # lists: num x tiles, num y tiles, squareness, total tiles
    perfect_attempts = []  # attempts with correct number of tiles
    attempts = []          # all attempts, even if incorrect number of tiles

    axes = [xres, yres]
    funcs = [floor, ceil]

    for axis in axes:
        sec_axis = yres if axis == xres else xres
        for func in funcs:
            primary = func(axis / tile_length)
            if primary > 0:
                secondary = threads / primary
                ts_p = axis / primary
                ts_s = sec_axis / secondary
                squareness = max(ts_p, ts_s) - min(ts_p, ts_s)
                attempt = [primary if axis == xres else secondary, primary if
                           axis != xres else secondary, squareness, primary * secondary]
                if attempt not in attempts:
                    attempts.append(attempt)
                    # will only be an integer if there are the right number of tiles
                    if secondary.is_integer():
                        perfect_attempts.append(attempt)

    if perfect_attempts:  # prefer to use attempt that has exactly the right number of tiles
        attempts = perfect_attempts

    attempt = sorted(attempts, key=lambda k: k[2])[0]  # pick set with most square tiles
    numtiles_x = round(attempt[0])
    numtiles_y = round(attempt[1])
    tile_x = ceil(xres / numtiles_x)
    tile_y = ceil(yres / numtiles_y)

    return (tile_x, tile_y)


def do_set_tile_size(context):
    if not ats_poll(context):
        return False

    scene = context.scene
    userpref = context.user_preferences

    settings = scene.ats_settings
    render = scene.render
    engine = render.engine
    device = scene.cycles.device if engine == 'CYCLES' else settings.prev_device
    border = render.use_border

    realxres, realyres = xres, yres = res = get_actual_res(scene.render)

    if border:
        xres = round(xres * (render.border_max_x - render.border_min_x))
        yres = round(yres * (render.border_max_y - render.border_min_y))

    choice = getattr(settings, get_tilesize_prop(engine, device, userpref))
    target = int(choice)

    numtiles_x = ceil(xres / target)
    numtiles_y = ceil(yres / target)
    settings.num_tiles = (numtiles_x, numtiles_y)
    if settings.use_optimal:
        tile_x = ceil(xres / numtiles_x)
        tile_y = ceil(yres / numtiles_y)
    else:
        tile_x = target
        tile_y = target

    # Print tile size (for debug purposes)
    # print("Tile size: %dx%d (%dx%d tiles)" % (tile_x, tile_y, ceil(xres / tile_x), ceil(yres / tile_y)))

    # Detect if there are fewer tiles than available threads
    threads = get_threads(context, device)
    if ((numtiles_x * numtiles_y) < threads):
        settings.threads_error = True
        if settings.thread_error_correct:
            tile_x, tile_y = max_tile_size(threads, xres, yres)
            settings.num_tiles = (ceil(xres / tile_x), ceil(yres / tile_y))
    else:
        settings.threads_error = False

    # Make sure tile sizes are within the internal limit
    tile_x = max(8, tile_x)
    tile_y = max(8, tile_y)
    tile_x = min(65536, tile_x)
    tile_y = min(65536, tile_y)

    render.tile_x = tile_x
    render.tile_y = tile_y

    settings.prev_engine = engine
    settings.prev_device = device
    settings.prev_border = border
    settings.prev_threads = threads
    settings.prev_choice = str(choice)
    settings.prev_res = res
    settings.prev_border_res = (render.border_min_x, render.border_min_y,
                                render.border_max_x, render.border_max_y)
    settings.prev_actual_tile_size = (tile_x, tile_y)
    settings.first_run = False

    return True


class SetTileSize(Operator):
    bl_idname = "render.autotilesize_set"
    bl_label = "Set"
    bl_description = "The first render may not obey the tile-size set here"

    @classmethod
    def poll(clss, context):
        return ats_poll(context)

    def execute(self, context):
        if do_set_tile_size(context):
            return {'FINISHED'}
        return {'CANCELLED'}


# ##### INTERFACE #####

def ui_layout(engine, layout, context):
    scene = context.scene
    userpref = context.user_preferences

    settings = scene.ats_settings
    render = scene.render
    engine = render.engine
    device = scene.cycles.device if engine == 'CYCLES' else settings.prev_device

    col = layout.column(align=True)
    sub = col.column(align=True)
    row = sub.row(align=True)
    row.prop(settings, "is_enabled", toggle=True)
    row.prop(settings, "use_advanced_ui", toggle=True, text="", icon='PREFERENCES')

    sub = col.column(align=False)
    sub.enabled = settings.is_enabled

    if settings.use_advanced_ui:
        row = sub.row(align=True)
        row.label("Target tile size:")
        row.separator()
        row.prop(settings, "target_type", expand=True)

        row = sub.row(align=True)
        row.prop(settings, get_tilesize_prop(engine, device, userpref), expand=True)
        sub.prop(settings, "use_optimal", text="Calculate Optimal Size")

        sub.label("Number of tiles: %s x %s (Total: %s)" %
                 (settings.num_tiles[0], settings.num_tiles[1],
                 settings.num_tiles[0] * settings.num_tiles[1])
                 )

    if settings.first_run:
        sub = layout.column(align=True)
        sub.operator("render.autotilesize_set", text="First-render fix", icon='ERROR')
    elif settings.prev_device != device:
        sub = layout.column(align=True)
        sub.operator("render.autotilesize_set", text="Device changed - fix", icon='ERROR')

    # if not very square tile
    if (render.tile_x / render.tile_y > 2) or (render.tile_x / render.tile_y < 0.5):
        sub.label(text="Warning: Tile size is not very square", icon='ERROR')
        sub.label(text="    Try a slightly different resolution")

    if settings.threads_error:
        row = sub.row(align=True)
        row.alignment = 'CENTER'
        row.label(text="Warning: Fewer tiles than threads", icon='ERROR')
        row.prop(settings, 'thread_error_correct')


def menu_func_cycles(self, context):
    ui_layout('CYCLES', self.layout, context)


def menu_func_bi(self, context):
    ui_layout('BLENDER_RENDER', self.layout, context)


# ##### REGISTRATION #####

def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.ats_settings = PointerProperty(
                                        type=AutoTileSizeSettings
                                        )

    # Note, the Cycles addon must be registered first, otherwise
    # this panel doesn't exist - better be safe here!
    cycles_panel = getattr(bpy.types, "CyclesRender_PT_performance", None)
    if cycles_panel is not None:
        cycles_panel.append(menu_func_cycles)

    bpy.types.RENDER_PT_performance.append(menu_func_bi)
    bpy.app.handlers.scene_update_post.append(on_scene_update)


def unregister():
    bpy.app.handlers.scene_update_post.remove(on_scene_update)
    bpy.types.RENDER_PT_performance.remove(menu_func_bi)

    cycles_panel = getattr(bpy.types, "CyclesRender_PT_performance", None)
    if cycles_panel is not None:
        cycles_panel.remove(menu_func_cycles)

    del bpy.types.Scene.ats_settings

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
