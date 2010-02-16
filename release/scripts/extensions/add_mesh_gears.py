# add_mesh_gear.py (c) 2009, 2010 Michel J. Anders (varkenvarken)
#
# add gears/cogwheels to the blender 2.50 add->mesh menu
#
# tested with the official blender 2.50 alpha 0 32-bit windows
# also tested with trunk svn 26208 on 32-bit windows
#
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


# blender 1 line description
"Add Gears (View3D > Add > Mesh > Gears)"

"""
What was needed to port it from 2.49 -> 2.50 alpha 0?

The basic functions that calculate the geometry (verts and faces) are unchanged
( add_tooth(), add_spoke2(), add_gear() )

These functions were designed to return lists of tuples (x,y,z) (for the vertices) and
lists of lists [i,k,l,m] (for the faces). Because the Blender 2.50 API does not provide
facilties to alter individual elements of the the verts and faces attributes of a mesh
directly we have to add the calculated vertices and faces in bulk by using the
mesh.add_geometry(nverts,nedges,nfaces) methodfolowed by
mesh.verts.foreach_set("co", verts_loc) and mesh.faces.foreach_set("verts_raw", faces).

Both the foreach_set() methods take flattened lists as arguments, not lists of tuples, so we
added a simple function to flatten a list of lists or tuples.

Also, the vertex group API is changed a little bit but the concepts are the same:
vertexgroup = ob.add_vertex_group('NAME_OF_VERTEXGROUP') # add a vertex group
for i in vertexgroup_vertex_indices:
    ob.add_vertex_to_group(i, vertexgroup, weight, 'ADD')

Now for some reason the name does not 'stick' and we have to set it this way:
vertexgroup.name = 'NAME_OF_VERTEXGROUP'
        
Conversion to 2.50 also meant we could simply do away with our crude user interface.
Just definining the appropriate properties in the AddGear() operator will display the
properties in the Blender GUI with the added benefit of making it interactive: changing
a property will redo the AddGear() operator providing the user with instant feedback.

FInally we had to convert/throw away some print statements to print functions as Blender
nows uses Python 3.x

The most puzzling issue was that the built in Python zip() function changed its behavior.
In 3.x it returns a zip object (that can be iterated over) instead of a list of tuples. This meant
we could no longer use deepcopy(zip(...)) but had to convert the zip object to a list of tuples
first.

The code to actually implement the AddGear() function is mostly copied from add_mesh_torus()
(distributed with Blender). 

Unresolved issues:

- removing doubles:
    the code that produces the teeth of the gears produces some duplicate vertices. The original
    script just called remove_doubles() but if we do that in 2.50 we have a problem. To apply
    the bpy.ops.mesh.remove_doubles() operator we have to change to edit mode. The moment
    we do that we loose to possibilty to interactively change the properties. Also changing back
    to object mode raises a strange exception (to investigate). So for now, removing doubles is left
    to the user once satisfied with the chosen setting for a gear,

- no suitable icon:
    a rather minor point but I reused the torus icon for the add->mesh->gear menu entry as there
    doesn't seem to be a generic mesh icon or a way to add custom icons. Too bad, but as this is
    just eye candy it's no big deal.

"""

import bpy
import Mathutils
from math import cos, sin, tan, atan, asin, pi,radians as rad
from copy import deepcopy as dc

def flatten(alist):
    "Flatten a list of lists or tuples."
    return sum([list(a) for a in alist],[])

#constants
faces=[[0,5,6,1],[1,6,7,2],[2,7,8,3],[3,8,9,4],[6,10,11,7],[7,11,12,8],[10,13,14,11],[11,14,15,12]]
L=16 # number of vertices
#edgefaces
ef  = [5,6,10,13,14,15,12,8,9]
ef2 = [i+L for i in ef]
# in python 3, zip() returns a zip object so we have to force the result into a list of lists to keep
# deepcopy happy later on in the script.
efc = [ [i,j,k,l] for i,j,k,l in zip(ef[:-1],ef2[:-1],ef2[1:],ef[1:])]
vv  = [5,6,8,9,21,22,24,25] #vertices in a valley
tv  = [13,14,15,29,30,31]   #vertices on a tooth

spokefaces=((0,1,2,5),(2,3,4,7),(5,2,7,6),(5,6,9,8),(6,7,10,9),(11,8,13,12),(8,9,10,13),(13,10,15,14))

