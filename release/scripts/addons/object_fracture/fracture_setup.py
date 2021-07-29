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
from bpy.props import *


def getsizefrommesh(ob):
    bb = ob.bound_box
    return(
        bb[5][0] - bb[0][0],
        bb[3][1] - bb[0][1],
        bb[1][2] - bb[0][2])


def setupshards(context):
    sce = context.scene
    #print(dir(context))
    #bpy.data.scenes[0].game_settings.all_frames

    tobeprocessed = []
    for ob in sce.objects:
        if ob.select:
            tobeprocessed.append(ob)

    for ob in tobeprocessed:
        g = ob.game

        g.physics_type = 'RIGID_BODY'
        g.use_collision_bounds = 1
        g.collision_bounds_type = 'CONVEX_HULL'
        g.rotation_damping = 0.9

        sizex, sizey, sizez = getsizefrommesh(ob)
        approxvolume = sizex * sizey * sizez
        g.mass = approxvolume

        sce.objects.active = ob

        bpy.ops.object.game_property_new()
        g.properties['prop'].name = 'shard'
        #sm=FloatProperty(name='shard',description='shardprop',default=0.0)
        #print (sm)
        #np=bpy.types.GameFloatProperty(sm)
        #name='shard',type='BOOL', value=1
        #print(ob)


class SetupFractureShards(bpy.types.Operator):
    """"""
    bl_idname = "object.setup_fracture_shards"
    bl_label = "Setup Fracture Shards"
    bl_options = {'REGISTER', 'UNDO'}

    #def poll(self, context):

    def execute(self, context):
        setupshards(context)
        return {'FINISHED'}
