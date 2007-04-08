#!BPY
"""
Name: 'Solid Wireframe'
Blender: 243
Group: 'Mesh'
Tooltip: 'Make a solid wireframe copy of this mesh'
"""

# -------------------------------------------------------------------------- 
# Solid Wireframe1.0 by Campbell Barton (AKA Ideasman42) 
# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
# 
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either version 2 
# of the License, or (at your option) any later version. 
# 
# This program is distributed in the hope that it will be useful, 
# but WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
# GNU General Public License for more details. 
# 
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software Foundation, 
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. 
# 
# ***** END GPL LICENCE BLOCK ***** 
# --------------------------------------------------------------------------
import Blender
from Blender import Scene, Mesh, Window, sys
from Blender.Mathutils import AngleBetweenVecs
#from BPyMesh import faceAngles # get angles for face cornders
import BPyMesh
reload(BPyMesh)
faceAngles = BPyMesh.faceAngles

# works out the distanbce to inset the corners based on angles
from BPyMathutils import angleToLength
#import BPyMathutils 
#reload(BPyMathutils)
#angleToLength = BPyMathutils.angleToLength

import mesh_solidify

import BPyMessages
import bpy



def solid_wire(ob_orig, me_orig, sce, PREF_THICKNESS):
	# This function runs out of editmode with a mesh
	# error cases are alredy checked for
	
	inset_half = PREF_THICKNESS / 2
	del PREF_THICKNESS
	
	ob = ob_orig.copy()
	me = me_orig.copy()
	ob.link(me)
	sce.objects.selected = []
	sce.objects.link(ob)
	ob.sel = True
	
	# Modify the object
	
	
	# each face needs its own verts
	# orig_vert_count =len(me.verts)
	new_vert_count = len(me.faces) * 4
	for f in me.faces:
		if len(f) == 3:
			new_vert_count -= 1
	
	
	new_verts = []
	new_faces = []
	vert_index = len(me.verts)
	
	for f in me.faces:
		f_v_co = [v.co for v in f]
		angles = faceAngles(f_v_co)
		f_v_idx = [v.index for v in f]
		
		def new_vert(fi):
			co = f_v_co[fi]
			a = angles[fi]
			if a > 180:
				vert_inset = 1 * inset_half
			else:
				vert_inset = inset_half * angleToLength( abs((180-a) / 2) )
			
			# Calculate the inset direction
			co1 = f_v_co[fi-1]
			co2 = fi+1 # Wrap this index back to the start
			if co2 == len(f_v_co): co2 = 0
			co2 = f_v_co[co2]
			
			co1 = co1 - co
			co2 = co2 - co
			co1.normalize()
			co2.normalize()
			d = co1+co2
			# Done with inset direction
			
			d.length = vert_inset
			return co+d
			# return co+((cent-co)*inset)
		
		new_verts.extend([new_vert(i) for i in xrange(len(f_v_co))])
		
		if len(f_v_idx) == 4:
			new_faces.extend([\
			(f_v_idx[1], f_v_idx[0], vert_index, vert_index+1),\
			(f_v_idx[2], f_v_idx[1], vert_index+1, vert_index+2),\
			(f_v_idx[3], f_v_idx[2], vert_index+2, vert_index+3),\
			(f_v_idx[0], f_v_idx[3], vert_index+3, vert_index),\
			])
		
		
		if len(f_v_idx) == 3:
			new_faces.extend([\
			(f_v_idx[1], f_v_idx[0], vert_index, vert_index+1),\
			(f_v_idx[2], f_v_idx[1], vert_index+1, vert_index+2),\
			(f_v_idx[0], f_v_idx[2], vert_index+2, vert_index),\
			])
		
		vert_index += len(f_v_co)
		
	me.faces.delete(1, range(len(me.faces)))
	me.verts.extend(new_verts)
	me.faces.extend(new_faces)

	# External function, solidify
	me.sel = True
	mesh_solidify.solidify(me, -inset_half*2)


def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = bpy.scenes.active
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return 
	
	# Create the variables.
	PREF_THICK = Blender.Draw.Create(0.1)
	
	pup_block = [\
	('Thick:', PREF_THICK, 0.0001, 2.0, 'Skin thickness in mesh space.'),\
	]
	
	if not Blender.Draw.PupBlock('Solid Wireframe', pup_block):
		return
	
	# Saves the editmode state and go's out of 
	# editmode if its enabled, we cant make
	# changes to the mesh data while in editmode.
	is_editmode = Window.EditMode()
	
	Window.WaitCursor(1)
	me = ob_act.getData(mesh=1) # old NMesh api is default
	t = sys.time()
	
	# Run the mesh editing function
	solid_wire(ob_act, me, sce, PREF_THICK.val)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'Solid Wireframe finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	if is_editmode: Window.EditMode(1)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()