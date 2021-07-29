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
    "name": "Auto Render Tile Size",
    "description": "Estimate the tile size that will render the fastest",
    "author": "Greg Zaal",
    "version": (2, 3),
    "blender": (2, 71, 0),
    "location": "Render Settings > Performance",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php?title=Extensions:2.6/Py/Scripts/Render/Auto_Tile_Size",
    "tracker_url": "https://projects.blender.org/tracker/index.php?func=detail&aid=36785&group_id=153&atid=467",
    "category": "Render"}

import bpy
from bpy.app.handlers import persistent


'''
TODO
    Make sure tile size is similar to the target (can sometimes be way off with strange resolutions)
    When using a very narrow border region, it wrongly detects that there are not enough threads.
'''


# dummy report function
def _report_print(report_type, text):
    print(text)

@persistent
def on_scene_update(context, report=_report_print):
    scene = bpy.context.scene

    if scene.render.engine not in ['CYCLES', 'BLENDER_RENDER']:
        return False

    renderer = scene.render.engine
    prevrenderer = scene.TileSizePrevRenderer

    device = scene.cycles.device
    prevdevice = scene.TileSizePrevDevice

    choice = 0
    if (device == 'GPU' and bpy.context.user_preferences.system.compute_device_type != 'NONE') and scene.render.engine == 'CYCLES':
        choice = scene.TileSizeGPUChoice
    elif (device == 'CPU' or bpy.context.user_preferences.system.compute_device_type == 'NONE') and scene.render.engine == 'CYCLES':
        choice = scene.TileSizeCPUChoice
    elif scene.render.engine == 'BLENDER_RENDER':
        choice = scene.TileSizeBIChoice
    prevchoice = int(scene.TileSizePrevChoice)

    res = str(int(getActualRes('x')))+'x'+str(int(getActualRes('y')))
    prevres = scene.TileSizePrevRes

    actual_ts = str(scene.render.tile_x)+'x'+str(scene.render.tile_y)
    prevactual_ts = scene.TileSizePrevActualTileSize

    actual_border = (str(scene.render.border_min_x)+"x"+str(scene.render.border_min_y))+'-'+(str(scene.render.border_max_x)+"x"+str(scene.render.border_max_y))
    prev_border = scene.TileSizePrevBorderRes

    # detect relevant changes in scene
    change_triggers = [renderer != prevrenderer,
                       device != prevdevice,
                       int(choice) != prevchoice,
                       res != prevres,
                       actual_border != prev_border,
                       actual_ts != prevactual_ts]
    if any(change_triggers) and scene.TileSizeEnable:
        scene.TileSizePrevRenderer = renderer
        scene.TileSizePrevDevice = device
        scene.TileSizePrevChoice = choice
        scene.TileSizePrevBorderRes = actual_border
        do_set_tile_size(context, report=_report_print)
    else:
        pass

    return True


def do_set_tile_size(context, report=_report_print):
    context = bpy.context
    scene = context.scene

    if scene.render.engine not in ['CYCLES', 'BLENDER_RENDER']:
        print("Auto Tile Size is not supported for this renderer")
        return False

    
    scene.TileSizeFirstRun = False
    device = scene.cycles.device
    target = 0
    xres = int(getActualRes('x'))
    yres = int(getActualRes('y'))
    realxres = xres
    realyres = yres

    if context.scene.render.use_border:
        xres = int(xres * (context.scene.render.border_max_x - context.scene.render.border_min_x))
        yres = int(yres * (context.scene.render.border_max_y - context.scene.render.border_min_y))

    if (device == 'GPU' and context.user_preferences.system.compute_device_type != 'NONE') and scene.render.engine == 'CYCLES':
        target = int(scene.TileSizeGPUChoice)
    elif (device == 'CPU' or context.user_preferences.system.compute_device_type == 'NONE') and scene.render.engine == 'CYCLES':
        target = int(scene.TileSizeCPUChoice)
    elif scene.render.engine == 'BLENDER_RENDER':
        target = int(scene.TileSizeBIChoice)
    else:
        report({'ERROR'}, "Failed to get compute device")
        return False

    scene.TileSizePrevChoice = str(target)
    scene.TileSizePrevDevice = device
    if scene.TileSizeClosestFactor:
        if isprime(xres):
            xres += 1
        if isprime(yres):
            yres += 1
        xfactors = list(xres/z for z in range (1, xres) if xres/z % 1 == 0) # get factors
        yfactors = list(yres/z for z in range (1, yres) if yres/z % 1 == 0) # get factors
        xtile = int(min(xfactors, key=lambda x: abs(x - target)))
        ytile = int(min(yfactors, key=lambda x: abs(x - target)))
        report({'INFO'}, str(xtile) + "x" + str(ytile) + " (" + str(int(xres / xtile)) + "x" + str(int(yres / ytile)) + " tiles)")
    else:
        xtile = target
        ytile = target
        report({'INFO'}, str(xtile) + "x" + str(ytile) + " (" + str(xres / xtile) + " x " + str(yres / ytile) + " tiles)")

    # Detect if there are fewer tiles than available threads
    if (int(xres / xtile) * int(yres / ytile) < scene.render.threads) and device != 'GPU':
        scene.TileSizeThreadsError = True
    else:
        scene.TileSizeThreadsError = False


    scene.render.tile_x = xtile
    scene.render.tile_y = ytile

    scene.TileSizePrevRes = str(realxres)+'x'+str(realyres)
    scene.TileSizePrevActualTileSize = str(scene.render.tile_x)+'x'+str(scene.render.tile_y)

    return True


