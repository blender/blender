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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****
# --------------------------------------------------------------------------

# <pep8 compliant>


def get_vcolor_layer_data(me):
    for lay in me.vertex_colors:
        if lay.active:
            return lay.data

    lay = me.vertex_colors.new()
    lay.active = True
    return lay.data


def applyVertexDirt(me, blur_iterations, blur_strength, clamp_dirt, clamp_clean, dirt_only, normalize):
    from mathutils import Vector
    from math import acos
    import array

    # We simulate the accumulation of dirt in the creases of geometric surfaces
    # by comparing the vertex normal to the average direction of all vertices
    # connected to that vertex. We can also simulate surfaces being buffed or
    # worn by testing protruding surfaces.
    #
    # So if the angle between the normal and geometric direction is:
    # < 90 - dirt has accumulated in the crease
    # > 90 - surface has been worn or buffed
    # ~ 90 - surface is flat and is generally unworn and clean
    #
    # This method is limited by the complexity or lack there of in the geometry.
    #
    # Original code and method by Keith "Wahooney" Boshoff.

    vert_tone = array.array("f", [0.0]) * len(me.vertices)

    # create lookup table for each vertex's connected vertices (via edges)
    con = [[] for i in range(len(me.vertices))]

    # add connected verts
    for e in me.edges:
        con[e.vertices[0]].append(e.vertices[1])
        con[e.vertices[1]].append(e.vertices[0])

    for i, v in enumerate(me.vertices):
        vec = Vector()
        no = v.normal
        co = v.co

        # get the direction of the vectors between the vertex and it's connected vertices
        for c in con[i]:
            vec += (me.vertices[c].co - co).normalized()

        # average the vector by dividing by the number of connected verts
        tot_con = len(con[i])

        if tot_con == 0:
            ang = pi / 2.0 # assume 90°, i. e. flat
        else:
            vec /= tot_con

            # angle is the acos() of the dot product between normal and connected verts.
            # > 90 degrees: convex
            # < 90 degrees: concave
            ang = acos(no.dot(vec))

        # enforce min/max
        ang = max(clamp_dirt, ang)

        if not dirt_only:
            ang = min(clamp_clean, ang)

        vert_tone[i] = ang

    # blur tones
    for i in range(blur_iterations):
        # backup the original tones
        orig_vert_tone = vert_tone[:]

        # use connected verts look up for blurring
        for j, c in enumerate(con):
            for v in c:
                vert_tone[j] += blur_strength * orig_vert_tone[v]

            vert_tone[j] /= len(c) * blur_strength + 1
        del orig_vert_tone

    if normalize:
        min_tone = min(vert_tone)
        max_tone = max(vert_tone)
    else:
        min_tone = clamp_dirt
        max_tone = clamp_clean

    tone_range = max_tone - min_tone

    if tone_range < 0.0001:
        # weak, don't cancel, see T43345
        tone_range = 0.0
    else:
        tone_range = 1.0 / tone_range

    active_col_layer = get_vcolor_layer_data(me)
    if not active_col_layer:
        return {'CANCELLED'}

    use_paint_mask = me.use_paint_mask
    for i, p in enumerate(me.polygons):
        if not use_paint_mask or p.select:
            for loop_index in p.loop_indices:
                loop = me.loops[loop_index]
                v = loop.vertex_index
                col = active_col_layer[loop_index].color
                tone = vert_tone[v]
                tone = (tone - min_tone) * tone_range

                if dirt_only:
                    tone = min(tone, 0.5) * 2.0

                col[0] = tone * col[0]
                col[1] = tone * col[1]
                col[2] = tone * col[2]
    me.update()
    return {'FINISHED'}


from bpy.types import Operator
from bpy.props import FloatProperty, IntProperty, BoolProperty
from math import pi


class VertexPaintDirt(Operator):
    '''Generate a dirt map gradient based on cavity'''
    bl_idname = "paint.vertex_color_dirt"
    bl_label = "Dirty Vertex Colors"
    bl_options = {'REGISTER', 'UNDO'}

    blur_strength: FloatProperty(
        name="Blur Strength",
        description="Blur strength per iteration",
        min=0.01, max=1.0,
        default=1.0,
    )
    blur_iterations: IntProperty(
        name="Blur Iterations",
        description="Number of times to blur the colors (higher blurs more)",
        min=0, max=40,
        default=1,
    )
    clean_angle: FloatProperty(
        name="Highlight Angle",
        description="Less than 90 limits the angle used in the tonal range",
        min=0.0, max=pi,
        default=pi,
        unit='ROTATION',
    )
    dirt_angle: FloatProperty(
        name="Dirt Angle",
        description="Less than 90 limits the angle used in the tonal range",
        min=0.0, max=pi,
        default=0.0,
        unit='ROTATION',
    )
    dirt_only: BoolProperty(
        name="Dirt Only",
        description="Don't calculate cleans for convex areas",
        default=False,
    )
    normalize: BoolProperty(
        name="Normalize",
        description="Normalize the colors, increasing the contrast",
        default=True,
    )

    @classmethod
    def poll(cls, context):
        obj = context.object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        obj = context.object
        mesh = obj.data

        ret = applyVertexDirt(
            mesh,
            self.blur_iterations,
            self.blur_strength,
            self.dirt_angle,
            self.clean_angle,
            self.dirt_only,
            self.normalize,
        )

        return ret


classes = (
    VertexPaintDirt,
)
