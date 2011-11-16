#
# Copyright 2011, Blender Foundation.
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

# <pep8 compliant>

from bl_operators.presets import AddPresetBase
from bpy.types import Operator


class AddPresetIntegrator(AddPresetBase, Operator):
    '''Add an Integrator Preset'''
    bl_idname = "render.cycles_integrator_preset_add"
    bl_label = "Add Integrator Preset"
    preset_menu = "CYCLES_MT_integrator_presets"

    preset_defines = [
        "cycles = bpy.context.scene.cycles"
    ]

    preset_values = [
        "cycles.max_bounces",
        "cycles.min_bounces",
        "cycles.no_caustics",
        "cycles.diffuse_bounces",
        "cycles.glossy_bounces",
        "cycles.transmission_bounces",
        "cycles.transparent_min_bounces",
        "cycles.transparent_max_bounces"
    ]

    preset_subdir = "cycles/integrator"


def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
