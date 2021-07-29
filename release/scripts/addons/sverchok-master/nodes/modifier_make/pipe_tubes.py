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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty
from mathutils import Vector
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_cycle
from math import sin, cos, radians, sqrt


class SvPipeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Pipe from edges '''
    bl_idname = 'SvPipeNode'
    bl_label = 'Pipe'
    bl_icon = 'OUTLINER_OB_EMPTY'

    #nsides = IntProperty(name='nsides', description='number of sides',
    #              default=4, min=4, max=24,
    #              options={'ANIMATABLE'}, update=updateNode)
    diameter = FloatProperty(name='diameter', description='diameter',
                  default=1.0,
                  options={'ANIMATABLE'}, update=updateNode)
    shape_prop = [('Square','Square','Square'),('Round','Round','Round')]
    shape = EnumProperty(items=shape_prop, name='shape',default='Square',
                         options={'ANIMATABLE'}, update=updateNode)
    #offset = FloatProperty(name='offset', description='offset from ends',
    #              default=0.0, min=-1, max=0.5,
    #              options={'ANIMATABLE'}, update=updateNode)
    #extrude = FloatProperty(name='extrude', description='extrude',
    #              default=1.0, min=0.2, max=10.0,
    #              options={'ANIMATABLE'}, update=updateNode)
    close = BoolProperty(name='close', description='close ends',
                  default=True,
                  options={'ANIMATABLE'}, update=updateNode)
    cup_fill = BoolProperty(name='close', description='close ends',
                  default=True,
                  options={'ANIMATABLE'}, update=updateNode)


    def draw_buttons(self, context, layout):
        layout.prop(self,'close',text='close')
        layout.prop(self,'cup_fill',text='cup_fill')
        layout.prop(self,'shape',expand=True)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vers', 'Vers')
        self.inputs.new('StringsSocket', "Edgs", "Edgs")
        #self.inputs.new('StringsSocket', "Diameter", "Diameter").prop_name = 'diameter'
        #self.inputs.new('StringsSocket', "Sides", "Sides").prop_name = 'nsides'
        #self.inputs.new('StringsSocket', "Offset", "Offset").prop_name = 'offset'
        s = self.inputs.new('VerticesSocket', "Size", "Size")
        s.use_prop = True
        s.prop = (0.5,0.5,0.5)
        #self.inputs.new('StringsSocket', "Extrude", "Extrude").prop_name = 'extrude'
        self.outputs.new('VerticesSocket', 'Vers', 'Vers')
        self.outputs.new('StringsSocket', "Pols", "Pols")

    def process(self):
        
        if self.outputs['Vers'].is_linked and self.inputs['Vers'].is_linked:
            Vecs = self.inputs['Vers'].sv_get()
            Edgs = self.inputs['Edgs'].sv_get()
            #Nsides = max(self.inputs['Sides'].sv_get()[0][0], 4)
            Shape = self.shape
            Cup = self.cup_fill
            #Diameter = self.inputs['Diameter'].sv_get()[0][0]
            Diameter = 1.0
            Size = self.inputs['Size'].sv_get()[0]
            #Offset = self.inputs['Offset'].sv_get()[0][0]
            #Extrude = self.inputs['Extrude'].sv_get()[0][0]
            outv, outp = self.Do_vecs(Vecs,Edgs,Diameter,Shape,Size,Cup) #Nsides,Offset,Extrude)

            if self.outputs['Vers'].is_linked:
                self.outputs['Vers'].sv_set(outv)
            if self.outputs['Pols'].is_linked:
                self.outputs['Pols'].sv_set(outp)
    

    def Do_vecs(self, Vecs,Edgs,Diameter,Shape,Size,Cup): #Offset,Extrude):
        if Shape == 'Square':
            Nsides = 4
            Diameter = Diameter*sqrt(2)/2
            Sides = 90
        else:
            Nsides = 12
            Diameter = Diameter/2
            Sides = 30

        outv = []
        outp = []
        for E,V in zip(Edgs,Vecs):
            outv_ = []
            outp_ = []
            k = 0
            Size, E = match_long_cycle([Size,E])
            for e,S in zip(E,Size):
                S0,S1,S2 = S
                S2 = (S2-1)/2
                circle = [ (Vector((sin(radians(i))*S0,cos(radians(i))*S1,0))*Diameter) \
                            for i in range(45,405,Sides) ]
                v2,v1 = Vector(V[e[1]]),Vector(V[e[0]])
                vecdi = v2-v1
                matrix_rot = vecdi.rotation_difference(Vector((0,0,1))).to_matrix().to_4x4()
                verts1 = [ (ve*matrix_rot+v1-vecdi*S2)[:] for ve in circle ]
                verts2 = [ (ve*matrix_rot+v2+vecdi*S2)[:] for ve in circle ]
                outv_.extend(verts1)
                outv_.extend(verts2)
                pols = [ [k+i+0,k+i-1,k+i+Nsides-1,k+i+Nsides] for i in range(1,Nsides,1) ]
                pols.append([k+0,k+Nsides-1,k+Nsides*2-1,k+Nsides])
                if Cup:
                    p1 = [ k+i for i in reversed(range(Nsides,Nsides*2,1)) ]
                    p2 = [ k+i for i in range(0,Nsides,1) ]
                    pols.append(p1)
                    pols.append(p2)
                    
                if self.close and k!=0:
                    p = [ [k+i+0-Nsides,k+i-1-Nsides,k+i-1,k+i] for i in range(1,Nsides,1) ]
                    pols.extend(p)
                    pols.append([k+0-Nsides,k-1,k+Nsides-1,k])
                outp_.extend(pols)
                k += Nsides*2
            outv.append(outv_)
            outp.append(outp_)
        return outv, outp



def register():
    bpy.utils.register_class(SvPipeNode)


def unregister():
    bpy.utils.unregister_class(SvPipeNode)

if __name__ == '__main__':
    register()

