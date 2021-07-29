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
from bpy.props import FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.utils.voronoi import Site, computeVoronoiDiagram, computeDelaunayTriangulation


class Voronoi2DNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Voronoi 2d line '''
    bl_idname = 'Voronoi2DNode'
    bl_label = 'Voronoi 2D'
    bl_icon = 'OUTLINER_OB_EMPTY'

    clip = FloatProperty(
        name='clip', description='Clipping Distance',
        default=1.0, min=0,
        options={'ANIMATABLE'}, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        # self.inputs.new('StringsSocket', "Clipping")
        self.outputs.new('VerticesSocket', "Vertices")
        self.outputs.new('StringsSocket', "Edges")
        # Polygon output does not work right now. Should be fixed
        # self.outputs.new('StringsSocket', "Polygons")

    def draw_buttons(self, context, layout):
        layout.prop(self, "clip", text="Clipping")

    def process(self):

        if not self.inputs['Vertices'].is_linked:
            return

        if not self.outputs['Vertices'].is_linked:
            return

        points_in = self.inputs['Vertices'].sv_get()

        pts_out = []
        # polys_out = []
        edges_out = []
        for obj in points_in:
            pt_list = []
            x_max = obj[0][0]
            x_min = obj[0][0]
            y_min = obj[0][1]
            y_max = obj[0][1]
            # creates points in format for voronoi library, throwing away z
            for pt in obj:
                x, y = pt[0], pt[1]
                x_max = max(x, x_max)
                x_min = min(x, x_min)
                y_max = max(y, y_max)
                y_min = min(x, x_min)
                pt_list.append(Site(pt[0], pt[1]))

            res = computeVoronoiDiagram(pt_list)

            edges = res[2]
            delta = self.clip
            x_max = x_max + delta
            y_max = y_max + delta

            x_min = x_min - delta
            y_min = y_min - delta

            # clipping box to bounding box.
            pts_tmp = []
            for pt in res[0]:
                x, y = pt[0], pt[1]
                if x < x_min:
                    x = x_min
                if x > x_max:
                    x = x_max

                if y < y_min:
                    y = y_min
                if y > y_max:
                    y = y_max
                pts_tmp.append((x, y, 0))

            pts_out.append(pts_tmp)
            edges_out.append([(edge[1], edge[2]) for edge in edges if -1 not in edge])

        # outputs
        self.outputs['Vertices'].sv_set(pts_out)
        self.outputs['Edges'].sv_set(edges_out)


# computeDelaunayTriangulation
class DelaunayTriangulation2DNode(bpy.types.Node, SverchCustomTreeNode):
    ''' DelaunayTriangulation '''
    bl_idname = 'DelaunayTriangulation2DNode'
    bl_label = 'Delaunay 2D'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        # self.outputs.new('StringsSocket', "Edges")
        self.outputs.new('StringsSocket', "Polygons")

    def process(self):

        if not self.inputs['Vertices'].is_linked:
            return
        if not self.outputs['Polygons'].is_linked:
            return

        tris_out = []
        points_in = []
        points_in = self.inputs['Vertices'].sv_get()

        for obj in points_in:
            pt_list = [Site(pt[0], pt[1]) for pt in obj]
            res = computeDelaunayTriangulation(pt_list)
            tris_out.append([tri for tri in res if -1 not in tri])


        self.outputs['Polygons'].sv_set(tris_out)


def register():
    bpy.utils.register_class(Voronoi2DNode)
    bpy.utils.register_class(DelaunayTriangulation2DNode)


def unregister():
    bpy.utils.unregister_class(Voronoi2DNode)
    bpy.utils.unregister_class(DelaunayTriangulation2DNode)