def add_tooth(a,t,d,r,Ad,De,b,p,rack=0,crown=0.0):
	"""
	private function: calculate the vertex coords for a single side
	section of a gear tooth. returns them as a list of lists.
	"""
	
	A=[a,a+t/4,a+t/2,a+3*t/4,a+t]
	C=[cos(i) for i in A] 
	S=[sin(i) for i in A]
	
	Ra=r+Ad
	Rd=r-De
	Rb=Rd-b
	
	#Pressure angle calc
	O =Ad*tan(p)
	p =atan(O/Ra)
	if r<0 : p = -p
	
	if rack :
		S =[sin(t/4)*I for I in range(-2,3)]
		Sp=[0,sin(-t/4+p),0,sin(t/4-p)]

		v=[(Rb,r*S[I],d) for I in range(5)]
		v.extend([(Rd,r*S[I],d) for I in range(5)])
		v.extend([(r,r*S[I],d) for I in range(1,4)])
		v.extend([(Ra,r*Sp[I],d) for I in range(1,4)])
		
	else :
		Cp=[0,cos(a+t/4+p),cos(a+t/2),cos(a+3*t/4-p)]
		Sp=[0,sin(a+t/4+p),sin(a+t/2),sin(a+3*t/4-p)]

		v=[(Rb*C[I],Rb*S[I],d) for I in range(5)]
		v.extend([(Rd*C[I],Rd*S[I],d) for I in range(5)])
		v.extend([(r*C[I],r*S[I],d+crown/3) for I in range(1,4)])
		v.extend([(Ra*Cp[I],Ra*Sp[I],d+crown) for I in range(1,4)])
		
	return v

def add_spoke2(a,t,d,r,De,b,s,w,l,gap=0,width=19):
	"""
	EXPERIMENTAL private function: calculate the vertex coords for a single side
	section of a gearspoke. returns them as a list of lists.
	"""
	
	Rd=r-De
	Rb=Rd-b
	Rl=Rb
	
	v  =[]
	ef =[]
	ef2=[]
	sf =[]
	if not gap :
		for N in range(width,1,-2) :
			ef.append(len(v))
			ts = t/4
			tm = a + 2*ts
			te = asin(w/Rb)
			td = te - ts
			t4 = ts+td*(width-N)/(width-3.0)
			A=[tm+(i-int(N/2))*t4 for i in range(N)]
			C=[cos(i) for i in A] 
			S=[sin(i) for i in A]
			v.extend([ (Rb*I,Rb*J,d) for (I,J) in zip(C,S)])
			ef2.append(len(v)-1)
			Rb= Rb-s
		n=0
		for N in range(width,3,-2) :
			sf.extend([(i+n,i+1+n,i+2+n,i+N+n) for i in range(0,N-1,2)])
			sf.extend([(i+2+n,i+N+n,i+N+1+n,i+N+2+n) for i in range(0,N-3,2)])
			n = n + N
		
	return v,ef,ef2,sf

def add_gear(N,r,Ad,De,b,p,D=1,skew=0,conangle=0,rack=0,crown=0.0, spoke=0,spbevel=0.1,spwidth=0.2,splength=1.0,spresol=9):
	"""
	"""
	worm	=0
	if N<5 : (worm,N)=(N,24)
	t	  =2*pi/N
	if rack: N=1
	p	    =rad(p)
	conangle=rad(conangle)
	skew	=rad(skew)
	scale   = (r - 2*D*tan(conangle) )/r
	
	f =[]
	v =[]
	tg=[] #vertexgroup of top vertices.
	vg=[] #vertexgroup of valley vertices
	
	
	M=[0]
	if worm : (M,skew,D)=(range(32),rad(11.25),D/2)
	
	for W in M:
		fl=W*N*L*2
		l=0	#number of vertices
		for I in range(int(N)):
			a=I*t
			for(s,d,c,first) in ((W*skew,W*2*D-D,1,1),((W+1)*skew,W*2*D+D,scale,0)):
				if worm and I%(int(N)/worm)!=0:
					v.extend(add_tooth(a+s,t,d,r-De,0.0,0.0,b,p))
				else:
					v.extend(add_tooth(a+s,t,d,r*c,Ad*c,De*c,b*c,p,rack,crown))
				if not worm or (W==0 and first) or (W==(len(M)-1) and not first) :	
					f.extend([ [j+l+fl for j in i]for i in dc(faces)])
				l += L

			#print (len(f))
			#print (dc(efc))
			f.extend([ [j+I*L*2+fl for j in i] for i in dc(efc)])
			#print (len(f))
			tg.extend([i+I*L*2 for i in tv])
			vg.extend([i+I*L*2 for i in vv])
	# EXPERIMENTAL: add spokes
	if not worm and spoke>0 :
		fl=len(v)
		for I in range(int(N)):
			a=I*t
			s=0 # for test
			if I%spoke==0 :
				for d in (-D,D) :
					(sv,ef,ef2,sf) = add_spoke2(a+s,t,d,r*c,De*c,b*c,spbevel,spwidth,splength,0,spresol)
					v.extend(sv)
					f.extend([ [j+fl for j in i]for i in sf])
					fl += len(sv)
				d1 = fl-len(sv)
				d2 = fl-2*len(sv)
				f.extend([(i+d2,j+d2,j+d1,i+d1) for (i,j) in zip(ef[:-1],ef[1:])])
				f.extend([(i+d2,j+d2,j+d1,i+d1) for (i,j) in zip(ef2[:-1],ef2[1:])])
			else :
				for d in (-D,D) :
					(sv,ef,ef2,sf) = add_spoke2(a+s,t,d,r*c,De*c,b*c,spbevel,spwidth,splength,1,spresol)
					v.extend(sv)
					fl += len(sv)
				d1 = fl-len(sv)
				d2 = fl-2*len(sv)
				#f.extend([(i+d2,i+1+d2,i+1+d1,i+d1) for (i) in (0,1,2,3)])
				#f.extend([(i+d2,i+1+d2,i+1+d1,i+d1) for (i) in (5,6,7,8)])
					
	return flatten(v), flatten(f), tg, vg


