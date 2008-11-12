#!BPY
"""
Name: 'Grow/Shrink Weight...'
Blender: 245
Group: 'WeightPaint'
Tooltip: 'Grow/Shrink active vertex group.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Grow Shrink Weight

This Script is to be used only in weight paint mode,
It grows/shrinks the bounds of the weight painted area
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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

from Blender import Scene, Draw, Window
import BPyMesh
def actWeightNormalize(me, PREF_MODE, PREF_MAX_DIST, PREF_STRENGTH, PREF_ITERATIONS):
	Window.WaitCursor(1)
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	act_group= me.activeGroup
	
	# Start with assumed zero weights
	orig_vert_weights= [0.0] * len(vWeightDict) # Will be directly assigned to orig_vert_weights
	
	
	# fill in the zeros with real weights.
	for i, wd in enumerate(vWeightDict):
		try:
			orig_vert_weights[i]= wd[act_group]
		except:
			pass
	
	new_vert_weights= list(orig_vert_weights)
	
	for dummy in xrange(PREF_ITERATIONS):
		# Minimize or maximize the weights. connection based.
		
		if PREF_MODE==0: # Grow
			op= max
		else: # Shrink
			op= min
		
		for ed in me.edges:
			if not PREF_MAX_DIST or ed.length < PREF_MAX_DIST:
			
				i1= ed.v1.index
				i2= ed.v2.index
				new_weight= op(orig_vert_weights[i1], orig_vert_weights[i2])
				
				if PREF_STRENGTH==1.0: # do a full copy
					new_vert_weights[i1]= op(new_weight, new_vert_weights[i1])
					new_vert_weights[i2]= op(new_weight, new_vert_weights[i2])
					
				else: # Do a faded copy
					new_vert_weights[i1]= op(new_weight, new_vert_weights[i1])
					new_vert_weights[i2]= op(new_weight, new_vert_weights[i2])
					
					# Face the copy with the original (orig is updated per iteration)
					new_vert_weights[i1]= (new_vert_weights[i1]*PREF_STRENGTH) + (orig_vert_weights[i1]*(1-PREF_STRENGTH))
					new_vert_weights[i2]= (new_vert_weights[i2]*PREF_STRENGTH) + (orig_vert_weights[i2]*(1-PREF_STRENGTH))
		
		
		for i, wd in enumerate(vWeightDict):
			new_weight= new_vert_weights[i]
			if new_weight != orig_vert_weights[i]:
				wd[act_group]= new_weight
		
		if dummy+1 != PREF_ITERATIONS: # dont copy the list on the last round.
			orig_vert_weights= list(new_vert_weights)
		
		
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	Window.WaitCursor(0)

def main():
	scn= Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_MAXDIST= Draw.Create(0.0)
	PREF_STRENGTH= Draw.Create(1.0)
	PREF_MODE= Draw.Create(0)
	PREF_ITERATIONS= Draw.Create(1)
	
	pup_block= [\
	('Bleed Dist:', PREF_MAXDIST, 0.0, 1.0, 'Set a distance limit for bleeding.'),\
	('Bleed Strength:', PREF_STRENGTH, 0.01, 1.0, 'Bleed strength between adjacent verts weight. 1:full, 0:None'),\
	('Iterations', PREF_ITERATIONS, 1, 20, 'Number of times to run the blending calculation.'),\
	('Contract (Shrink)', PREF_MODE, 'Shrink instead of growing.'),\
	]
	
	if not Draw.PupBlock('Grow/Shrink...', pup_block):
		return
	
	actWeightNormalize(me, PREF_MODE.val, PREF_MAXDIST.val, PREF_STRENGTH.val, PREF_ITERATIONS.val)


if __name__=='__main__':
	main()