def isprime(num):
    for x in range(2, int(num ** 0.5) + 1):
        if num % x == 0:
            return False
    return True


def getActualRes(xy):
    rend = bpy.context.scene.render
    rend_percent = rend.resolution_percentage * 0.01
    x = (str(rend.resolution_x * rend_percent).split('.'))[0]
    y = (str(rend.resolution_y * rend_percent).split('.'))[0]
    returnvar = x
    if xy == "y":
        returnvar = y
    return (returnvar)


class SetTileSize(bpy.types.Operator):

    'The first render may not obey the tile-size set here'
    bl_idname = 'autotile.set'
    bl_label = 'Set'

    def execute(self, context):
        context.scene.TileSizeFirstRun = False
        if do_set_tile_size(context, self.report):
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

def ui_layout(renderer, layout, context):
    scene = context.scene
    col = layout.column(align=False)
    row = col.row(align=True)
    row.prop(scene, 'TileSizeEnable', toggle=True)
    row.prop(scene, 'TileSizeAdvancedUI', toggle=True, text = '', icon = 'PREFERENCES')
    if scene.TileSizeAdvancedUI:
        col.label("Target tile size:")

        row = col.row(align=False)
        if renderer == 'CYCLES':
            if scene.cycles.device == 'GPU' and context.user_preferences.system.compute_device_type != 'NONE' and scene.render.engine == 'CYCLES':
                row.prop(scene, 'TileSizeGPUChoice', expand=True)
            elif scene.cycles.device == 'CPU' or context.user_preferences.system.compute_device_type == 'NONE' and scene.render.engine == 'CYCLES':
                row.prop(scene, 'TileSizeCPUChoice', expand=True)
        elif renderer == 'BLENDER_RENDER':
            row.prop(scene, 'TileSizeBIChoice', expand=True)

        col.prop(scene, 'TileSizeClosestFactor', text='Consistent Tiles')

        if scene.TileSizeFirstRun:
            col = layout.column(align=True)
            col.operator('autotile.set', text="First-render fix", icon='ERROR')
        elif scene.TileSizePrevDevice != scene.cycles.device:
            col = layout.column(align=True)
            col.operator('autotile.set', text="Device changed - fix", icon='ERROR')
        if (scene.render.tile_x/scene.render.tile_y > 2) or (scene.render.tile_x/scene.render.tile_y < 0.5): # if not very square tile
            col.label (text="Warning: Tile size is not very square", icon='ERROR')
            col.label (text="    Try a slightly different resolution")
            col.label (text="    or disable Consistent Tiles")
        if scene.TileSizeThreadsError:
            col.label (text="Warning: There are fewer tiles than render threads", icon='ERROR')


def menu_func_cycles(self, context):
    layout = self.layout
    ui_layout('CYCLES', layout, context)

def menu_func_bi(self, context):
    layout = self.layout
    ui_layout('BLENDER_RENDER', layout, context)


def _update_tile_size(self, context):
    do_set_tile_size(context)


