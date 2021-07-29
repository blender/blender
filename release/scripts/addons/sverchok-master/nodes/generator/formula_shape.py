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
import ast
from bpy.props import IntProperty, FloatProperty, EnumProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode

import bpy, math, cmath, mathutils
from math import acos, acosh, asin, asinh, atan, atan2, \
                            atanh,ceil,copysign,cos,cosh,degrees,e, \
                            erf,erfc,exp,expm1,fabs,factorial,floor, \
                            fmod,frexp,fsum,gamma,hypot,isfinite,isinf, \
                            isnan,ldexp,lgamma,log,log10,log1p,log2,modf, \
                            pi,pow,radians,sin,sinh,sqrt,tan,tanh,trunc

sv_no_ve = [[(3, -1, 0),  (1, -1, 0),  (1, 2, 0),  (4, 2, 0),  (-1, -1, 0),
  (0, -1, 0),  (1, 0, 0),  (0, 2, 0),  (-1, 2, 0),  (-1, 0, 0),  (0, 0, 0),
  (3, 2, 0),  (2, 2, 0),  (3, 0, 0),  (3, 1, 0),  (7, -1, 0),  (8, -1, 0),
  (7, 2, 0),  (10, 2, 0),  (5, -1, 0),  (6, -1, 0),  (7, 0, 0),  (7, 1, 0),
  (6, 2, 0),  (5, 2, 0),  (5, 0, 0),  (5, 1, 0),  (10, 0, 0),  (10, 1, 0),
  (9, 2, 0),  (8, 2, 0),  (8, 0, 0),  (8, 1, 0),  (9, 0, 0)]]
  
sv_no_ed = [[[12, 11],  [5, 1],  [11, 3],  [5, 4],  [6, 1],  [7, 2],
  [8, 7],  [9, 8],  [10, 9],  [6, 10],  [14, 11],  [13, 0],  [14, 13],
  [30, 29],  [20, 15],  [22, 17],  [29, 18],  [28, 27],  [20, 19],
  [21, 15],  [22, 21],  [23, 17],  [24, 23],  [26, 24],  [25, 19],
  [26, 25],  [28, 18],  [32, 30],  [31, 16],  [32, 31],  [33, 31],
  [27, 33]]]

class SvFormulaShapeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Formula shape '''
    bl_idname = 'SvFormulaShapeNode'
    bl_label = 'Formula shape'
    bl_icon = 'IPO'
    
    # vertex numers
    number = IntProperty(name='number', description='vertex number', default=100,
                    options={'ANIMATABLE'},
                    update=updateNode)
    # scale
    scale = FloatProperty(name='scale', description='scale', default=1,
                    options={'ANIMATABLE'},
                    update=updateNode)
    # sphere
    XX = FloatProperty(name='XX', description='XX factor', default=1,
                    options={'ANIMATABLE'},
                    update=updateNode)
    # sphere
    YY = FloatProperty(name='YY', description='YY factor', default=1,
                    options={'ANIMATABLE'},
                    update=updateNode)
    # sphere
    ZZ = FloatProperty(name='ZZ', description='ZZ factor', default=1,
                    options={'ANIMATABLE'},
                    update=updateNode)
    
    # formula for usual case
    list_formulaX = [    'i*XX',
                        '(i/100)',
                        '(i**2)*XX',
                        'sqrt(i*XX)',
                        'sin(XX)',
                        'sin(i*XX)',
                        'sin(i*XX+i**2)',
                        'sin(i)/(XX+cos(i))',
                        'sin(cos(i*XX))',
                        'sin(i)*cos(i)*i*XX',
                        'sin(i)*i*XX',
                        'cos(XX)',
                        'cos(i*XX)',
                        'cos(i)*i*XX',
                        '(cos(i)**2)*i*XX',
                        'cos(i*XX)**3',
                        'cos(i/2)*i*XX-cos(i/2)',
                        'cos(i*XX+i**2)',
                        'cos(i)*i*XX',
                        'cos(i*XX)',
                        'tan(i*XX)',
                        'tan(sin(i*XX))',
                        'tan(cos(XX*i**2))',
                        'atan(cos(i*XX))',
                        'XX*log((1+ sin(i)))',
                        #sphere
                        'cos(XX) * cos(YY)',
                        'cos(XX) * sin(XX)',
                        'sin(XX) * cos(YY)',
                        'sin(XX) * sin(YY)',
                        'sin(YY) * sin(YY)',
                        'log(exp(sin(XX)) * exp(cos(YY))', 
                        'log(exp(sin(XX)) * exp(sin(YY))',
                        'exp(cos(XX))',
                        'sin(XX) * atan(YY)',
                        'tan(XX) * atan(YY)',
                        'sin(YY) * sin(YY) * sin(XX)',
                        'sin(XX) * sin(YY) * sin(YY)',
                        'sin(YY) * sin(YY) * cos(XX)',
                        'sin(XX) * cos(YY) * cos(YY)',
                        'sin(XX) * cos(YY) * sin(YY)',
                        'sin(YY) * cos(YY) * sin(YY)',
                        'sin(XX) * cos(YY) * log(YY, 2)/10',
                        'sin(YY) * cos(YY) * cos(XX)', ]
                        
    list_formulaY = [   'i*YY',
                        '(i/100)',
                        '(i**2)*YY',
                        'sqrt(i*YY)',
                        'sin(YY)',
                        'sin(i*YY)',
                        'sin(i*YY+i**2)',
                        'sin(i)/(YY+cos(i))',
                        'sin(cos(i*YY))',
                        'sin(i)*cos(i)*i*YY',
                        'sin(i)*i*YY',
                        'cos(YY)',
                        'cos(i*YY)',
                        'cos(i)*i*YY',
                        '(cos(i)**2)*i*YY',
                        'cos(i*YY)**3',
                        'cos(i/2)*i*YY-cos(i/2)',
                        'cos(i*YY+i**2)',
                        'cos(i)*i*YY',
                        'cos(i*YY)',
                        'tan(i*YY)',
                        'tan(sin(i*YY))',
                        'tan(cos(YY*i**2))',
                        'atan(cos(i*YY))',
                        'YY*log((1+ sin(i)))',
                        #sphere
                        'cos(XX) * cos(YY)',
                        'cos(XX) * sin(XX)',
                        'sin(XX) * cos(YY)',
                        'sin(XX) * sin(YY)',
                        'sin(YY) * sin(YY)',
                        'log(exp(sin(XX)) * exp(cos(YY))', 
                        'log(exp(sin(XX)) * exp(sin(YY))',
                        'exp(cos(XX))',
                        'sin(XX) * atan(YY)',
                        'tan(XX) * atan(YY)',
                        'sin(YY) * sin(YY) * sin(XX)',
                        'sin(XX) * sin(YY) * sin(YY)',
                        'sin(YY) * sin(YY) * cos(XX)',
                        'sin(XX) * cos(YY) * cos(YY)',
                        'sin(XX) * cos(YY) * sin(YY)',
                        'sin(YY) * cos(YY) * sin(YY)',
                        'sin(XX) * cos(YY) * log(YY, 2)/10',
                        'sin(YY) * cos(YY) * cos(XX)', ]
                        
    list_formulaZ = [   'i*ZZ',
                        '(i/100)',
                        '(i**2)*ZZ',
                        'sqrt(i*ZZ)',
                        'sin(ZZ)',
                        'sin(i*ZZ)',
                        'sin(i*ZZ+i**2)',
                        'sin(i)/(ZZ+cos(i))',
                        'sin(cos(i*ZZ))',
                        'sin(i)*cos(i)*i*ZZ',
                        'sin(i)*i*ZZ',
                        'cos(ZZ)',
                        'cos(i*ZZ)',
                        'cos(i)*i*ZZ',
                        '(cos(i)**2)*i*ZZ',
                        'cos(i*ZZ)**3',
                        'cos(i/2)*i*ZZ-cos(i/2)',
                        'cos(i*ZZ+i**2)',
                        'cos(i)*i*ZZ',
                        'cos(i*ZZ)',
                        'tan(i*ZZ)',
                        'tan(sin(i*ZZ))',
                        'tan(cos(ZZ*i**2))',
                        'atan(cos(i*ZZ))',
                        'ZZ*log((1+ sin(i)))',
                        #sphere
                        'cos(XX) * cos(YY)',
                        'cos(XX) * sin(XX)',
                        'sin(XX) * cos(YY)',
                        'sin(XX) * sin(YY)',
                        'sin(YY) * sin(YY)',
                        'log(exp(sin(XX)) * exp(cos(YY))', 
                        'log(exp(sin(XX)) * exp(sin(YY))',
                        'exp(cos(XX))',
                        'sin(XX) * atan(YY)',
                        'tan(XX) * atan(YY)',
                        'sin(YY) * sin(YY) * sin(XX)',
                        'sin(XX) * sin(YY) * sin(YY)',
                        'sin(YY) * sin(YY) * cos(XX)',
                        'sin(XX) * cos(YY) * cos(YY)',
                        'sin(XX) * cos(YY) * sin(YY)',
                        'sin(YY) * cos(YY) * sin(YY)',
                        'sin(XX) * cos(YY) * log(YY, 2)/10',
                        'sin(YY) * cos(YY) * cos(XX)', ]
                        
    formulaX_enum = [(i, i, 'formulaX {0}'.format(k), k) for k, i in enumerate(list_formulaX)]
    formulaY_enum = [(i, i, 'formulaY {0}'.format(k), k) for k, i in enumerate(list_formulaY)]
    formulaZ_enum = [(i, i, 'formulaZ {0}'.format(k), k) for k, i in enumerate(list_formulaZ)]
    formulaX = EnumProperty(items=formulaX_enum, name='formulaX',
                    options={'ANIMATABLE'},
                    update=updateNode)
    formulaY = EnumProperty(items=formulaY_enum, name='formulaY',
                    options={'ANIMATABLE'},
                    update=updateNode)
    formulaZ = EnumProperty(items=formulaZ_enum, name='formulaZ',
                    options={'ANIMATABLE'},
                    update=updateNode)
    
    list_X_X = [    'XX',
                    'i',
                    'i**2',
                    'pi*i',
                    'i*i*pi',
                    'i/pi',
                    'pi/i',
                    'i*XX',
                    'i/XX',
                    'XX/i',
                    'XX*pi*i',
                    'XX/i/pi',
                    'XX/pi*i',
                    'i*pi/XX',
                    'i/pi*XX',
                    'pi/i*XX',
                    'i*XX/pi',
                    '1/i*XX*pi',
                    # logs
                    'log(pi*i)+XX',
                    'log(i*1)/tan(XX*i)*50',
                    '12/pi*log(i)*XX',
                    '2*pi*log(i)*XX',
                    # trigono
                    'tan(i)',
                    'sin(i)',
                    'cos(i)',
                    'sin(XX/pi*i)',
                    'sin(tan(XX*i))*50',
                    'cos(XX*pi*i)',
                    'tan(XX*i*pi)',
                    'tan(i)*XX',
                    ]
    
    X_X_enum = [(i, i, i, k) for k, i in enumerate(list_X_X)]
    X_X = EnumProperty(items=X_X_enum, name='X_X',
                    options={'ANIMATABLE'},
                    update=updateNode)
                    
    list_Y_Y = [    'YY',
                    'i',
                    'i**2',
                    'pi*i',
                    'i*i*pi',
                    'i/pi',
                    'pi/i',
                    'i*YY',
                    'i/YY',
                    'YY/i',
                    'YY*pi*i',
                    'YY/i/pi',
                    'YY/pi*i',
                    'i*pi/YY',
                    'i/pi*YY',
                    'pi/i*YY',
                    'i*YY/pi',
                    '1/i*YY*pi',
                    # logs
                    'log(pi*i)+YY',
                    'log(i*1)/tan(YY*i)*50',
                    '12/pi*log(i)*YY',
                    '2*pi*log(i)*YY',
                    # trigono
                    'tan(i)',
                    'sin(i)',
                    'cos(i)',
                    'sin(YY/pi*i)',
                    'sin(tan(YY*i))*50',
                    'cos(YY*pi*i)',
                    'tan(YY*i*pi)',
                    'tan(i)*YY',
                    ]
    
    Y_Y_enum = [(i, i, i, k) for k, i in enumerate(list_Y_Y)]
    Y_Y = EnumProperty(items=Y_Y_enum, name='Y_Y',
                    options={'ANIMATABLE'},
                    update=updateNode)
    
    list_Z_Z = [    'ZZ',
                    'i',
                    'i**2',
                    'pi*i',
                    'i*i*pi',
                    'i/pi',
                    'pi/i',
                    'i*ZZ',
                    'i/ZZ',
                    'ZZ/i',
                    'ZZ*pi*i',
                    'ZZ/i/pi',
                    'ZZ/pi*i',
                    'i*pi/ZZ',
                    'i/pi*ZZ',
                    'pi/i*ZZ',
                    'i*ZZ/pi',
                    '1/i*ZZ*pi',
                    # logs
                    'log(pi*i)+ZZ',
                    'log(i*1)/tan(ZZ*i)*50',
                    '12/pi*log(i)*ZZ',
                    '2*pi*log(i)*ZZ',
                    # trigono
                    'tan(i)',
                    'sin(i)',
                    'cos(i)',
                    'sin(ZZ/pi*i)',
                    'sin(tan(ZZ*i))*50',
                    'cos(ZZ*pi*i)',
                    'tan(ZZ*i*pi)',
                    'tan(i)*ZZ',
                    ]
    
    Z_Z_enum = [(i, i, i, k) for k, i in enumerate(list_Z_Z)]
    Z_Z = EnumProperty(items=Z_Z_enum, name='Z_Z',
                    options={'ANIMATABLE'},
                    update=updateNode)
    
    list_i = [      'n*f',
                    'n*f*ZZ',
                    'n*f*YY',
                    'n*f*XX',
                    'n*f*ZZ*XX*YY', ]
    
    i_enum = [(i, i, 'i override {0}'.format(k), k) for k, i in enumerate(list_i)]
    i_override = EnumProperty(items=i_enum, name='i_override',
                    options={'ANIMATABLE'},
                    update=updateNode)
    
    # end veriables enumerate
    
    def makeverts(self, vert, f, XX, YY, ZZ, fx,fy,fz, X_X, Y_Y, Z_Z, i_over):
        ''' main function '''
        out=[]
        for n in range(vert):
            i = eval(i_over)
            XX = eval(X_X)
            YY = eval(Y_Y)
            ZZ = eval(Z_Z)
            X = eval(fx)
            Y = eval(fy)
            Z = eval(fz)
            out.append((X,Y,Z))
        return [out]

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "Count").prop_name = 'number'
        self.inputs.new('StringsSocket', "Scale").prop_name = 'scale'
        self.inputs.new('StringsSocket', "XX").prop_name = 'XX'
        self.inputs.new('StringsSocket', "YY").prop_name = 'YY'
        self.inputs.new('StringsSocket', "ZZ").prop_name = 'ZZ'
        self.outputs.new('VerticesSocket', "Verts", "Verts")
        self.outputs.new('StringsSocket', "Edges", "Edges")
        self.width = 400
    
    def draw_buttons(self,context,layout):
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, 'formulaX', text='')
        row.prop(self, 'formulaY', text='')
        row.prop(self, 'formulaZ', text='')
        row = col.row(align=True)
        row.prop(self, 'X_X', text='')
        row.prop(self, 'Y_Y', text='')
        row.prop(self, 'Z_Z', text='')
        col.prop(self, 'i_override', text='i')
    
    def process(self):
        # inputs
        Count = self.inputs['Count'].sv_get()[0][0]
        Scale = self.inputs['Scale'].sv_get()[0][0]
        SP1 = self.inputs['XX'].sv_get()[0][0]
        SP2 = self.inputs['YY'].sv_get()[0][0]
        SP3 = self.inputs['ZZ'].sv_get()[0][0]
        #print(self.formula, self.XX_YY, self.i_override)
        # outputs
        if self.outputs['Verts'].is_linked:
            try:
                out = self.makeverts(Count, Scale, SP1, SP2, SP3, 
                                self.formulaX, self.formulaY, self.formulaZ, 
                                self.X_X, self.Y_Y, self.Z_Z, self.i_override)
                self.outputs['Verts'].sv_set(out)
            except:
                print('Cannot calculate, formula generator')
                out = sv_no_ve
                edg = sv_no_ed
                self.outputs['Verts'].sv_set(sv_no_ve)
                self.outputs['Edges'].sv_set(sv_no_ed)
                return

        if self.outputs['Edges'].is_linked:
            edg = [[[i-1, i] for i in range(1, Count)]]
            self.outputs['Edges'].sv_set(edg)


def register():
    bpy.utils.register_class(SvFormulaShapeNode)


def unregister():
    bpy.utils.unregister_class(SvFormulaShapeNode)
if __name__ == '__main__':
    register()




### --------------------- Sphere setups: enable engine, Main sphere formula (further) and single preset to view this presets in action








#    i=i*1.2
#    tt = i*i
#    YY = 1/i*500
#    X = ( (sin(tt), sin(YY), cos(tt)) )
#    X = ( (sin(f), f/20, f/20 ))

### ------------- Over Spherical: This formulas can be used with sphericals variables presets, or this one:

#    f=1/1
#    i=i*0.5+0
#    tt = 2*pi*i #2=123456...
#    pp = log(i*3)/tan(tt)*0.1 #2=123456...

    


### ------------- Special: presets + formulas
#    i=i*0.8000+250
#    tt = 2*pi*i #2=123456...
#    pp = log(i*1)/tan(tt)*1 #2=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), exp(i/400))

#    i=i*1 # i=i*2
#    tt = 5*pi*i #2=123456... #5*pi*...
#    pp = log(i*4)/tan(tt)*1 #4=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), (i/100)+cos(tt))

#    i=i*0.5
#    tt = 6*pi*i #2=123456...
#    pp = log(i*3)/tan(tt)*1 #2=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), (i/100)+cos(tt))

#    i=i*0.5
#    tt = 8*pi*i #2=123456...
#    pp = log(i*3)/tan(tt)*2 #2=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), (i/100))

#    i=i*3 ## i-i*0.5
#    tt = 5*pi*i #2=123456...
#    pp = log(i*3)/tan(tt)*0.1 #2=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), (i/100))

#    i=i*0.5+220
#    tt = 5*pi*i #2=123456...
#    pp = log(i*3)/tan(tt)*0.1 #2=123456...
#    X = (cos(tt) * cos(pp), sin(tt)* sin(pp), (i/100))

#    i=i*1 # i=i*2
#    tt = 0.1*pi*i #2=123456... #5*pi*...
#    pp = log(i*4)/tan(tt)*1 #4=123456...
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), (i/100)+cos(tt))

#    i = i
#    tt = i*pi/2
#    pp = i*2
#    X = (sin(tt) * cos(pp), sin(tt)* sin(pp), cos(tt))

### ------------- Torus section:
#T1
#    tt = 90*3.1417*i #2
#    pp = 5/3.1417*i #25.5319
#    X = ((3+cos(pp)) * cos(tt)/2, (3+cos(pp))* sin(tt)/2, sin(pp)/2)
#T2
#    tt = 3.1417*i/3 #2
#    pp = 5/3.1417*i #25.5319
#    X = ((3+cos(pp)) * cos(tt)/2, (3+cos(pp))* sin(tt)/2, sin(pp)/2)
#T3
#    tt = i
#    pp = 12*i
#    X = ((3+cos(pp)) * cos(tt)/2, (3+cos(pp))* sin(tt)/2, sin(pp)/2)



