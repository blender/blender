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

# Gorodetskiy Nikita aka nikitron made this node in 2014 y.

import bpy

from mathutils import Vector, Euler
from mathutils.geometry import distance_point_to_plane as D2P
from mathutils.geometry import intersect_line_line as IL2L
from mathutils.geometry import intersect_line_plane as IL2P
from mathutils.geometry import normal as NM
from mathutils import kdtree as KDT
from sverchok.data_structure import (Vector_generate, Vector_degenerate, fullList,
                                     dataCorrect,
                                     updateNode)
from math import sin, atan, cos, degrees, radians
from bpy.props import FloatProperty, BoolProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.nodes.modifier_change.polygons_to_edges import pols_edges


class SvWafelNode(bpy.types.Node, SverchCustomTreeNode):
    '''Making vertical wafel - much raw node'''
    bl_idname = 'SvWafelNode'
    bl_label = 'Wafel'
    bl_icon = 'OUTLINER_OB_EMPTY'


    def ext_draw_checking(self, context):
        # check for sockets to add
        if self.bindCircle and not ('radCircle' in self.inputs):
            self.inputs.new('StringsSocket', 'radCircle').prop_name = 'circle_rad'
        elif not self.bindCircle and ('radCircle' in self.inputs):
            self.inputs.remove(self.inputs['radCircle'])
        if self.do_tube_sect and not any([('vecTube' in self.inputs), ('radTube' in self.inputs)]):
            self.inputs.new('VerticesSocket', 'vecTube', 'vecTube')
            self.inputs.new('StringsSocket', 'radTube').prop_name = 'tube_radius'
        elif not self.do_tube_sect and any([('vecTube' in self.inputs), ('radTube' in self.inputs)]):
            self.inputs.remove(self.inputs['vecTube'])
            self.inputs.remove(self.inputs['radTube'])
        if self.do_contra and not ('vecContr' in self.inputs):
            self.inputs.new('VerticesSocket', 'vecContr')
        elif not self.do_contra and ('vecContr' in self.inputs):
            self.inputs.remove(self.inputs['vecContr'])

    # now we just need to sort out the properties that creates socket
    # from the ones that should process.

    thick = FloatProperty(name='thick', description='thickness of material',
                           default=0.01)

    circle_rad = FloatProperty(name='radius', description='radius of circle',
                           default=0.01)

    tube_radius = FloatProperty(name='tube_radius', description='radius of tube',
                           default=0.05)

    threshold = FloatProperty(name='threshold', description='threshold for intersect edge',
                           default=16)

    circl_place = EnumProperty(name='place Bind', items=[('Up','Up','Up'),('Midl','Midl','Midl'),('Down','Down','Down')],
                           description='circle placement', default='Up', update=updateNode)

    out_up_down = EnumProperty(name='out_up_down', items=[('Up','Up','Up'),('Down','Down','Down')],
                           description='out up or down sect', default='Up', update=updateNode)

    rounded = BoolProperty(name='rounded', description='making rounded edges',
                           default = False, update=updateNode)
    rounded_outside = BoolProperty(name='roundout', description='making rounded edges outside',
                           default = False, update=updateNode)

    bindCircle = BoolProperty(name='Bind2', description='circle for leyer to bind with contras',
                           default=False, update=ext_draw_checking)

    do_contra = BoolProperty(name='Contra', description='making contraversion for some coplanar segment',
                            default = False, update=ext_draw_checking)

    do_tube_sect = BoolProperty(name='Tube', description='making tube section',
                            default = False, update=ext_draw_checking)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'vecLine', 'vecLine')
        self.inputs.new('VerticesSocket', 'vecPlane', 'vecPlane')
        self.inputs.new('StringsSocket', 'edgPlane', 'edgPlane')
        self.inputs.new('StringsSocket', 'thick').prop_name = 'thick'

        self.outputs.new('VerticesSocket', 'vert', 'vert')
        self.outputs.new('StringsSocket', 'edge', 'edge')
        #self.outputs.new('VerticesSocket', 'vertLo', 'vertLo')
        #self.outputs.new('StringsSocket', 'edgeLo', 'edgeLo')
        self.outputs.new('VerticesSocket', 'centers', 'centers')

    def draw_main_ui_elements(self, context, layout):
        col = layout.column(align=True)
        col.prop(self, 'threshold')
        row = col.row(align=True)
        row.prop(self, 'out_up_down', expand=True)

    def draw_buttons(self, context, layout):
        self.draw_main_ui_elements(context, layout)

    def draw_buttons_ext(self, context, layout):
        row = layout.row(align=True)
        row.prop(self, 'rounded_outside')
        row.prop(self, 'rounded')
        row = layout.row(align=True)
        row.label(text=' ')
        row.prop(self, 'bindCircle')
        row = layout.row(align=True)
        row.prop(self, 'do_contra')
        row.prop(self, 'do_tube_sect')
        if self.bindCircle:
            row = layout.row(align=True)
            row.prop(self, 'circl_place', expand=True)

    def rotation_on_axis(self, p,v,a):
        '''
        rotate one point 'p' over axis normalized 'v' on degrees 'a'
        '''
        Xp,Yp,Zp = p[:]
        Xv,Yv,Zv = v[:]
        Temp = 1 - cos(a)
        Nx = Xp * (Xv * Temp * Xv + cos(a)) + \
             Yp * (Yv * Temp * Xv - sin(a) * Zv) + \
             Zp * (Zv * Temp * Xv + sin(a) * Yv)

        Ny = Xp * (Xv * Temp * Yv + sin(a) * Zv) + \
             Yp * (Yv * Temp * Yv + cos(a)) + \
             Zp * (Zv * Temp * Yv - sin(a) * Xv)

        Nz = Xp * (Xv * Temp * Zv - sin(a) * Yv) + \
             Yp * (Yv * Temp * Zv + sin(a) * Xv) + \
             Zp * (Zv * Temp * Zv + cos(a))
        return Vector(( Nx,Ny,Nz ))

    def calc_indexes(self, edgp, near):
        '''
        find binded edges and vertices, prepare to delete edges
        '''
        q = []
        deledges = []
        for i in edgp:
            if near in i:
                for t in i:
                    if t != near:
                        q.append(t)
                deledges.append(list(i))
        q.append(deledges)
        return q

    def interpolation(self, vecp, vec, en0, en1, diry, dirx):
        '''
        shifting on height
        '''
        out = []
        k = False
        for en in [en0, en1]:
            if k:
                out.append(IL2L(vec,vecp[en],vec-dirx,vec-dirx-diry)[0])
            else:
                out.append(IL2L(vec,vecp[en],vec+dirx,vec+dirx-diry)[0])
            k = True
        return out

    def calc_leftright(self, vecp, vec, dirx, en0, en1, thick, diry):
        '''
        calc left right from defined point and direction to join vertices
        oriented on given indexes
        left right - points
        l r - indexes of this nearest points
        lz rz - height difference to compensate
        '''
        a,b = vecp[en0]-vec+dirx, vecp[en0]-vec-dirx
        if a.length > b.length:
            l =  en0
            r =  en1
            rz, lz = self.interpolation(vecp, vec, l, r, diry, dirx)
        else:
            l =  en1
            r =  en0
            rz, lz = self.interpolation(vecp, vec, l, r, diry, dirx)
        return l, r, lz, rz

    def get_coplanar(self,vec, loc_cont, norm_cont,vec_cont):
        '''
        if coplanar - than make flip cutting up-bottom
        '''
        for locon, nocon, vecon in zip(loc_cont,norm_cont,vec_cont):
            x = [i[0] for i in vecon]
            y = [i[1] for i in vecon]
            con_domein = vec[0]<max(x) and vec[0]>min(x) and vec[1]<max(y) and vec[1]>min(y)
            if con_domein:
                a = abs(D2P(vec,locon[0],nocon[0]))
                if a < 0.001:
                    return True
        return False


    def process(self):

        if 'vecLine' in self.inputs and \
                'vecPlane' in self.inputs and \
                'edgPlane' in self.inputs:
            print(self.name, 'is starting')
            if self.inputs['vecLine'].links and \
                    self.inputs['vecPlane'].links and \
                    self.inputs['edgPlane'].links:
                if self.bindCircle:
                    circle = [ (Vector((sin(radians(i)),cos(radians(i)),0))*self.circle_rad)/4 \
                              for i in range(0,360,30) ]
                vec = self.inputs['vecLine'].sv_get()
                vecplan = self.inputs['vecPlane'].sv_get()
                edgplan = self.inputs['edgPlane'].sv_get()
                if len(edgplan[0][0]) > 2:
                    edgplan = pols_edges(edgplan)
                thick = self.inputs['thick'].sv_get()[0][0]
                threshold_coplanar = 0.005
                sinuso60 = 0.8660254037844386
                sinuso60_minus = 0.133974596
                sinuso30 = 0.5
                sinuso45 = 0.7071067811865475
                thick_2 = thick/2
                thick_3 = thick/3
                thick_6 = thick/6
                threshold = self.threshold
                if 'vecContr' in self.inputs and self.inputs['vecContr'].links:
                    vecont = self.inputs['vecContr'].sv_get()
                    #edgcont = self.inputs['edgContr'].sv_get()
                    vec_cont = Vector_generate(vecont)
                    loc_cont = [ [ i[0] ] for i in vec_cont ]
                    norm_cont = [ [ NM(i[0],i[len(i)//2], i[-1]) ] for i in vec_cont ] # довести до ума
                else:
                    vec_cont = []
                if 'vecTube' in self.inputs and self.inputs['vecTube'].links:
                    vectube = self.inputs['vecTube'].sv_get()
                    vec_tube = Vector_generate(vectube)
                    tube_radius = self.inputs['radTube'].sv_get()[0][0]
                    circle_tube = [ (Vector((sin(radians(i)),cos(radians(i)),0))*tube_radius) \
                              for i in range(0,360,15) ]
                else:
                    vec_tube = []
                outeup = []
                outelo = []
                vupper = []
                vlower = []
                centers = []
                vec_ = Vector_generate(vec)
                vecplan_ = Vector_generate(vecplan)
                for centersver, vecp, edgp in zip(vecplan,vecplan_,edgplan):
                    tubes_flag_bed_solution_i_know = False
                    newinds1 = [list(e) for e in edgp]
                    newinds2 = newinds1.copy()
                    vupperob = vecp.copy()
                    vlowerob = vecp.copy()
                    deledges1 = []
                    deledges2 = []
                    # to define bounds
                    x = [i[0] for i in vecp]
                    y = [i[1] for i in vecp]
                    z = [i[2] for i in vecp]
                    m1x,m2x,m1y,m2y,m1z,m2z = max(x), min(x), max(y), min(y), max(z), min(z)
                    l = Vector((sum(x)/len(x),sum(y)/len(y),sum(z)/len(z)))
                    n_select = [vecp[0],vecp[len(vecp)//2], vecp[-1]] # довести до ума
                    n_select.sort(key=lambda x: sum(x[:]), reverse=False)
                    n_ = NM(n_select[0],n_select[1],n_select[2])
                    n_.normalize()
                    # а виновта ли нормаль?
                    if n_[0] < 0:
                        n = n_ * -1
                    else:
                        n = n_
                    cen = [sum(i) for i in zip(*centersver)]
                    centers.append(Vector(cen)/len(centersver))
                    k = 0
                    lenvep = len(vecp)
                    # KDtree collections closest to join edges to sockets
                    tree = KDT.KDTree(lenvep)
                    for i,v in enumerate(vecp):
                        tree.insert(v,i)
                    tree.balance()
                    # vertical edges iterations
                    # every edge is object - two points, one edge
                    for v in vec_:
                        if not v: continue
                        # sort vertices by Z value
                        # find two vertices - one lower, two upper
                        vlist = [v[0],v[1]]
                        vlist.sort(key=lambda x: x[2], reverse=False)
                        # flip if coplanar to enemy plane
                        # flip plane coplanar
                        if vec_cont:
                            fliped = self.get_coplanar(v[0], loc_cont,norm_cont, vec_cont)
                        else:
                            fliped = False
                        shortedge = (vlist[1]-vlist[0]).length
                        if fliped:
                            two, one = vlist
                        else:
                            one, two = vlist
                        # coplanar to owner
                        cop = abs(D2P(one,l,n))
                        # defining bounds
                        inside = one[0]<m1x and one[0]>m2x and one[1]<m1y and one[1]>m2y \
                                 and one[2]<=m1z and one[2]>=m2z
                        # if in bounds and coplanar do:
                        #print(self.name,l, cop, inside)
                        if cop < threshold_coplanar and inside and shortedge > thick*threshold:
                            '''
                            huge calculations. if we can reduce...
                            '''
                            # find shift for thickness in sockets
                            diry = two - one
                            diry.normalize()
                            # solution for vertical wafel - cool but not in diagonal case
                            # angle = radians(degrees(atan(n.y/n.x))+90)
                            dirx_ = self.rotation_on_axis(diry, n, radians(90))
                            dirx = dirx_*thick_2
                            # вектор, индекс, расстояние
                            # запоминаем порядок находим какие удалить рёбра
                            # делаем выборку левая-правая точка
                            nearv_1, near_1 = tree.find(one)[:2]
                            nearv_2, near_2 = tree.find(two)[:2]
                            # indexes of two nearest points
                            # удалить рёбра что мешают спать заодно
                            try:
                                en_0, en_1, de1 = self.calc_indexes(edgp, near_1)
                                deledges1.extend(de1)
                                en_2, en_3, de2 = self.calc_indexes(edgp, near_2)
                                deledges2.extend(de2)
                            except:
                                print('Waffel Wrong crossection node')
                                break
                            # print(vecp, one, dirx, en_0, en_1)
                            # left-right indexes and vectors
                            # с учётом интерполяций по высоте
                            l1, r1, lz1, rz1 = \
                                    self.calc_leftright(vecp, one, dirx, en_0, en_1, thick_2, diry)
                            l2, r2, lz2, rz2 = \
                                    self.calc_leftright(vecp, two, dirx, en_2, en_3, thick_2, diry)
                            # print(left2, right2, l2, r2, lz2, rz2)
                            # средняя точка и её смещение по толщине материала
                            three = (one-two)/2 + two

                            # rounded section
                            if self.rounded:
                                '''рёбра'''
                                # пазы формируем независимо от верх низ

                                # outeob1 = [[lenvep+k+8,lenvep+k],[lenvep+k+1,lenvep+k+2],
                                #           [lenvep+k+2,lenvep+k+3],[lenvep+k+3,lenvep+k+4],
                                #           [lenvep+k+4,lenvep+k+5],[lenvep+k+5,lenvep+k+6],
                                #           [lenvep+k+6,lenvep+k+7],[lenvep+k+7,lenvep+k+8],
                                #           [lenvep+k+9,lenvep+k+1]]

                                # outeob2 = [[lenvep+k,lenvep+k+1],[lenvep+k+1,lenvep+k+2],
                                #           [lenvep+k+2,lenvep+k+3],[lenvep+k+3,lenvep+k+4],
                                #           [lenvep+k+4,lenvep+k+5],[lenvep+k+5,lenvep+k+6],
                                #           [lenvep+k+6,lenvep+k+7],[lenvep+k+7,lenvep+k+8],
                                #           [lenvep+k+8,lenvep+k+9]]


                                outeob1 = [[lenvep+k+8,lenvep+k]]
                                outeob1.extend([[lenvep+k+i,lenvep+k+i+1] for i in range(1,10,1)])
                                outeob1.append([lenvep+k+9,lenvep+k+1])

                                outeob2 = [[lenvep+k+i,lenvep+k+i+1] for i in range(0,10,1)]


                                # наполнение списков lenvep = length(vecp)
                                newinds1.extend([[l1, lenvep+k], [lenvep+k+9, r1]])
                                newinds2.extend([[l2, lenvep+k+9], [lenvep+k, r2]])
                                '''Вектора'''
                                round1 = diry*thick_3
                                round2 = diry*thick_3*sinuso30
                                round2_= dirx/3 + dirx*(2*sinuso60/3)
                                round3 = diry*thick_3*sinuso60_minus
                                round3_= dirx/3 + dirx*(2*sinuso30/3)
                                round4 = dirx/3
                                vupperob.extend([lz2,
                                                 three+round1-dirx, three+round2-round2_,
                                                 three+round3-round3_, three-round4,
                                                 three+round4, three+round3+round3_,
                                                 three+round2+round2_, three+round1+dirx,
                                                 rz2])
                                vlowerob.extend([rz1,
                                                 three-round1-dirx, three-round2-round2_,
                                                 three-round3-round3_, three-round4,
                                                 three+round4, three-round3+round3_,
                                                 three-round2+round2_, three-round1+dirx,
                                                 lz1])
                                k += 10
                            elif  self.rounded_outside:
                                '''рёбра вовнешнее'''
                                # пазы формируем независимо от верх низ

                                outeob1 = [[lenvep+k+28,lenvep+k]]
                                outeob1.extend([[lenvep+k+i,lenvep+k+i+1] for i in range(1,30,1)])
                                outeob1.append([lenvep+k+29,lenvep+k+1])

                                outeob2 = [[lenvep+k+i,lenvep+k+i+1] for i in range(0,30,1)]

                                # наполнение списков lenvep = length(vecp)
                                newinds1.extend([[l1, lenvep+k], [lenvep+k+29, r1]])
                                newinds2.extend([[l2, lenvep+k+29], [lenvep+k, r2]])
                                '''Вектора'''
                                round1 = diry*thick_3
                                round2 = diry*thick_3*sinuso30
                                round2_= dirx/3 + dirx*(2*sinuso60/3)
                                round3 = diry*thick_3*sinuso60_minus
                                round3_= dirx/3 + dirx*(2*sinuso30/3)
                                round4 = dirx/3
                                vupperob.extend([lz2,
                                                 three+round1-dirx, three+round2-round2_,
                                                 three+round3-round3_, three-round4,
                                                 three+round4, three+round3+round3_,
                                                 three+round2+round2_, three+round1+dirx,
                                                 rz2])
                                vlowerob.extend([rz1,
                                                 three-round1-dirx, three-round2-round2_,
                                                 three-round3-round3_, three-round4,
                                                 three+round4, three-round3+round3_,
                                                 three-round2+round2_, three-round1+dirx,
                                                 lz1])
                                k += 10

                            # streight section
                            else:
                                '''рёбра'''
                                # пазы формируем независимо от верх низ
                                outeob1 = [[lenvep+k,lenvep+k+1],[lenvep+k+1,lenvep+k+2],[lenvep+k+2,lenvep+k+3]]
                                outeob2 = [[lenvep+k,lenvep+k+1],[lenvep+k+1,lenvep+k+2],[lenvep+k+2,lenvep+k+3]]
                                # наполнение списков lenvep = length(vecp)
                                newinds1.extend([[l1, lenvep+k], [lenvep+k+3, r1]])
                                newinds2.extend([[l2, lenvep+k+3], [lenvep+k, r2]])
                                '''Вектора'''
                                vupperob.extend([lz2, three-dirx,
                                                 three+dirx, rz2])
                                vlowerob.extend([rz1, three+dirx,
                                                 three-dirx, lz1])
                                k += 4
                            newinds1.extend(outeob1)
                            newinds2.extend(outeob2)

                            # circles to bing panels section
                            if self.bindCircle:
                                CP = self.circl_place
                                if CP == 'Midl':
                                    crcl_cntr = IL2P(one, two, Vector((0,0,0)), Vector((0,0,-1)))
                                elif CP == 'Up' and not fliped:
                                    crcl_cntr = two - diry*self.circle_rad*2
                                elif CP == 'Down' and not fliped:
                                    crcl_cntr = one + diry*self.circle_rad*2
                                elif CP == 'Up' and fliped:
                                    crcl_cntr = one + diry*self.circle_rad*2
                                elif CP == 'Down' and fliped:
                                    crcl_cntr = two - diry*self.circle_rad*2
                                # forgot howto 'else' in line iteration?
                                outeob1 = [ [lenvep+k+i,lenvep+k+i+1] for i in range(0,11) ]
                                outeob1.append([lenvep+k,lenvep+k+11])
                                outeob2 = [ [lenvep+k+i,lenvep+k+i+1] for i in range(12,23) ]
                                outeob2.append([lenvep+k+12,lenvep+k+23])
                                newinds1.extend(outeob1+outeob2)
                                newinds2.extend(outeob1+outeob2)
                                mat_rot_cir = n.rotation_difference(Vector((0,0,1))).to_matrix().to_4x4()
                                circle_to_add_1 = [vecir*mat_rot_cir+crcl_cntr+ \
                                        dirx_*self.circle_rad for vecir in circle ]
                                circle_to_add_2 = [vecir*mat_rot_cir+crcl_cntr- \
                                        dirx_*self.circle_rad for vecir in circle ]
                                vupperob.extend(circle_to_add_1+circle_to_add_2)
                                vlowerob.extend(circle_to_add_1+circle_to_add_2)
                                k += 24

                            # TUBE section
                            if vec_tube and not tubes_flag_bed_solution_i_know:
                                for v in vec_tube:
                                    tubeverlength = len(v)
                                    if tubeverlength == 2:
                                        crcl_cntr = IL2P(v[0], v[1], l, n)
                                        if crcl_cntr:
                                            inside = crcl_cntr[0]<m1x and crcl_cntr[0]>m2x and crcl_cntr[1]<m1y \
                                                 and crcl_cntr[1]>m2y and crcl_cntr[2]<=m1z and crcl_cntr[2]>=m2z
                                            if inside:
                                                outeob = [ [lenvep+k+i,lenvep+k+i+1] for i in range(0,23) ]
                                                outeob.append([lenvep+k,lenvep+k+23])
                                                newinds1.extend(outeob)
                                                newinds2.extend(outeob)
                                                mat_rot_cir = n.rotation_difference(Vector((0,0,1))).to_matrix().to_4x4()
                                                circle_to_add = [ vecir*mat_rot_cir+crcl_cntr for vecir in circle_tube ]
                                                vupperob.extend(circle_to_add)
                                                vlowerob.extend(circle_to_add)
                                                k += 24
                                    else:
                                        tubeshift = tubeverlength//2
                                        crcl_cntr = IL2P(v[0], v[tubeshift], l, n)
                                        if crcl_cntr:
                                            inside = crcl_cntr[0]<m1x and crcl_cntr[0]>m2x and crcl_cntr[1]<m1y \
                                                 and crcl_cntr[1]>m2y and crcl_cntr[2]<=m1z and crcl_cntr[2]>=m2z
                                            if inside:
                                                outeob = [ [lenvep+k+i,lenvep+k+i+1] for i in range(tubeshift-1) ]
                                                outeob.append([lenvep+k,lenvep+k+tubeshift-1])
                                                newinds1.extend(outeob)
                                                newinds2.extend(outeob)
                                                for tubevert in range(tubeshift):
                                                    tubevert_out = IL2P(v[tubevert], v[tubevert+tubeshift], l, n)
                                                    vupperob.append(tubevert_out)
                                                    vlowerob.append(tubevert_out)
                                                k += tubeshift

                                tubes_flag_bed_solution_i_know = True
                        elif cop < threshold_coplanar and inside and shortedge <= thick*threshold:
                            vupperob.extend([one,two])
                            vlowerob.extend([one,two])
                            newinds1.append([lenvep+k,lenvep+k+1])
                            newinds2.append([lenvep+k,lenvep+k+1])
                            k += 2
                    del tree
                    for e in deledges1:
                        if e in newinds1:
                            newinds1.remove(e)
                    for e in deledges2:
                        if e in newinds2:
                            newinds2.remove(e)
                    if vupperob or vlowerob:
                        outeup.append(newinds2)
                        outelo.append(newinds1)
                        vupper.append(vupperob)
                        vlower.append(vlowerob)
                vupper = Vector_degenerate(vupper)
                vlower = Vector_degenerate(vlower)
                centers = Vector_degenerate([centers])

                if 'vert' in self.outputs:
                    if self.out_up_down == 'Up':
                        out = dataCorrect(vupper)
                    else:
                        out = dataCorrect(vlower)
                    self.outputs['vert'].sv_set(out)
                if 'edge' in self.outputs and self.outputs['edge'].links:
                    if self.out_up_down == 'Up':
                        self.outputs['edge'].sv_set(outeup)
                    else:
                        self.outputs['edge'].sv_set(outelo)
                if 'centers' in self.outputs and self.outputs['centers'].links:
                    self.outputs['centers'].sv_set(centers)
                print(self.name, 'is finishing')


def register():
    bpy.utils.register_class(SvWafelNode)


def unregister():
    bpy.utils.unregister_class(SvWafelNode)


if __name__ == '__main__':
    register()
