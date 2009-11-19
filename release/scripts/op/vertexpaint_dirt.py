# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

# History
#
# Originally written by Campbell Barton aka ideasman42
#
# 2009-11-01: * 2.5 port by Keith "Wahooney" Boshoff
#              * Replaced old method with my own, speed is similar (about 0.001 sec on Suzanne)
#               but results are far more accurate
#

import bpy
import Mathutils
import math
import time

from Mathutils import Vector
from bpy.props import *

def applyVertexDirt(me, blur_iterations, blur_strength, clamp_dirt, clamp_clean, dirt_only):
##    Window.WaitCursor(1)

    #BPyMesh.meshCalcNormals(me)

    vert_tone = [0.0] * len(me.verts)

    min_tone =180.0
    max_tone =0.0

    # create lookup table for each vertex's connected vertices (via edges)
    con = []

    con = [[] for i in range(len(me.verts))]

    # add connected verts
    for e in me.edges:
        con[e.verts[0]].append(e.verts[1])
        con[e.verts[1]].append(e.verts[0])

    for v in me.verts:
        vec = Vector()
        no = v.normal
        co = v.co

        # get the direction of the vectors between the vertex and it's connected vertices
        for c in con[v.index]:
            vec += Vector(me.verts[c].co - co).normalize()

        # normalize the vector by dividing by the number of connected verts
        vec /= len(con[v.index])

        # angle is the acos of the dot product between vert and connected verts normals
        ang = math.acos(no.dot(vec))

        # enforce min/max
        ang = max(clamp_dirt, ang)

        if not dirt_only:
            ang = min(clamp_clean, ang)

        vert_tone[v.index] = ang

    # blur tones
    for i in range(blur_iterations):
        # backup the original tones
        orig_vert_tone = list(vert_tone)

        # use connected verts look up for blurring
        for j, c in enumerate(con):
            for v in c:
                vert_tone[j] += blur_strength * orig_vert_tone[v]

            vert_tone[j] /= len(c) * blur_strength + 1

    min_tone = min(vert_tone)
    max_tone = max(vert_tone)

    # debug information
    # print(min_tone * 2 * math.pi)
    # print(max_tone * 2 * math.pi)
    # print(clamp_clean)
    # print(clamp_dirt)

    tone_range = max_tone-min_tone

    if not tone_range:
        return

    active_col_layer = None

    if len(me.vertex_colors):
        for lay in me.vertex_colors:
            if lay.active:
                active_col_layer = lay.data
    else:
        bpy.ops.mesh.vertex_color_add()
        me.vertex_colors[0].active = True
        active_col_layer = me.vertex_colors[0].data

    if not active_col_layer:
        return('CANCELLED', )

    for i, f in enumerate(me.faces):
        if not me.use_paint_mask or f.selected:

            f_col = active_col_layer[i]

            f_col = [f_col.color1, f_col.color2, f_col.color3, f_col.color4]

            for j, v in enumerate(f.verts):
                col = f_col[j]
                tone = vert_tone[me.verts[v].index]
                tone = (tone-min_tone)/tone_range

                if dirt_only:
                    tone = min(tone, 0.5)
                    tone *= 2

                col[0] = tone*col[0]
                col[1] = tone*col[1]
                col[2] = tone*col[2]

##    Window.WaitCursor(0)

class VertexPaintDirt(bpy.types.Operator):

    bl_idname = "mesh.vertex_paint_dirt"
    bl_label = "Dirty Vertex Colors"
    bl_register = True
    bl_undo = True

    blur_strength = FloatProperty(name = "Blur Strength", description = "Blur strength per iteration", default = 1.0, min = 0.01, max = 1.0)
    blur_iterations = IntProperty(name = "Blur Iterations", description = "Number times to blur the colors. (higher blurs more)", default = 1, min = 0, max = 40)
    clean_angle = FloatProperty(name = "Highlight Angle", description = "Less then 90 limits the angle used in the tonal range", default = 180.0, min = 0.0, max = 180.0)
    dirt_angle = FloatProperty(name = "Dirt Angle", description = "Less then 90 limits the angle used in the tonal range", default = 0.0, min = 0.0, max = 180.0)
    dirt_only = BoolProperty(name= "Dirt Only", description = "Dont calculate cleans for convex areas", default = False)

    def execute(self, context):
        sce = context.scene
        ob = context.object

        if not ob or ob.type != 'MESH':
            print('Error, no active mesh object, aborting.')
            return('CANCELLED',)

        me = ob.data

        t = time.time()

        applyVertexDirt(me, self.properties.blur_iterations, self.properties.blur_strength, math.radians(self.properties.dirt_angle), math.radians(self.properties.clean_angle), self.properties.dirt_only)

        print('Dirt calculated in %.6f' % (time.time()-t))

        return('FINISHED',)

bpy.ops.add(VertexPaintDirt)

if __name__ == "__main__":
    bpy.ops.mesh.vertex_paint_dirt()