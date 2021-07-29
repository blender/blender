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

# by Alexander Nedovizin

import bpy
from bpy.props import BoolProperty, IntProperty, StringProperty, FloatProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode
from sverchok.data_structure import handle_read, handle_write

from random import uniform
from copy import deepcopy
from cmath import exp


class SvNeuro_Elman:


    def init_w(self, number, ext, treshold):
        out = []
        for n in range(number):
            tmp = []
            for e in range(ext):
                tmp.append(uniform(-treshold, treshold))
            out.append(tmp)

        return out


    def sigmoida(self, x, a):
        if a==0:
            b=1
        else:
            b = 1/a
        return 1/(1+exp(-b*x).real+1e-8)

    def neuro(self, list_in, etalon, maxim, learning, prop):
        outA = self.layerA(list_in, prop)
        outB = self.layerB(outA, prop)
        outC = self.layerC(outB, prop)

        if learning:
            lin = len(etalon)
            if lin<prop['InC']:
                d = prop['InC']-lin
                etalon = etalon+[0]*d
            etalon_ = list(map(lambda x: x/maxim, etalon))
            self.learning(outA, outB, outC, etalon_, maxim, prop)
        outC_ = list(map(lambda x: x*maxim, outC))
        return outC_


    def layerA(self, list_in, prop):
        lin = len(list_in)
        if lin<prop['InA']:
            d = prop['InA']-lin
            list_in = list_in+[1]*d

        outA = deepcopy(list_in)
        return outA


    def layerB(self, outA, prop):
        outB = [0]*prop['InB']
        for ida,la in enumerate(prop['wA']):
            for idb, lb in enumerate(la):
                t1 = lb*outA[ida]
                outB[idb] += t1

        outB_ = [self.sigmoida(p,prop['InB']) for p in outB]
        return outB_

    def layerC(self, outB, prop):
        outC = [0]*prop['InC']
        for idb,lb in enumerate(prop['wB']):
            for idc, lc in enumerate(lb):
                t1 = lc*outB[idb]
                outC[idc] += t1
        return outC



# **********************
    def sigma(self, ej, f_vj):
        return ej*f_vj

    def f_vj_sigmoida(self, a, yj):
        if a==0:
            b = 1
        else:
            b = 1/a
        return b*yj*(1-yj)

    def func_ej_last(self, dj, yj):
        return dj-yj

    def func_ej_inner(self, Esigmak, wkj):
        return Esigmak*wkj

    def delta_wji(self, sigmaj, yi, prop):
        return prop['k_learning']*sigmaj*yi

    def func_w(self, w, dw, prop):
        return (1-prop['k_lambda'])*w + dw

    def learning(self, outA, outB, outC, etalon, maxim, prop):
        list_wA = deepcopy(prop['wA'])
        list_wB = deepcopy(prop['wB'])
        list_x = deepcopy(outA)
        for idx, x in enumerate(outA):
            step = 0

            xi = deepcopy(x)
            outB_ = deepcopy(outB)
            outC_ = deepcopy(outC)
            while step<prop['cycles']:
                step += 1
                eB = [0]*prop['InB']
                eA = [0]*prop['InA']
                for idc, c in enumerate(outC_):
                    c_ = self.sigmoida(c, prop['InC'])
                    eC = self.func_ej_last(etalon[idc], c)
                    f_vC = self.f_vj_sigmoida(prop['InC'], c_)
                    sigmaC = self.sigma(eC, f_vC)

                    for idb, b in enumerate(outB_):
                        dwji = self.delta_wji(sigmaC, b, prop)
                        list_wB[idb][idc] = self.func_w(list_wB[idb][idc], dwji, prop)
                        eB[idb] += sigmaC*dwji

                for idb, b in enumerate(outB_):
                    f_vB = self.f_vj_sigmoida(prop['InB'], b)
                    sigmaB = self.sigma(eB[idb], f_vB)

                    for ida, a in enumerate(outA):
                        dwji = self.delta_wji(sigmaB, a, prop)
                        list_wA[ida][idb] = self.func_w(list_wA[ida][idb], dwji, prop)
                        eA[ida] += sigmaB*dwji

                xi = xi - prop['epsilon'] * xi*(maxim-xi)
                absdx = abs(x-xi)
                if absdx<= prop['trashold'] or absdx>abs(maxim/2): break
                list_x[idx] = xi

                outB_ = self.layerB(list_x, prop)
                outC_ = self.layerC(outB, prop)

        prop['wA'] = list_wA
        prop['wB'] = list_wB