from bpy.props import *


class AddGear(bpy.types.Operator):
    '''Add a gear mesh.'''
    bl_idname = "mesh.gear_add"
    bl_label = "Add Gear"
    bl_register = True
    bl_undo = True

    number_of_teeth = IntProperty(name="Number of Teeth",
                                  description="Number of teeth on the gear",
                                  default=12,min=4,max=200)
    radius = FloatProperty(name="Radius",
                           description="Radius of the gear, negative for crown gear",
                           default=1.0, min=-100.0, max=100.0)
    addendum = FloatProperty(name="Addendum",
                           description="Addendum, extent of tooth above radius",
                           default=0.1, min=0.01, max=100.0)
    dedendum = FloatProperty(name="Dedendum",
                           description="Dedendum, extent of tooth below radius",
                           default=0.1, min=0.0, max=100.0)
    angle = FloatProperty(name="Pressure Angle",
                           description="Pressure angle, skewness of tooth tip (degrees)",
                           default=20.0, min=0.0, max=45.0)
    base = FloatProperty(name="Base",
                           description="Base, extent of gear below radius",
                           default=0.2, min=0.0, max=100.0)
    width = FloatProperty(name="Width",
                           description="Width, thickness of gear",
                           default=0.2, min=0.05, max=100.0)
    skew = FloatProperty(name="Skewness",
                           description="Skew of teeth (degrees)",
                           default=0.0, min=-90.0, max=90.0)
    conangle = FloatProperty(name="Conical angle",
                           description="Conical angle of gear (degrees)",
                           default=0.0, min=0.0, max=90.0)
    crown = FloatProperty(name="Crown",
                           description="Inward pointing extend of crown teeth",
                           default=0.0, min=0.0, max=100.0)

    def execute(self, context):

        verts_loc, faces, tip_vertices, valley_vertices = add_gear(self.properties.number_of_teeth,
                                    self.properties.radius,
                                    self.properties.addendum,
                                    self.properties.dedendum,
                                    self.properties.base,
                                    self.properties.angle,
                                    self.properties.width,
                                    skew=self.properties.skew,
                                    conangle=self.properties.conangle,
                                    crown=self.properties.crown)

        #print(len(verts_loc)/3,faces)
        
        mesh = bpy.data.meshes.new("Gear")

        mesh.add_geometry(int(len(verts_loc) / 3), 0, int(len(faces) / 4))
        mesh.verts.foreach_set("co", verts_loc)
        mesh.faces.foreach_set("verts_raw", faces)

        scene = context.scene

        # ugh (to quote the author of add_mesh_torus :-)
        for ob in scene.objects:
            ob.selected = False

        mesh.update()
        
        ob_new = bpy.data.objects.new('Gear','MESH')
        ob_new.data = mesh

        tipgroup = ob_new.add_vertex_group('Tips')
        # for some reason the name does not 'stick' and we have to set it this way:
        tipgroup.name = 'Tips'
        for i in tip_vertices:
            ob_new.add_vertex_to_group(i, tipgroup, 1.0, 'ADD')
            
        valleygroup = ob_new.add_vertex_group('Valleys')
        # for some reason the name does not 'stick' and we have to set it this way:
        valleygroup.name = 'Valleys'
        for i in valley_vertices:
            ob_new.add_vertex_to_group(i, valleygroup, 1.0, 'ADD')
            
        scene.objects.link(ob_new)
        scene.objects.active = ob_new
        ob_new.selected = True

        #print(1,bpy.context.mode)
        #bpy.ops.object.mode_set(mode='EDIT')
        #print(2,bpy.context.mode)
        #bpy.ops.mesh.remove_doubles()
        #print(3,bpy.context.mode)
        # unfortunately the next line wont get us back to object mode but bombs
        #bpy.ops.object.mode_set('OBJECT')
        #print(4,bpy.context.mode)
                
        ob_new.location = tuple(context.scene.cursor_location)

        return {'FINISHED'}


# Add to a menu, reuse an icon used elsewhere that happens to have fitting name
# unfortunately, the icon shown is the one I expected from looking at the
# blenderbuttons file from the release/datafiles directory

menu_func = (lambda self, context: self.layout.operator(AddGear.bl_idname,
                                        text="Gear", icon='GEARS'))

def register():
    bpy.types.register(AddGear)
    bpy.types.INFO_MT_mesh_add.append(menu_func)

def unregister():
    bpy.types.unregister(AddGear)
    bpy.types.INFO_MT_mesh_add.remove(menu_func)
