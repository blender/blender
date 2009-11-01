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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy, Mathutils
from math import cos, sin, pi, radians


def add_torus(PREF_MAJOR_RAD, PREF_MINOR_RAD, PREF_MAJOR_SEG, PREF_MINOR_SEG):
	Vector = Mathutils.Vector
	Quaternion = Mathutils.Quaternion
	
	PI_2= pi*2
	Z_AXIS = 0,0,1
	
	verts = []
	faces = []
	i1 = 0
	tot_verts = PREF_MAJOR_SEG * PREF_MINOR_SEG
	for major_index in range(PREF_MAJOR_SEG):
		verts_tmp = []
		quat = Quaternion( Z_AXIS, (major_index/PREF_MAJOR_SEG)*PI_2)

		for minor_index in range(PREF_MINOR_SEG):
			angle = 2*pi*minor_index/PREF_MINOR_SEG
			
			vec = Vector(PREF_MAJOR_RAD+(cos(angle)*PREF_MINOR_RAD), 0, (sin(angle)*PREF_MINOR_RAD)) * quat
			verts.extend([vec.x, vec.y, vec.z])
			
			if minor_index+1==PREF_MINOR_SEG:
				i2 = (major_index)*PREF_MINOR_SEG
				i3 = i1 + PREF_MINOR_SEG
				i4 = i2 + PREF_MINOR_SEG
				
			else:
				i2 = i1 + 1
				i3 = i1 + PREF_MINOR_SEG
				i4 = i3 + 1
			
			if i2>=tot_verts:	i2 = i2-tot_verts
			if i3>=tot_verts:	i3 = i3-tot_verts
			if i4>=tot_verts:	i4 = i4-tot_verts
			
			# stupid eekadoodle
			if i2:	faces.extend( [i1,i3,i4,i2] )
			else:	faces.extend( [i2,i1,i3,i4] )
				
			i1+=1
	
	return verts, faces

from bpy.props import *

class MESH_OT_primitive_torus_add(bpy.types.Operator):
	'''Add a torus mesh.'''
	bl_idname = "mesh.primitive_torus_add"
	bl_label = "Add Torus"
	bl_register = True
	bl_undo = True
	
	major_radius = FloatProperty(name="Major Radius", description="Number of segments for the main ring of the torus", default= 1.0, min= 0.01, max= 100.0)
	minor_radius = FloatProperty(name="Minor Radius", description="Number of segments for the minor ring of the torus", default= 0.25, min= 0.01, max= 100.0)
	major_segments = IntProperty(name="Major Segments", description="Number of segments for the main ring of the torus", default= 48, min= 3, max= 256)
	minor_segments = IntProperty(name="Minor Segments", description="Number of segments for the minor ring of the torus", default= 16, min= 3, max= 256)
	
	def execute(self, context):
		verts_loc, faces = add_torus(self.major_radius, self.minor_radius, self.major_segments, self.minor_segments)
		
		me= bpy.data.add_mesh("Torus")
		
		me.add_geometry(int(len(verts_loc)/3), 0, int(len(faces)/4))
		me.verts.foreach_set("co", verts_loc)
		me.faces.foreach_set("verts_raw", faces)
		
		sce = context.scene
		
		# ugh
		for ob in sce.objects:
			ob.selected = False
		
		me.update()
		ob= bpy.data.add_object('MESH', "Torus")
		ob.data= me
		context.scene.add_object(ob)
		context.scene.active_object = ob
		ob.selected = True
		
		ob.location = tuple(context.scene.cursor_location)
		
		return ('FINISHED',)

# Register the operator
bpy.ops.add(MESH_OT_primitive_torus_add)

# Add to a menu
import dynamic_menu
import space_info
menu_item = dynamic_menu.add(bpy.types.INFO_MT_mesh_add, (lambda self, context: self.layout.itemO("mesh.primitive_torus_add", text="Torus", icon='ICON_MESH_DONUT')) )

if __name__ == "__main__":
	bpy.ops.mesh.primitive_torus_add()