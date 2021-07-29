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

from sverchok.node_tree import SverchCustomTreeNode, VerticesSocket
from sverchok.data_structure import (updateNode, fullList, multi_socket, levelsOflist,
                            SvSetSocketAnyType, SvGetSocketAnyType)


class LineConnectNode(bpy.types.Node, SverchCustomTreeNode):
    ''' UV Connect node '''
    bl_idname = 'LineConnectNode'
    bl_label = 'UV Connect'
    bl_icon = 'OUTLINER_OB_EMPTY'

    replacement_nodes = [('LineConnectNodeMK2', None, None)]

    JoinLevel = IntProperty(name='JoinLevel', description='Choose connect level of data (see help)',
                            default=1, min=1, max=2,
                            update=updateNode)
    polygons = BoolProperty(name='polygons', description='Do polygons or not?',
                            default=True,
                            update=updateNode)
    direction = [('U_dir', 'U_dir', 'u direction'), ('V_dir', 'V_dir', 'v direction')]
    dir_check = EnumProperty(name='direction',
                             items=direction,
                             options={'ANIMATABLE'}, update=updateNode)
    # as cyclic too have to have U cyclic and V cyclic flags - two flags
    cicl_check = BoolProperty(name='cycle', description='cycle line',
                              default=False,
                              update=updateNode)
    slice_check = BoolProperty(name='slice', description='slice polygon',
                               default=True,
                               update=updateNode)

    base_name = 'vertices '
    multi_socket_type = 'VerticesSocket'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vertices', 'vertices')
        self.outputs.new('VerticesSocket', 'vertices', 'vertices')
        self.outputs.new('StringsSocket', 'data', 'data')

    def draw_buttons(self, context, layout):
        layout.prop(self, "dir_check", text="direction", expand=True)
        layout.prop(self, "cicl_check", text="cycle")
        row = layout.row(align=True)
        row.prop(self, "polygons", text="polygons")
        row.prop(self, "slice_check", text="slice")
        #layout.prop(self, "JoinLevel", text="level")

    def connect(self, vers, dirn, cicl, clev, polygons, slice):
        vers_ = []
        lens = []

        # for joinvers to one object
        def joinvers(ver):
            joinvers = []
            for ob in ver:
                fullList(list(ob), ml)
                joinvers.extend(ob)
            return joinvers
        # we will take common case of nestiness, it is not flatten as correctData is,
        # but pick in upper level bottom level of data. could be automated in future
        # to check for levelsOflist() and correct in recursion
        # lol = levelsOflist(vers)
        # print(lol)
        #if lol == 4: # was clev - manually defined, but it is wrong way
        for ob in vers:
            for o in ob:
                vers_.append(o)
                lens.append(len(o))
        #elif lol == 5:
        #    for ob in vers:
        #        for o in ob:
        #            for v in o:
        #                vers_.append(v)
        #                lens.append(len(v))
        #else:
        #    print('wrong level in UV connect')
        lenvers = len(vers_)
        #print(lenvers, lens)
        edges = []
        ml = max(lens)
        if dirn == 'U_dir':
            if polygons:

                # joinvers to implement
                length_ob = []
                newobject = []
                for k, ob in enumerate(vers_):
                    length_ob.append(len(ob))
                    newobject.extend(ob)
                # joinvers to implement

                curr = 0
                objecto = []
                indexes__ = []
                if slice:
                    indexes__ = [[j*ml+i for j in range(lenvers)] for i in range(ml)]
                    objecto = [a for a in zip(*indexes__)]
                else:
                    for i, ob in enumerate(length_ob):
                        indexes_ = []
                        for w in range(ob):
                            indexes_.append(curr)
                            curr += 1
                        if i == 0 and cicl:
                            cicle_firstrow = indexes_
                        if i > 0:
                            indexes = indexes_ + indexes__[::-1]
                            quaded = [(indexes[k], indexes[k+1], indexes[-(k+2)], indexes[-(k+1)])
                                      for k in range((len(indexes)-1)//2)]
                            objecto.extend(quaded)
                            if i == len(length_ob)-1 and cicl:
                                indexes = cicle_firstrow + indexes_[::-1]
                                quaded = [(indexes[k], indexes[k+1], indexes[-(k+2)], indexes[-(k+1)])
                                          for k in range((len(indexes)-1)//2)]
                                objecto.extend(quaded)
                        indexes__ = indexes_
                vers_ = [newobject]
                edges = [objecto]
            elif not polygons:
                for k, ob in enumerate(vers_):
                    objecto = []
                    for i, ve in enumerate(ob[:-1]):
                        objecto.append([i, i+1])
                    if cicl:
                        objecto.append([0, len(ob)-1])
                    edges.append(objecto)

        # not direction:
        elif dirn == 'V_dir':
            objecto = []
            # it making not direction order, but one-edged polygon instead of two rows
            # to remake - yet one flag that operates slicing. because the next is slicing,
            # not direction for polygons
            if polygons:
                if slice:
                    joinvers = joinvers(vers_)
                    for i, ve in enumerate(vers_[0][:]):
                        inds = [j*ml+i for j in range(lenvers)]
                        objecto.append(inds)
                else:
                    # flip matrix transpose:
                    vers_flip = [a for a in zip(*vers_)]
                    vers_ = vers_flip
                    # flip matrix transpose:

                    joinvers = joinvers(vers_)
                    for i, ob in enumerate(vers_[:-1]):
                        for k, ve in enumerate(ob[:-1]):
                            objecto.append([i*lenvers+k, (i+1)*lenvers+k, (i+1)*lenvers+k+1, i*lenvers+k+1])
                            if i == 0 and cicl:
                                objecto.append([k+1, (ml-1)*lenvers+k+1, (ml-1)*lenvers+k, k])
            elif not polygons:
                joinvers = joinvers(vers_)
                for i, ve in enumerate(vers_[0][:]):
                    inds = [j*ml+i for j in range(lenvers)]
                    for i, item in enumerate(inds):
                        if i == 0 and cicl:
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
        if self.outputs['vertices'].is_linked or self.outputs['data'].is_linked:
            slots = []
            for socket in self.inputs:
                if socket.is_linked:
                    slots.append(socket.sv_get())
            if len(slots) == 0:
                return
            lol = levelsOflist(slots)
            if lol == 4:
                result = self.connect(slots, self.dir_check, self.cicl_check, lol, self.polygons, self.slice_check)
            elif lol == 5:
                one = []
                two = []
                for slo in slots:
                    for s in slo:
                        result = self.connect([s], self.dir_check, self.cicl_check, lol, self.polygons, self.slice_check)
                        one.extend(result[0])
                        two.extend(result[1])
                result = (one,two)
            

            if self.outputs['vertices'].is_linked:
                SvSetSocketAnyType(self, 'vertices', result[0])
            if self.outputs['data'].is_linked:
                SvSetSocketAnyType(self, 'data', result[1])
                
                
def register():
    bpy.utils.register_class(LineConnectNode)


def unregister():
    bpy.utils.unregister_class(LineConnectNode)

if __name__ == '__main__':
    register()

