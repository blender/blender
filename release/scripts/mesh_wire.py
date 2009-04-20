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
from Blender.Mathutils import AngleBetweenVecs, TriangleNormal
from BPyMesh import faceAngles # get angles for face cornders
#import BPyMesh
#reload(BPyMesh)
#faceAngles = BPyMesh.faceAngles

# works out the distanbce to inset the corners based on angles
from BPyMathutils import angleToLength
#import BPyMathutils 
#reload(BPyMathutils)
#angleToLength = BPyMathutils.angleToLength

import mesh_solidify

import BPyMessages
reload(BPyMessages)
import bpy


def solid_wire(ob_orig, me_orig, sce, PREF_THICKNESS, PREF_SOLID, PREF_SHARP, PREF_XSHARP):
	if not PREF_SHARP and PREF_XSHARP:
		PREF_XSHARP = False
	
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
	sce.objects.active = ob
	
	# Modify the object, should be a set
	FGON= Mesh.EdgeFlags.FGON
	edges_fgon = dict([(ed.key,None) for ed in me.edges if ed.flag & FGON])
	# edges_fgon.fromkeys([ed.key for ed in me.edges if ed.flag & FGON])
	
	del FGON

	
	
	# each face needs its own verts
	# orig_vert_count =len(me.verts)
	new_vert_count = len(me.faces) * 4
	for f in me.faces:
		if len(f) == 3:
			new_vert_count -= 1
	
	if PREF_SHARP == 0:
		new_faces_edge= {}
		
		def add_edge(i1,i2, ni1, ni2):
			
			if i1>i2:
				i1,i2 = i2,i1
				flip = True
			else:
				flip = False
			new_faces_edge.setdefault((i1,i2), []).append((ni1, ni2, flip))
		
	
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
		
		new_verts.extend([new_vert(i) for i in xrange(len(f_v_co))])
		
		if len(f_v_idx) == 4:
			faces = [\
			(f_v_idx[1], f_v_idx[0], vert_index, vert_index+1),\
			(f_v_idx[2], f_v_idx[1], vert_index+1, vert_index+2),\
			(f_v_idx[3], f_v_idx[2], vert_index+2, vert_index+3),\
			(f_v_idx[0], f_v_idx[3], vert_index+3, vert_index),\
			]
		else:
			faces = [\
			(f_v_idx[1], f_v_idx[0], vert_index, vert_index+1),\
			(f_v_idx[2], f_v_idx[1], vert_index+1, vert_index+2),\
			(f_v_idx[0], f_v_idx[2], vert_index+2, vert_index),\
			]
		
		
		if PREF_SHARP == 1:
			if not edges_fgon:
				new_faces.extend(faces)
			else:
				for nf in faces:
					i1,i2 = nf[0], nf[1]
					if i1>i2: i1,i2 = i2,i1
					
					if edges_fgon and (i1,i2) not in edges_fgon:
						new_faces.append(nf)
			
			
		
		elif PREF_SHARP == 0:
			for nf in faces:
				add_edge(*nf)
			
		vert_index += len(f_v_co)
	
	me.verts.extend(new_verts)
	
	if PREF_SHARP == 0:
		def add_tri_flipped(i1,i2,i3):
			try:
				if AngleBetweenVecs(me.verts[i1].no, TriangleNormal(me.verts[i1].co, me.verts[i2].co, me.verts[i3].co)) < 90:
					return i3,i2,i1
				else:
					return i1,i2,i3
			except:
				return i1,i2,i3
		
		# This stores new verts that use this vert
		# used for re-averaging this verts location
		# based on surrounding verts. looks better but not needed.
		vert_users = [set() for i in xrange(vert_index)]
		
		for (i1,i2), nf in new_faces_edge.iteritems():
			
			if len(nf) == 2:
				# Add the main face
				if edges_fgon and (i1,i2) not in edges_fgon:
					new_faces.append((nf[0][0], nf[0][1], nf[1][0], nf[1][1]))
				
				
				if nf[0][2]:	key1 = nf[0][1],nf[0][0]
				else:			key1 = nf[0][0],nf[0][1]
				if nf[1][2]:	key2 = nf[1][1],nf[1][0]
				else:			key2 = nf[1][0],nf[1][1]
				
				# CRAP, cont work out which way to flip so make it oppisite the verts normal.
				
				###new_faces.append((i2, key1[0], key2[0])) # NO FLIPPING, WORKS THOUGH
				###new_faces.append((i1, key1[1], key2[1]))
				new_faces.append(add_tri_flipped(i2, key1[0], key2[0]))
				new_faces.append(add_tri_flipped(i1, key1[1], key2[1]))
				
				# Average vert loction so its not tooo pointy
				# not realy needed but looks better
				vert_users[i2].update((key1[0], key2[0]))
				vert_users[i1].update((key1[1], key2[1]))
			
			if len(nf) == 1:
				if nf[0][2]:	new_faces.append((nf[0][0], nf[0][1], i2, i1)) # flipped
				else:			new_faces.append((i1,i2, nf[0][0], nf[0][1]))
				
		
		# average points now.
		for i, vusers in enumerate(vert_users):
			if vusers:
				co = me.verts[i].co
				co.zero()
				
				for ii in vusers:
					co += me.verts[ii].co
				co /= len(vusers)
	
	me.faces.delete(1, range(len(me.faces)))
	
	me.faces.extend(new_faces)

	# External function, solidify
	me.sel = True
	if PREF_SOLID:
		mesh_solidify.solidify(me, -inset_half*2, True, False, PREF_XSHARP)


def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = bpy.data.scenes.active
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return 
	
	# Saves the editmode state and go's out of 
	# editmode if its enabled, we cant make
	# changes to the mesh data while in editmode.
	is_editmode = Window.EditMode()
	Window.EditMode(0)
	
	me = ob_act.getData(mesh=1) # old NMesh api is default
	if len(me.faces)==0:
		BPyMessages.Error_NoMeshFaces()
		if is_editmode: Window.EditMode(1)
		return
	
	# Create the variables.
	PREF_THICK = Blender.Draw.Create(0.005)
	PREF_SOLID = Blender.Draw.Create(1)
	PREF_SHARP = Blender.Draw.Create(1)
	PREF_XSHARP = Blender.Draw.Create(0)
	
	pup_block = [\
	('Thick:', PREF_THICK, 0.0001, 2.0, 'Skin thickness in mesh space.'),\
	('Solid Wire', PREF_SOLID, 'If Disabled, will use 6 sided wire segments'),\
	('Sharp Wire', PREF_SHARP, 'Use the original mesh topology for more accurate sharp wire.'),\
	('Extra Sharp', PREF_XSHARP, 'Use less geometry to create a sharper looking wire'),\
	]
	
	if not Blender.Draw.PupBlock('Solid Wireframe', pup_block):
		if is_editmode: Window.EditMode(1)
		return
	
	Window.WaitCursor(1)
	t = sys.time()
	
	# Run the mesh editing function
	solid_wire(ob_act, me, sce, PREF_THICK.val, PREF_SOLID.val, PREF_SHARP.val, PREF_XSHARP.val)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'Solid Wireframe finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	if is_editmode: Window.EditMode(1)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()