class SvNeuroElman1LNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Neuro Elman 1 Layer '''
    bl_idname = 'SvNeuroElman1LNode'
    bl_label = '*Neuro Elman 1 Layer'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Elman = SvNeuro_Elman()
    k_learning = FloatProperty(name='k_learning',
                            default=0.1,
                            update=updateNode)
    gisterezis = FloatProperty(name='gisterezis',
                            default=0.1,
                            min = 0.0,
                            update=updateNode)
    maximum = FloatProperty(name='maximum',
                            default=3.0,
                            update=updateNode)
    menushka = BoolProperty(name='menushka',
                            default=False)
    epsilon = FloatProperty(name='epsilon',
                            default=1.0,
                            update=updateNode)
    treshold = FloatProperty(name='treshold',
                            default=0.01,
                            update=updateNode)
    k_lambda = FloatProperty(name='k_lambda',
                            default=0.001,
                            max = 0.1,
                            update=updateNode)
    cycles = IntProperty(name='cycles', default=3, min = 1, update=updateNode)
    lA = IntProperty(name='lA', default=1, min = 0, update=updateNode)
    lB = IntProperty(name='lB', default=5, min = 0, update=updateNode)
    lC = IntProperty(name='lC', default=1, min = 0, update=updateNode)



    def sv_init(self, context):
        self.inputs.new('StringsSocket', "data", "data")
        self.inputs.new('StringsSocket', "etalon", "etalon")
        self.outputs.new('StringsSocket', "result", "result")


    def draw_buttons(self, context, layout):
        handle_name = self.name + self.id_data.name
        layout.prop(self, "k_learning", text="koeff learning")
        layout.prop(self, "gisterezis", text="gisterezis")
        layout.prop(self, "maximum", text="maximum")
        layout.prop(self, "cycles", text="cycles")
        op_start = layout.operator('node.sverchok_neuro', text='Restart')
        op_start.typ=1
        op_start.handle_name = handle_name
        layout.prop(self, "menushka", text="extend sets:")
        if self.menushka:
            col_top = layout.column(align=True)
            row = col_top.row(align=True)
            row.prop(self, "lA", text="A layer")
            row = col_top.row(align=True)
            row.prop(self, "lB", text="B layer")
            row = col_top.row(align=True)
            row.prop(self, "lC", text="C layer")
            col = layout.column(align=True)
            col.prop(self, "epsilon", text="epsilon")
            col = layout.column(align=True)
            col.prop(self, "k_lambda", text="lambda")
            col = layout.column(align=True)
            col.prop(self, "treshold", text="treshold")


    def process(self):
        handle_name = self.name + self.id_data.name
        handle = handle_read(handle_name)
        props = handle[1]
        if not handle[0]:
            props = {'InA':2,
                     'InB':5,
                     'InC':1,
                     'wA':[],
                     'wB':[],
                     'gister':0.01,
                     'k_learning':0.1,
                     'epsilon':1.3,
                     'cycles':3,
                     'trashold':0.01,
                     'k_lambda':0.0001}

            props['wA'] = self.Elman.init_w(props['InA'], props['InB'], props['trashold'])
            props['wB'] = self.Elman.init_w(props['InB'], props['InC'], props['trashold'])


        self.Elman.gister = abs(self.gisterezis)
        self.Elman.k_learning = self.k_learning

        result = []
        if self.outputs['result'].is_linked and self.inputs['data'].is_linked:

            if self.inputs['etalon'].is_linked:
                etalon = self.inputs['etalon'].sv_get()[0]
                flag = True
            else:
                flag = False
                etalon = [[0]]

            if (props['InA']!=self.lA+1) or props['InB']!=self.lB or props['InC']!=self.lC:
                props['InA'] = self.lA+1
                props['InB'] = self.lB
                props['InC'] = self.lC
                props['wA'] = self.Elman.init_w(props['InA'], props['InB'])
                props['wB'] = self.Elman.init_w(props['InB'], props['InC'])

            props['gister'] = self.gisterezis
            props['k_learning'] = self.k_learning
            props['epsilon'] = self.epsilon
            props['k_lambda'] = self.k_lambda
            props['cycles'] = self.cycles
            props['trashold'] = self.treshold

            data_ = self.inputs['data'].sv_get()[0]
            if type(etalon[0]) not in [list, tuple]: etalon = [etalon]
            if type(data_[0]) not in [list, tuple]: data_ = [data_]
            for idx, data in enumerate(data_):
                let = len(etalon)-1
                eta = etalon[min(idx,let)]
                data2 = [1.0]+data
                if type(eta) not in [list, tuple]: eta = [eta]
                result.append([self.Elman.neuro(data2, eta, self.maximum, flag, props)])

        else:
            result = [[[]]]

        handle_write(handle_name, props)
        self.outputs['result'].sv_set(result)




#*********************************

class SvNeuroOps(bpy.types.Operator):
    """ Neuro operators """
    bl_idname = "node.sverchok_neuro"
    bl_label = "Sverchok Neuro operators"
    bl_options = {'REGISTER', 'UNDO'}

    typ = IntProperty(name = 'typ', default=0)
    handle_name = StringProperty(name='handle')

    def execute(self, context):
        if self.typ == 1:
            handle = handle_read(self.handle_name)
            prop = handle[1]
            Elman = SvNeuro_Elman()
            if handle[0]:
                prop['wA']=Elman.init_w(prop['InA'], prop['InB'], prop['trashold'])
                prop['wB']=Elman.init_w(prop['InB'], prop['InC'], prop['trashold'])
                handle_write(self.handle_name, prop)

        return {'FINISHED'}






def register():
    bpy.utils.register_class(SvNeuroOps)
    bpy.utils.register_class(SvNeuroElman1LNode)


def unregister():
    bpy.utils.unregister_class(SvNeuroElman1LNode)
    bpy.utils.unregister_class(SvNeuroOps)
