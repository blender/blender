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
from bpy.props import IntProperty, BoolProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, fullList, multi_socket, levelsOflist)


class LineConnectNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' uv Edges/Surfaces '''
    bl_idname = 'LineConnectNodeMK2'
    bl_label = 'UV Connection'
    bl_icon = 'OUTLINER_OB_EMPTY'

    base_name = 'vertices '
    multi_socket_type = 'VerticesSocket'

    direction = [('U_dir', 'U_dir', 'u direction'), ('V_dir', 'V_dir', 'v direction')]
    polsORedges = [('Pols', 'Pols', 'Pols'), ('Edges', 'Edges', 'Edges')]

    JoinLevel = IntProperty(
        name='JoinLevel', description='Choose connect level of data (see help)',
        default=1, min=1, max=2, update=updateNode)

    polygons = EnumProperty(
        name='polsORedges', items=polsORedges, options={'ANIMATABLE'}, update=updateNode)
    
    dir_check = EnumProperty(
        name='direction', items=direction, options={'ANIMATABLE'}, update=updateNode)

    # as cyclic too have to have U cyclic and V cyclic flags - two flags
    cicl_check_U = BoolProperty(name='cycleU', description='cycle U', default=False, update=updateNode)
    cicl_check_V = BoolProperty(name='cycleV', description='cycle V', default=False, update=updateNode)
    cup_U = BoolProperty(name='cup U', description='cup U', default=False, update=updateNode)
    cup_V = BoolProperty(name='cup V', description='cup V', default=False, update=updateNode)
    slice_check = BoolProperty(name='slice', description='slice polygon', default=True, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.outputs.new('VerticesSocket', 'vertices', 'vertices')
        self.outputs.new('StringsSocket', 'data', 'data')

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "dir_check", text="direction", expand=True)
        row = col.row(align=True)
        row.prop(self, "cicl_check_U", text="cycle U", toggle=True)
        row.prop(self, "cicl_check_V", text="cycle V", toggle=True)
        row = col.row(align=True)
        row.prop(self, "cup_U", text="cup U", toggle=True)
        row.prop(self, "cup_V", text="cup V", toggle=True)
        row = col.row(align=True)
        row.prop(self, "polygons", text="polygons", expand=True)
        row = col.row(align=True)
        row.prop(self, "slice_check", text="slice")
        #layout.prop(self, "JoinLevel", text="level")

    def connect(self, vers, dirn, ciclU, ciclV, clev, polygons, slice, cupU, cupV):
        ''' doing all job to connect '''

        def joinvers(ver):
            ''' for joinvers to one object '''
            joinvers = []
            for ob in ver:
                fullList(list(ob), lenvers)
                joinvers.extend(ob)
            return joinvers

        def cupends(lenobjs,lenvers,flip=False):
            if not flip:
                out = [[j*lenvers for j in reversed(range(lenobjs))]]
                out.extend( [[j*lenvers+lenvers-1 for j in range(lenobjs)]])
            else:
                out = [[j for j in reversed(range(lenobjs))]]
                out.extend( [[j+lenobjs*(lenvers-1) for j in range(lenobjs)]])
            return out

        vers_ = []
        lens = []
        edges = []

        for ob in vers:
            ''' prepate standard levels (correcting for default state)
                and calc everage length of each object'''
            for o in ob:
                vers_.append(o)
                lens.append(len(o))

        # lenobjs == number of sverchok objects
        lenobjs = len(vers_)
        # lenvers == amount of elements in one object
        lenvers = max(lens)

        if dirn == 'U_dir':
            if polygons == "Pols":

                # joinvers to implement
                length_ob = []
                newobject = []
                for ob in vers_:
                    length_ob.append(len(ob))
                    newobject.extend(ob)
                # joinvers to implement

                curr = 0
                objecto = []
                indexes__ = []
                if slice:
                    indexes__ = [[j*lenvers+i for j in range(lenobjs)] for i in range(lenvers)]
                    objecto = [a for a in zip(*indexes__)]
                else:
                    for i, ob in enumerate(length_ob):
                        indexes_ = []
                        for w in range(ob):
                            indexes_.append(curr)
                            curr += 1
                        if i > 0:
                            indexes = indexes_ + indexes__[::-1]
                            quaded = [(indexes[k], indexes[k+1], indexes[-(k+2)], indexes[-(k+1)])
                                      for k in range((len(indexes)-1)//2)]
                            objecto.extend(quaded)
                            if i == len(length_ob)-1 and ciclU:
                                indexes = cicle_firstrow + indexes_[::-1]
                                quaded = [(indexes[k], indexes[k+1], indexes[-(k+2)], indexes[-(k+1)])
                                          for k in range((len(indexes)-1)//2)]
                                objecto.extend(quaded)

                            if i == len(length_ob)-1 and ciclV:
                                quaded = [ [ (k-1)*lenvers, k*lenvers-1, (k+1)*lenvers-1, k*lenvers ]
                                          for k in range(lenobjs) if k > 0 ]
                                objecto.extend(quaded)
                        if i == 0 and ciclU:
                            cicle_firstrow = indexes_
                            if ciclV:
                                objecto.append([ 0, (lenobjs-1)*lenvers, lenobjs*lenvers-1, lenvers-1 ])
                        indexes__ = indexes_
                    if cupU:
                        objecto.extend(cupends(lenobjs,lenvers))
                    if cupV:
                        objecto.extend(cupends(lenvers,lenobjs,flip=True))
                vers_ = [newobject]
                edges = [objecto]
            elif polygons == "Edges":
                for k, ob in enumerate(vers_):
                    objecto = []
                    for i, ve in enumerate(ob[:-1]):
                        objecto.append([i, i+1])
                    if ciclU:
                        objecto.append([0, len(ob)-1])
                    edges.append(objecto)

        elif dirn == 'V_dir':
            objecto = []
            # it making V direction order, but one-edged polygon instead of two rows
            # to remake - yet one flag that operates slicing. because the next is slicing,
            # not direction for polygons
            if polygons == "Pols":
                if slice:
                    joinvers = joinvers(vers_)
                    for i, ve in enumerate(vers_[0][:]):
                        inds = [j*lenvers+i for j in range(lenobjs)]
                        objecto.append(inds)
                else:
                    # flip matrix transpose:
                    vers_flip = [a for a in zip(*vers_)]
                    vers_ = vers_flip
                    # flip matrix transpose:

                    joinvers = joinvers(vers_)
                    for i, ob in enumerate(vers_[:-1]):
                        for k, ve in enumerate(ob[:-1]):
                            objecto.append([i*lenobjs+k, (i+1)*lenobjs+k, (i+1)*lenobjs+k+1, i*lenobjs+k+1])
                            if i == 0 and ciclV:
                                objecto.append([k+1, (lenvers-1)*lenobjs+k+1, (lenvers-1)*lenobjs+k, k])
                        if i == 0 and ciclU and ciclV:
                            objecto.append([ 0, (lenvers-1)*lenobjs, lenvers*lenobjs-1, lenobjs-1 ])
                        if i == 0 and ciclU:
                            quaded = [ [ (k-1)*lenobjs, k*lenobjs-1, (k+1)*lenobjs-1, k*lenobjs ]
                                      for k in range(lenvers) if k > 0 ]
                            objecto.extend(quaded)

                    if cupV:
                        objecto.extend(cupends(lenvers,lenobjs))
                    if cupU:
                        objecto.extend(cupends(lenobjs,lenvers,flip=True))
            elif polygons == "Edges":
                joinvers = joinvers(vers_)
                for i, ve in enumerate(vers_[0][:]):
                    inds = [j*lenvers+i for j in range(lenobjs)]
                    for i, item in enumerate(inds):
                        if i == 0 and ciclV:
                            objecto.append([inds[0], inds[-1]])
                        elif i == 0:
                            continue
                        else:
                            objecto.append([item, inds[i-1]])
            edges.append(objecto)
            vers_ = [joinvers]
        return vers_, edges

    def update(self):
        # inputs
        multi_socket(self, min=1)

    def process(self):
        if self.inputs[0].is_linked:
            slots = [socket.sv_get() for socket in self.inputs if socket.is_linked]
            lol = levelsOflist(slots)
            if lol == 4:
                one, two = self.connect(slots, self.dir_check, self.cicl_check_U, self.cicl_check_V, lol, self.polygons, self.slice_check, self.cup_U, self.cup_V)
            elif lol == 5:
                one = []
                two = []
                for slo in slots:
                    for s in slo:
                        result = self.connect([s], self.dir_check, self.cicl_check_U, self.cicl_check_V, lol, self.polygons, self.slice_check, self.cup_U, self.cup_V)
                        one.extend(result[0])
                        two.extend(result[1])
            else:
                return
            if self.outputs['vertices'].is_linked:
                self.outputs['vertices'].sv_set(one)
            if self.outputs['data'].is_linked:
                self.outputs['data'].sv_set(two)


def register():
    bpy.utils.register_class(LineConnectNodeMK2)


def unregister():
    bpy.utils.unregister_class(LineConnectNodeMK2)

if __name__ == '__main__':
    register()
