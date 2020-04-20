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

import bpy

class NewSimulation(bpy.types.Operator):
    """Create a new simulation data block and edit it in the opened simulation editor"""

    bl_idname = "simulation.new"
    bl_label = "New Simulation"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.area.type == 'NODE_EDITOR' and context.space_data.tree_type == 'SimulationNodeTree'

    def execute(self, context):
        simulation = bpy.data.simulations.new("Simulation")
        context.space_data.simulation = simulation
        return {'FINISHED'}

classes = (
    NewSimulation,
)
