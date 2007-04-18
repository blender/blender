#!BPY

"""
Name: 'Armature Symmetry'
Blender: 242
Group: 'Armature'
Tooltip: 'Make an Armature symmetrical'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartist")
__version__ = "1.0 2006-7-26"

__doc__ = """\
This script creates perfectly symmetrical armatures,
based on the best fit when comparing the mirrored locations of 2 bones.
Hidden bones are ignored, and optionally only operate on selected bones.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton 2006
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
import bpy
Vector= Blender.Mathutils.Vector


def VecXFlip(vec):
	'''
	Return a copy of this vector x flipped.
	'''
	x,y,z= vec
	return Vector(-x,y,z)

def editbone_mirror_diff(editbone1, editbone2):
	'''
	X Mirror bone compare
	return a float representing the difference between the 2 bones
	the smaller the better the match
	'''
	h1= editbone1.head
	h2= editbone2.head
	
	t1= editbone1.tail
	t2= editbone2.tail
	
	# Mirror bone2's location
	h2= VecXFlip(h2)
	t2= VecXFlip(t2)
	
	#return (h1-h2).length + (t1-t2).length # returns the length only
	
	# For this function its easier to return the bones also
	return ((h1-h2).length + (t1-t2).length)/2, editbone1, editbone2

def editbone_mirror_merge(editbone1, editbone2, PREF_MODE_L2R, PREF_MODE_R2L):
	'''
	Merge these 2 bones to their mirrored locations
	'''
	h1= editbone1.head
	h2= editbone2.head
	
	t1= editbone1.tail
	t2= editbone2.tail
	
	if PREF_MODE_L2R and PREF_MODE_R2L:
		# Median, flip bone2's locations and average, then apply to editbone1, flip and apply to editbone2
		h2_f= VecXFlip(h2)
		t2_f= VecXFlip(t2)
		
		h_med= (h1+h2_f)*0.5 # middle between t1 and flipped t2
		t_med= (t1+t2_f)*0.5 # middle between h1 and flipped h2
		
		# Apply the median to editbone1
		editbone1.head= h_med
		editbone1.tail= t_med
		
		# Flip in place for editbone2
		h_med.x= -h_med.x
		t_med.x= -t_med.x
		
		# Apply the median to editbone2
		editbone2.head= h_med
		editbone2.tail= t_med
		
		# Average the roll, this might need some logical work, but looks good for now.
		r1= editbone1.roll
		r2= -editbone2.roll
		# print 'rolls are', r1,r2
		r_med= (r1+r2)/2
		# print 'new roll is', r_med
		editbone1.roll= r_med
		editbone2.roll= -r_med # mirror roll
	
	else: # Copy from 1 side to another
		
		# Crafty function we can use so L>R and R>L can use the same code
		def IS_XMIRROR_SOURCE(xval):
			'''Source means is this the value we want to copy from'''
			
			if PREF_MODE_L2R:
				if xval<0:	return True
				else:		return False
			else: # PREF_MODE_R2L
				if xval<0:	return False
				else:		return True
		
		if IS_XMIRROR_SOURCE( h1.x ):# head bone 1s negative, so copy it to h2
			editbone2.head= VecXFlip(h1)
		else:
			'''
			assume h2.x<0 - not a big deal if were wrong,
			its unlikely to ever happen because the bones would both be on the same side.
			'''
			
			# head bone 2s negative, so copy it to h1
			editbone1.head= VecXFlip(h2)
		
		# Same as above for tail
		if IS_XMIRROR_SOURCE(t1.x):
			editbone2.tail= VecXFlip(t1)
		else:
			editbone1.tail= VecXFlip(t2)
		
		# Copy roll from 1 bone to another, use the head's location to decide which side it's on.
		if IS_XMIRROR_SOURCE(editbone1.head):
			editbone2.roll= -editbone1.roll
		else:
			editbone1.roll= -editbone2.roll
		

