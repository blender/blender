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
from bpy.props import BoolProperty
from mathutils import Matrix

from sverchok.node_tree import (SverchCustomTreeNode, MatrixSocket,
                       StringsSocket, VerticesSocket)
from sverchok.data_structure import (dataCorrect, updateNode,
                            Matrix_generate, Vector_generate,
                            SvGetSocketAnyType)


sverchok_bakery_cache = {}


class BakeryNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Bakery node to bake geometry every time '''
    bl_idname = 'BakeryNode'
    bl_label = 'Bakery'
    bl_icon = 'OUTLINER_OB_EMPTY'

    activate = BoolProperty(name='Show', description='Activate node?',
                            default=True,
                            update=updateNode)

    def draw_buttons(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, "activate", text="Show")
        pass

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.inputs.new('StringsSocket', 'edg_pol', 'edg_pol')
        self.inputs.new('MatrixSocket', 'matrix', 'matrix')

    def process(self):
        # check if running during startup, cancel if True
        try:
            l = bpy.data.node_groups[self.id_data.name]
        except Exception as e:
            print("Bakery cannot run during startup", e)
            return

        if self.inputs['vertices'].links and self.inputs['edg_pol'].links and self.activate:
            if 'vertices' in self.inputs and self.inputs['vertices'].links and \
                    type(self.inputs['vertices'].links[0].from_socket) == VerticesSocket:
                propv = SvGetSocketAnyType(self, self.inputs['vertices'])
                vertices = dataCorrect(propv, nominal_dept=2)
            else:
                vertices = []

            if 'edg_pol' in self.inputs and self.inputs['edg_pol'].links and \
                    type(self.inputs['edg_pol'].links[0].from_socket) == StringsSocket:
                prope = SvGetSocketAnyType(self, self.inputs['edg_pol'])
                edges = dataCorrect(prope)
            else:
                edges = []

            if 'matrix' in self.inputs and self.inputs['matrix'].links and \
                    type(self.inputs['matrix'].links[0].from_socket) == MatrixSocket:
                propm = SvGetSocketAnyType(self, self.inputs['matrix'])
                matrices = dataCorrect(propm)
            else:
                matrices = []
                if vertices and edges:
                    for i in vertices:
                        matrices.append(Matrix())

            if vertices and edges:
                self.makeobjects(vertices, edges, matrices)
            self.use_custom_color = True
            self.color = (1, 0.3, 0)
        else:
            self.use_custom_color = True
            self.color = (0.1, 0.05, 0)

            for obj in bpy.context.scene.objects:
                nam = 'Sv_' + self.name
                if nam in obj.name:
                    bpy.context.scene.objects[obj.name].select = True
                    bpy.ops.object.delete(use_global=False)
            global sverchok_bakery_cache
            sverchok_bakery_cache[self.name] = []

    def makeobjects(self, vers, edg_pol, mats):

        fht = []
        if len(edg_pol[0][0]) == 2:
            pols = []
            for edgs in edg_pol:
                maxi = max(max(a) for a in edgs)
                fht.append(maxi)
        elif len(edg_pol[0][0]) > 2:
            edgs = []
            for pols in edg_pol:
                maxi = max(max(a) for a in pols)
                fht.append(maxi)
        vertices = Vector_generate(vers)
        matrixes = Matrix_generate(mats)
        objects = {}
        fhtagn = []
        for u, f in enumerate(fht):
            fhtagn.append(min(len(vertices[u]), fht[u]))
        #lenmesh = len(vertices) - 1

        # name space for particular bakery node
        # defined only by matrices count (without .001 etc)
        global sverchok_bakery_cache
        try:
            cache = sverchok_bakery_cache[self.name]
        except:
            cache = []
        names = ['Sv_' + self.name + str(i) for i, t in enumerate(mats)]
        #print('bakery'+str(names)+str(cache))

        bpy.ops.object.select_all(action='DESELECT')
        for i, obj in enumerate(bpy.context.scene.objects):
            nam = 'Sv_' + self.name
            if nam in obj.name:
                if obj.name not in names:
                    bpy.context.scene.objects[obj.name].select = True
                    bpy.ops.object.delete(use_global=False)

        for i, m in enumerate(matrixes):
            # solution to reduce number of vertices respect to edges/pols
            ########
            k = i
            lenver = len(vertices) - 1
            if i > lenver:
                v = vertices[-1]
                k = lenver
            else:
                v = vertices[k]
            if (len(v)-1) < fhtagn[k]:
                continue
            elif fhtagn[k] < (len(v)-1):
                nonneed = (len(v)-1) - fhtagn[k]
                for q in range(nonneed):
                    v.pop((fhtagn[k]+1))
            #########
            # end of solution to reduce vertices

            e = edg_pol[k] if edgs else []
            p = edg_pol[k] if pols else []

            # to change old, create new separately
            if names[i] not in cache:
                objects[str(i)] = self.makemesh(names[i], v, e, p, m)
            elif bpy.context.scene.objects.find(names[i]) >= 0:
                objects[str(i)] = self.makemesh_exist(names[i], v, e, p, m)
            else:
                objects[str(i)] = self.makemesh(names[i], v, e, p, m)

        for i, ite in enumerate(objects.values()):
            me = ite[1]
            ob = ite[0]
            calcedg = True
            if edgs:
                calcedg = False
            me.update(calc_edges=calcedg)
            if ob.name not in cache:
                bpy.context.scene.objects.link(ob)

        # save cache
        sverchok_bakery_cache[self.name] = names

    def makemesh(self, i, v, e, p, m):
        name = i
        me = bpy.data.meshes.new(name)
        me.from_pydata(v, e, p)
        ob = bpy.data.objects.new(name, me)
        ob.matrix_world = m
        ob.show_name = False
        ob.hide_select = False
        return [ob, me]

    def makemesh_exist(self, i, v, e, p, m):
        name = i
        me = bpy.data.meshes.new(name)
        me.from_pydata(v, e, p)
        ob = bpy.data.objects[name]
        ob.matrix_world = m
        return [ob, me]


def register():
    bpy.utils.register_class(BakeryNode)


def unregister():
    bpy.utils.unregister_class(BakeryNode)
