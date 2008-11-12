# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# Version History:
#   1.0 original release bakes an armature into a matrix
#   1.1 optional params (ACTION_BAKE, ACTION_BAKE_FIRST_FRAME, direct function to key and return the Action

import Blender
from Blender import sys
import bpy
def getBakedPoseData(ob_arm, start_frame, end_frame, ACTION_BAKE = False, ACTION_BAKE_FIRST_FRAME = True):
	'''
	If you are currently getting IPO's this function can be used to
	ACTION_BAKE==False: return a list of frame aligned bone dictionary's
	ACTION_BAKE==True: return an action with keys aligned to bone constrained movement
	if ACTION_BAKE_FIRST_FRAME is not supplied or is true: keys begin at frame 1
	
	The data in these can be swaped in for the IPO loc and quat
	
	If you want to bake an action, this is not as hard and the ipo hack can be removed.
	'''
	
	# --------------------------------- Dummy Action! Only for this functon
	backup_action = ob_arm.action
	backup_frame = Blender.Get('curframe')
	
	DUMMY_ACTION_NAME = '~DONT_USE~'
	# Get the dummy action if it has no users
	try:
		new_action = bpy.data.actions[DUMMY_ACTION_NAME]
		if new_action.users:
			new_action = None
	except:
		new_action = None
	
	if not new_action:
		new_action = bpy.data.actions.new(DUMMY_ACTION_NAME)
		new_action.fakeUser = False
	# ---------------------------------- Done
	
	Matrix = Blender.Mathutils.Matrix
	Quaternion = Blender.Mathutils.Quaternion
	Vector = Blender.Mathutils.Vector
	POSE_XFORM= [Blender.Object.Pose.LOC, Blender.Object.Pose.ROT]
	
	# Each dict a frame
	bake_data = [{} for i in xrange(1+end_frame-start_frame)]
	
	pose=			ob_arm.getPose()
	armature_data=	ob_arm.getData();
	pose_bones=		pose.bones
	
	# --------------------------------- Build a list of arma data for reuse
	armature_bone_data = []
	bones_index = {}
	for bone_name, rest_bone in armature_data.bones.items():
		pose_bone = pose_bones[bone_name]
		rest_matrix = rest_bone.matrix['ARMATURESPACE']
		rest_matrix_inv = rest_matrix.copy().invert()
		armature_bone_data.append( [len(bones_index), -1, bone_name, rest_bone, rest_matrix, rest_matrix_inv, pose_bone, None ])
		bones_index[bone_name] = len(bones_index)
	
	# Set the parent ID's
	for bone_name, pose_bone in pose_bones.items():
		parent = pose_bone.parent
		if parent:
			bone_index= bones_index[bone_name]
			parent_index= bones_index[parent.name]
			armature_bone_data[ bone_index ][1]= parent_index
	# ---------------------------------- Done
	
	
	
	# --------------------------------- Main loop to collect IPO data
	frame_index = 0
	NvideoFrames= end_frame-start_frame
	for current_frame in xrange(start_frame, end_frame+1):
		if   frame_index==0: start=sys.time()
		elif frame_index==15: print NvideoFrames*(sys.time()-start),"seconds estimated..." #slows as it grows *3
		elif frame_index >15:
			percom= frame_index*100/NvideoFrames
			print "Frame %i Overall %i percent complete\r" % (current_frame, percom),
		ob_arm.action = backup_action
		#pose.update() # not needed
		Blender.Set('curframe', current_frame)
		#Blender.Window.RedrawAll()
		#frame_data = bake_data[frame_index]
		ob_arm.action = new_action
		###for i,pose_bone in enumerate(pose_bones):
		
		for index, parent_index, bone_name, rest_bone, rest_matrix, rest_matrix_inv, pose_bone, ipo in armature_bone_data:
			matrix= pose_bone.poseMatrix
			parent_bone= rest_bone.parent
			if parent_index != -1:
				parent_pose_matrix =		armature_bone_data[parent_index][6].poseMatrix
				parent_bone_matrix_inv =	armature_bone_data[parent_index][5]
				matrix=						matrix * parent_pose_matrix.copy().invert()
				rest_matrix=				rest_matrix * parent_bone_matrix_inv
			
			matrix=matrix * rest_matrix.copy().invert()
			pose_bone.quat=	matrix.toQuat()
			pose_bone.loc=	matrix.translationPart()
			if ACTION_BAKE==False:
				pose_bone.insertKey(ob_arm, 1, POSE_XFORM) # always frame 1
	 
				# THIS IS A BAD HACK! IT SUCKS BIGTIME BUT THE RESULT ARE NICE
				# - use a temp action and bake into that, always at the same frame
				#   so as not to make big IPO's, then collect the result from the IPOs
			
				# Now get the data from the IPOs
				if not ipo:	ipo = armature_bone_data[index][7] = new_action.getChannelIpo(bone_name)
			
				loc = Vector()
				quat  = Quaternion()
			
				for curve in ipo:
					val = curve.evaluate(1)
					curve_name= curve.name
					if   curve_name == 'LocX':  loc[0] = val
					elif curve_name == 'LocY':  loc[1] = val
					elif curve_name == 'LocZ':  loc[2] = val
					elif curve_name == 'QuatW': quat[3]  = val
					elif curve_name == 'QuatX': quat[0]  = val
					elif curve_name == 'QuatY': quat[1]  = val
					elif curve_name == 'QuatZ': quat[2]  = val
			
				bake_data[frame_index][bone_name] = loc, quat
			else:
				if ACTION_BAKE_FIRST_FRAME: pose_bone.insertKey(ob_arm, frame_index+1,  POSE_XFORM)
				else:           pose_bone.insertKey(ob_arm, current_frame , POSE_XFORM)
		frame_index+=1
	print "\nBaking Complete."
	ob_arm.action = backup_action
	if ACTION_BAKE==False:
		Blender.Set('curframe', backup_frame)
		return bake_data
	elif ACTION_BAKE==True:
		return new_action
	else: print "ERROR: Invalid ACTION_BAKE %i sent to BPyArmature" % ACTION_BAKE