def armature_symetry(\
	arm_ob,\
	PREF_MAX_DIST,\
	PREF_XMID_SNAP,\
	PREF_XZERO_THRESH,\
	PREF_MODE_L2R,\
	PREF_MODE_R2L,\
	PREF_SEL_ONLY):
	
	'''
	Main function that does all the work,
	return the number of 
	'''
	arm_data= arm_ob.data
	arm_data.makeEditable()
	
	# Get the bones
	bones= []
	HIDDEN_EDIT= Blender.Armature.HIDDEN_EDIT
	BONE_SELECTED= Blender.Armature.BONE_SELECTED
	
	if PREF_SEL_ONLY:
		for eb in arm_data.bones.values():
			options= eb.options
			if HIDDEN_EDIT not in options and BONE_SELECTED in options:
				bones.append(eb)
	else:
		# All non hidden bones
		for eb in arm_data.bones.values():
			options= eb.options
			if HIDDEN_EDIT not in options:
				bones.append(eb)
	
	del HIDDEN_EDIT # remove temp variables
	del BONE_SELECTED
	
	# Store the numder of bones we have modified for a message
	tot_editbones= len(bones)
	tot_editbones_modified= 0
	
	if PREF_XMID_SNAP:
		# Remove bones that are in the middle (X Zero)
		# reverse loop so we can remove items in the list.
		for eb_idx in xrange(len(bones)-1, -1, -1):
			edit_bone= bones[eb_idx]
			if abs(edit_bone.head.x) + abs(edit_bone.tail.x) <= PREF_XZERO_THRESH/2:
				
				# This is a center bone, clamp and remove from the bone list so we dont use again.
				if edit_bone.tail.x or edit_bone.head.x:
					tot_editbones_modified += 1
					
				edit_bone.tail.x= edit_bone.head.x= 0
				del bones[eb_idx]
				
				
	
	
	bone_comparisons= []
	
	# Compare every bone with every other bone, shouldn't be too slow.
	# These 2 "for" loops only compare once
	for eb_idx_a in xrange(len(bones)-1, -1, -1):
		edit_bone_a= bones[eb_idx_a]
		for eb_idx_b in xrange(eb_idx_a-1, -1, -1):
			edit_bone_b= bones[eb_idx_b]
			# Error float the first value from editbone_mirror_diff() so we can sort the resulting list.
			bone_comparisons.append(editbone_mirror_diff(edit_bone_a, edit_bone_b))
	
	
	bone_comparisons.sort() # best matches first
	
	# Make a dict() of bone names that have been used so we dont mirror more then once
	bone_mirrored= {}
	
	for error, editbone1, editbone2 in bone_comparisons:
		# print 'Trying to merge at error %.3f' % error
		if error > PREF_MAX_DIST:
			# print 'breaking, max error limit reached PREF_MAX_DIST: %.3f' % PREF_MAX_DIST
			break
		
		if not bone_mirrored.has_key(editbone1.name) and not bone_mirrored.has_key(editbone2.name):
			# Were not used, execute the mirror
			editbone_mirror_merge(editbone1, editbone2, PREF_MODE_L2R, PREF_MODE_R2L)
			# print 'Merging bones'
			
			# Add ourselves so we aren't touched again
			bone_mirrored[editbone1.name] = None # dummy value, would use sets in python 2.4
			bone_mirrored[editbone2.name] = None
			
			# If both options are enabled, then we have changed 2 bones
			tot_editbones_modified+= PREF_MODE_L2R + PREF_MODE_R2L
			
	arm_data.update() # get out of armature editmode
	return tot_editbones, tot_editbones_modified

	
def main():
	'''
	User interface function that gets the options and calls armature_symetry()
	'''
	
	scn= bpy.data.scenes.active
	arm_ob= scn.objects.active
	
	if not arm_ob or arm_ob.type!='Armature':
		Blender.Draw.PupMenu('No Armature object selected.')
		return
	
	# Cant be in editmode for armature.makeEditable()
	is_editmode= Blender.Window.EditMode()
	if is_editmode: Blender.Window.EditMode(0)
	Draw= Blender.Draw
	
	# Defaults for the user input
	PREF_XMID_SNAP= Draw.Create(1)
	PREF_MAX_DIST= Draw.Create(0.4)
	PREF_XZERO_THRESH= Draw.Create(0.02)
	
	PREF_MODE_L2R= Draw.Create(1)
	PREF_MODE_R2L= Draw.Create(0)
	PREF_SEL_ONLY= Draw.Create(1)
	
	pup_block = [\
	'Left (-), Right (+)',\
	('Left > Right', PREF_MODE_L2R, 'Copy from the Left to Right of the mesh. Enable Both for a mid loc.'),\
	('Right > Left', PREF_MODE_R2L, 'Copy from the Right to Left of the mesh. Enable Both for a mid loc.'),\
	'',\
	('MaxDist:', PREF_MAX_DIST, 0.0, 4.0, 'Maximum difference in mirror bones to match up pairs.'),\
	('XZero limit:', PREF_XZERO_THRESH, 0.0, 2.0, 'Tolerance for locking bones into the middle (X/zero).'),\
	('XMidSnap Bones', PREF_XMID_SNAP, 'Snap middle verts to X Zero (uses XZero limit)'),\
	('Selected Only', PREF_SEL_ONLY, 'Only xmirror selected bones.'),\
	]
	
	# Popup, exit if the user doesn't click OK
	if not Draw.PupBlock("X Mirror mesh tool", pup_block):
		return	
	
	# Replace the variables with their button values.
	PREF_XMID_SNAP= PREF_XMID_SNAP.val
	PREF_MAX_DIST= PREF_MAX_DIST.val
	PREF_MODE_L2R= PREF_MODE_L2R.val
	PREF_MODE_R2L= PREF_MODE_R2L.val
	PREF_XZERO_THRESH= PREF_XZERO_THRESH.val
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	
	# If both are off assume mid-point and enable both
	if not PREF_MODE_R2L and not PREF_MODE_L2R:
		PREF_MODE_R2L= PREF_MODE_L2R= True
	
	
	tot_editbones, tot_editbones_modified = armature_symetry(\
	  arm_ob,\
	  PREF_MAX_DIST,\
	  PREF_XMID_SNAP,\
	  PREF_XZERO_THRESH,\
	  PREF_MODE_L2R,\
	  PREF_MODE_R2L,\
	  PREF_SEL_ONLY)
	
	if is_editmode: Blender.Window.EditMode(1)
	
	# Redraw all views before popup
	Blender.Window.RedrawAll()
	
	# Print results
	if PREF_SEL_ONLY:
		msg= 'moved %i bones of %i selected' % (tot_editbones_modified, tot_editbones)
	else:
		msg= 'moved %i bones of %i visible' % (tot_editbones_modified, tot_editbones)
	
	
	Blender.Draw.PupMenu(msg)
	
# Check for __main__ so this function can be imported by other scripts without running the script.
if __name__=='__main__':
	main()