def register():
    bpy.types.Scene.TileSizeGPUChoice = bpy.props.EnumProperty(
        name="Target GPU Tile Size",
        items=(("16", "16", "16 x 16"), 
               ("32", "32", "32 x 32"), 
               ("64", "64", "64 x 64"), 
               ("128", "128", "128 x 128"), 
               ("256", "256", "256 x 256"), 
               ("512", "512", "512 x 512"), 
               ("1024", "1024", "1024 x 1024")),
        default='256',
        description="Square dimentions of tiles",
        update=_update_tile_size)
    bpy.types.Scene.TileSizeCPUChoice = bpy.props.EnumProperty(
        name="Target CPU Tile Size",
        items=(("16", "16", "16 x 16"), 
               ("32", "32", "32 x 32"), 
               ("64", "64", "64 x 64"), 
               ("128", "128", "128 x 128"), 
               ("256", "256", "256 x 256"), 
               ("512", "512", "512 x 512"), 
               ("1024", "1024", "1024 x 1024")),
        default='32',
        description="Square dimentions of tiles",
        update=_update_tile_size)
    bpy.types.Scene.TileSizeBIChoice = bpy.props.EnumProperty(
        name="Target CPU Tile Size",
        items=(("16", "16", "16 x 16"), 
               ("32", "32", "32 x 32"), 
               ("64", "64", "64 x 64"), 
               ("128", "128", "128 x 128"), 
               ("256", "256", "256 x 256"), 
               ("512", "512", "512 x 512"), 
               ("1024", "1024", "1024 x 1024")),
        default='64',
        description="Square dimentions of tiles",
        update=_update_tile_size)
    bpy.types.Scene.TileSizeClosestFactor = bpy.props.BoolProperty(
        name="Consistent Tiles",
        default=True,
        description="Makes sure that all tiles are the same size (improves render speed)",
        update=_update_tile_size)
    bpy.types.Scene.TileSizeEnable = bpy.props.BoolProperty(
        name="Auto Tile Size",
        default=True,
        description="Calculate the best tile size based on factors of the render size and the chosen target",
        update=_update_tile_size)
    bpy.types.Scene.TileSizeFirstRun = bpy.props.BoolProperty(
        name="FirstRun",
        default=True,
        description="FirstRun")
    bpy.types.Scene.TileSizePrevChoice = bpy.props.StringProperty(
        name="prevchoice",
        default='0',
        description="prevchoice")
    bpy.types.Scene.TileSizePrevRenderer = bpy.props.StringProperty(
        name="prevrenderer",
        default='prevrenderer',
        description="prevrenderer")
    bpy.types.Scene.TileSizePrevDevice = bpy.props.StringProperty(
        name="prevdevice",
        default='prevdevice',
        description="prevdevice")
    bpy.types.Scene.TileSizePrevRes = bpy.props.StringProperty(
        name="prevres",
        default='prevres',
        description="prevres")
    bpy.types.Scene.TileSizePrevBorderRes = bpy.props.StringProperty(
        name="prevborder",
        default="prevborder",
        description="prevborder")
    bpy.types.Scene.TileSizeThreadsError = bpy.props.BoolProperty(
        name="ThreadsError",
        description="ThreadsError")
    bpy.types.Scene.TileSizePrevActualTileSize = bpy.props.StringProperty(
        name="prevres",
        default='prevres',
        description="prevres")
    bpy.types.Scene.TileSizeAdvancedUI = bpy.props.BoolProperty(
        name="Advanced Settings",
        default=False,
        description="Show extra options for more control over the calculated tile size")

    bpy.types.CyclesRender_PT_performance.append(menu_func_cycles)
    bpy.types.RENDER_PT_performance.append(menu_func_bi)
    bpy.app.handlers.scene_update_post.append(on_scene_update)

    bpy.utils.register_module(__name__)


def unregister():
    del bpy.types.Scene.TileSizeGPUChoice
    del bpy.types.Scene.TileSizeCPUChoice
    del bpy.types.Scene.TileSizeBIChoice
    del bpy.types.Scene.TileSizeClosestFactor
    del bpy.types.Scene.TileSizeEnable
    del bpy.types.Scene.TileSizeFirstRun
    del bpy.types.Scene.TileSizePrevChoice
    del bpy.types.Scene.TileSizePrevRenderer
    del bpy.types.Scene.TileSizePrevDevice
    del bpy.types.Scene.TileSizePrevRes
    del bpy.types.Scene.TileSizePrevBorderRes
    del bpy.types.Scene.TileSizeThreadsError
    del bpy.types.Scene.TileSizePrevActualTileSize
    del bpy.types.Scene.TileSizeAdvancedUI

    bpy.types.CyclesRender_PT_performance.remove(menu_func_cycles)
    bpy.types.RENDER_PT_performance.remove(menu_func_bi)
    bpy.app.handlers.scene_update_post.remove(on_scene_update)

    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
