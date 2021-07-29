# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport

# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENCE BLOCK #####

"""
Additional links:
    Author Site: http://www.jordiart.com
"""

import bpy
from bpy.types import Operator
from bpy.props import BoolProperty
from collections import defaultdict


# Fast Navigate toggle function
def trigger_fast_navigate(trigger):
    scene = bpy.context.scene.display_tools
    scene.FastNavigateStop = False
    trigger = not trigger


# Control how to display particles during fast navigate
def display_particles(mode, dis_particles):
    scene = bpy.context.scene.display_tools

    for particles in bpy.data.particles:
        if scene.ShowParticles is False:
            particles.draw_method = 'NONE'
        else:
            if particles.type == 'EMITTER':
                particles.draw_method = 'DOT'
                particles.draw_percentage = 100
            else:
                particles.draw_method = 'RENDER'
                particles.draw_percentage = dis_particles

    return dis_particles


# Fast Navigate operator
class FastNavigate(Operator):
    bl_idname = "view3d.fast_navigate_operator"
    bl_label = "Fast Navigate"
    bl_description = ("Limit the objects drawing in the 3D view for faster navigation\n"
                      "Runs in modal mode until Stop is pressed or Esc, Return, Space")

    trigger = BoolProperty(default=False)
    mode = BoolProperty(default=False)
    screen_width = [0, 0]
    store_fail = False
    store_init_particles = {}
    store_viewport_shade = None
    check_particles_draw = False
    fast_particles_draw = 0

    def modal(self, context, event):
        context.area.tag_redraw()
        scene = context.scene.display_tools

        if scene.FastNavigateStop is True:
            self.execute(context)
            return {'FINISHED'}

        if context.area.type != 'VIEW_3D':
            self.execute(context)
            return {'FINISHED'}

        if scene.EditActive is True:
            self.fast_navigate_stuff(context, event)
            return {'PASS_THROUGH'}
        else:
            obj = context.active_object
            if obj:
                if obj.mode != 'EDIT':
                    self.fast_navigate_stuff(context, event)
                    return {'PASS_THROUGH'}
                else:
                    return {'PASS_THROUGH'}
            else:
                self.fast_navigate_stuff(context, event)
                return {'PASS_THROUGH'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type != 'VIEW_3D':
            self.report({'WARNING'},
                        "Fast Navigate: View3D not found. Operation Cancelled")
            return {'CANCELLED'}

        self.store_init_particles = defaultdict(list)
        self.store_fail = False

        context.window_manager.modal_handler_add(self)
        trigger_fast_navigate(self.trigger)
        scene = context.scene.display_tools
        scene.DelayTime = scene.DelayTimeGlobal
        self.get_screen_size(context, scene)
        self.start_settings_store(context, store=False)
        self.check_particles_draw = True

        return {'RUNNING_MODAL'}

    def execute(self, context):
        scene = context.scene.display_tools
        scene.FastNavigateStop = True

        self.start_settings_store(context, store=True)

        mess, mess_txt = ('WARNING', "Fast Navigate: Some Settings could not be restored") if \
                    self.store_fail else ('INFO', "Exited the Fast Navigate mode")

        self.report({mess}, mess_txt)

        return {'FINISHED'}

    @staticmethod
    def calc_delay(scene):
        if scene.Delay is True:
            if scene.DelayTime < scene.DelayTimeGlobal:
                scene.DelayTime += 1

    def start_settings_store(self, context, store=False):
        try:
            view = context.space_data
            shade = view.viewport_shade if view.type == 'VIEW_3D' else None
            if store is False:
                if not shade:
                    self.store_fail = True
                else:
                    self.store_viewport_shade = shade
                for particle in bpy.data.particles:
                    self.store_init_particles[particle.name] = \
                        [particle.draw_method, particle.draw_percentage]
            else:
                if not shade:
                    self.store_fail = True
                else:
                    shade = self.store_viewport_shade or 'SOLID'
                for particle in bpy.data.particles:
                    particle.draw_method = self.store_init_particles[particle.name][0]
                    particle.draw_percentage = self.store_init_particles[particle.name][1]
        except:
            self.store_fail = True

    def get_screen_size(self, context, scene):
        if context.area.type == 'VIEW_3D':
            coord_x = context.area.x + scene.ScreenStart
            coord_max_x = context.area.width - scene.ScreenEnd
            self.screen_width = [coord_x, coord_max_x]

    # Do repetitive fast navigate related stuff
    def fast_navigate_stuff(self, context, event):
        scene = context.scene.display_tools
        view = context.space_data

        if scene.FastNavigateStop is True:
            return {'FINISHED'}

        if context.area.type != 'VIEW_3D':
            scene.FastNavigateStop = True
            return {'FINISHED'}

        if event.type in {'ESC', 'RET', 'SPACE'}:
            scene.FastNavigateStop = True
            return {'CANCELLED'}

        # limit the active area
        if event.mouse_x not in range(self.screen_width[0], self.screen_width[1]):
            return {'PASS_THROUGH'}

        # fast navigate while orbit/panning
        if event.type == 'MIDDLEMOUSE':
            self.calc_delay(scene)
            view.viewport_shade = scene.FastMode
            self.mode = False

        # fast navigate while transform operations
        if event.type in {'G', 'R', 'S'}:
            self.calc_delay(scene)
            view.viewport_shade = scene.FastMode
            self.mode = False

        # fast navigate while menu popups or duplicates
        if event.type in {'W', 'D', 'L', 'U', 'I', 'M', 'A', 'B'}:
            self.calc_delay(scene)
            view.viewport_shade = scene.FastMode
            self.mode = False

        # fast navigate while numpad navigation
        if event.type in {'NUMPAD_PERIOD', 'NUMPAD_1', 'NUMPAD_2', 'NUMPAD_3',
                          'NUMPAD_4', 'NUMPAD_5', 'NUMPAD_6', 'NUMPAD_7',
                          'NUMPAD_8', 'NUMPAD_9'}:
            self.calc_delay(scene)
            view.viewport_shade = scene.FastMode
            self.mode = False

        # fast navigate while zooming with mousewheel too
        if event.type in {'WHEELUPMOUSE', 'WHEELDOWNMOUSE'}:
            scene.DelayTime = scene.DelayTimeGlobal
            view.viewport_shade = scene.FastMode
            self.mode = False

        if event.type == 'MOUSEMOVE':
            if scene.Delay is True:
                if scene.DelayTime == 0:
                    view.viewport_shade = scene.OriginalMode
                    scene.DelayTime = scene.DelayTimeGlobal
                    self.mode = True
            else:
                view.viewport_shade = scene.OriginalMode
                self.mode = True

        if scene.Delay is True:
            scene.DelayTime -= 1
            if scene.DelayTime == 0:
                view.viewport_shade = scene.OriginalMode
                scene.DelayTime = scene.DelayTimeGlobal
                self.mode = True

        # update particles draw (only update call when the fast draw or the percentage are changed)
        self.check_particles_draw = not self.mode
        dis_particles = scene.InitialParticles if self.mode else scene.ParticlesPercentageDisplay

        if self.check_particles_draw is True:
            self.fast_particles_draw = display_particles(self.mode, dis_particles)
        else:
            if self.fast_particles_draw != dis_particles:
                self.fast_particles_draw = display_particles(self.mode, dis_particles)


# Fast Navigate Stop
def fast_navigate_stop(context):
    scene = context.scene.display_tools
    scene.FastNavigateStop = True


# Fast Navigate Stop Operator
class FastNavigateStop(Operator):
    bl_idname = "view3d.fast_navigate_stop"
    bl_label = "Stop"
    bl_description = "Stop Fast Navigate"

    def execute(self, context):
        fast_navigate_stop(context)

        return {'FINISHED'}


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
