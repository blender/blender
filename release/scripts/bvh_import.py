#!BPY

"""
Name: 'Motion Capture (.bvh)...'
Blender: 242
Group: 'Import'
Tip: 'Import a (.bvh) motion capture file'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.0.4 05/12/04"

__bpydoc__ = """\
This script imports BVH motion capture data to Blender.
as empties or armatures.
"""

# -------------------------------------------------------------------------- 
# BVH Import v2.0 by Campbell Barton (AKA Ideasman) 
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the 
# GNU General Public License for more details. 
# 
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software Foundation, 
# Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA. 
# 
# ***** END GPL LICENCE BLOCK ***** 
# -------------------------------------------------------------------------- 

import Blender
Vector= Blender.Mathutils.Vector
Euler= Blender.Mathutils.Euler
Matrix= Blender.Mathutils.Matrix
RotationMatrix = Blender.Mathutils.RotationMatrix
TranslationMatrix= Blender.Mathutils.TranslationMatrix

DEG2RAD = 0.017453292519943295
class bvh_node_class(object):
	__slots__=(\
	'name',# bvh joint name
	'parent',# bvh_node_class type or None for no parent
	'children',# ffd
	'rest_loc_world',# worldspace rest location for the head of this node
	'rest_loc_local',# localspace rest location for the head of this node
	'rest_tail_world',# # worldspace rest location for the tail of this node
	'rest_tail_local',# # worldspace rest location for the tail of this node
	'channels',# list of 6 ints, -1 for an unused channel, otherwise an index for the BVH motion data lines
	'anim_data',# a list one tuple's one for each frame. (locx, locy, locz, rotx, roty, rotz)
	'temp')# use this for whatever you want
	
	def __init__(self, name, rest_loc_world, rest_loc_local, parent, channels):
		self.name= name
		self.rest_loc_world= rest_loc_world
		self.rest_loc_local= rest_loc_local
		self.rest_tail_world= None
		self.rest_tail_local= None
		self.parent= parent
		self.channels= channels
		self.children= []
		
		# list of 6 length tuples: (lx,ly,lz, rx,ry,rz)
		# even if the channels arnt used they will just be zero
		# 
		self.anim_data= [(0,0,0,0,0,0)] 
		
	
	def __repr__(self):
		return 'BVH name:"%s", rest_loc:(%.3f,%.3f,%.3f), rest_tail:(%.3f,%.3f,%.3f)' %\
		(self.name,\
		self.rest_loc_world.x, self.rest_loc_world.y, self.rest_loc_world.z,\
		self.rest_loc_world.x, self.rest_loc_world.y, self.rest_loc_world.z)
	
# Change the order rotation is applied.
MATRIX_IDENTITY_3x3 = Matrix([1,0,0],[0,1,0],[0,0,1])
MATRIX_IDENTITY_4x4 = Matrix([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1])
def eulerRotate(x,y,z): 
	x,y,z = x%360,y%360,z%360 # Clamp all values between 0 and 360, values outside this raise an error.
	xmat = RotationMatrix(x,3,'x')
	ymat = RotationMatrix(y,3,'y')
	zmat = RotationMatrix(z,3,'z')
	# Standard BVH multiplication order, apply the rotation in the order Z,X,Y
	return (ymat*(xmat * (zmat * MATRIX_IDENTITY_3x3))).toEuler()


def read_bvh(file_path, GLOBAL_SCALE=1.0):
	# File loading stuff
	# Open the file for importing
	file = open(file_path, 'r')	
	
	# Seperate into a list of lists, each line a list of words.
	file_lines = file.readlines()
	# Non standard carrage returns?
	if len(file_lines) == 1:
		file_lines = file_lines[0].split('\r')
	
	# Split by whitespace.
	file_lines =[ll for ll in [ l.split() for l in file_lines] if ll]
	
	
	# Create Hirachy as empties
	
	if file_lines[0][0].lower() == 'hierarchy':
		#print 'Importing the BVH Hierarchy for:', file_path
		pass
	else:
		raise 'ERROR: This is not a BVH file'
	
	bvh_nodes= {None:None}
	bvh_nodes_serial = [None]
	
	channelIndex = -1
	

	lineIdx = 0 # An index for the file.
	while lineIdx < len(file_lines) -1:
		#...
		if file_lines[lineIdx][0].lower() == 'root' or file_lines[lineIdx][0].lower() == 'joint':
			
			# Join spaces into 1 word with underscores joining it.
			if len(file_lines[lineIdx]) > 2:
				file_lines[lineIdx][1] = '_'.join(file_lines[lineIdx][1:])
				file_lines[lineIdx] = file_lines[lineIdx][:2]
			
			# MAY NEED TO SUPPORT MULTIPLE ROOT's HERE!!!, Still unsure weather multiple roots are possible.??
			
			# Make sure the names are unique- Object names will match joint names exactly and both will be unique.
			name = file_lines[lineIdx][1]
			
			#print '%snode: %s, parent: %s' % (len(bvh_nodes_serial) * '  ', name,  bvh_nodes_serial[-1])
			
			lineIdx += 2 # Incriment to the next line (Offset)
			rest_loc_local = Vector( GLOBAL_SCALE*float(file_lines[lineIdx][1]), GLOBAL_SCALE*float(file_lines[lineIdx][2]), GLOBAL_SCALE*float(file_lines[lineIdx][3]) )
			lineIdx += 1 # Incriment to the next line (Channels)
			
			# newChannel[Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation]
			# newChannel references indecies to the motiondata,
			# if not assigned then -1 refers to the last value that will be added on loading at a value of zero, this is appended 
			# We'll add a zero value onto the end of the MotionDATA so this is always refers to a value.
			my_channel = [-1, -1, -1, -1, -1, -1] 
			for channel in file_lines[lineIdx][2:]:
				channel= channel.lower()
				channelIndex += 1 # So the index points to the right channel
				if   channel == 'xposition':	my_channel[0] = channelIndex
				elif channel == 'yposition':	my_channel[1] = channelIndex
				elif channel == 'zposition':	my_channel[2] = channelIndex
				elif channel == 'xrotation':	my_channel[3] = channelIndex
				elif channel == 'yrotation':	my_channel[4] = channelIndex
				elif channel == 'zrotation':	my_channel[5] = channelIndex
			
			channels = file_lines[lineIdx][2:]
			
			my_parent= bvh_nodes_serial[-1] # account for none
			
			
			# Apply the parents offset accumletivly
			if my_parent==None:
				rest_loc_world= Vector(rest_loc_local)
			else:
				rest_loc_world= my_parent.rest_loc_world + rest_loc_local
			
			bvh_node= bvh_nodes[name]= bvh_node_class(name, rest_loc_world, rest_loc_local, my_parent, my_channel)
			
			# If we have another child then we can call ourselves a parent, else 
			bvh_nodes_serial.append(bvh_node)

		# Account for an end node
		if file_lines[lineIdx][0].lower() == 'end' and file_lines[lineIdx][1].lower() == 'site': # There is somtimes a name after 'End Site' but we will ignore it.
			lineIdx += 2 # Incriment to the next line (Offset)
			rest_tail = Vector( GLOBAL_SCALE*float(file_lines[lineIdx][1]), GLOBAL_SCALE*float(file_lines[lineIdx][2]), GLOBAL_SCALE*float(file_lines[lineIdx][3]) )
			
			bvh_nodes_serial[-1].rest_tail_world= bvh_nodes_serial[-1].rest_loc_world + rest_tail
			bvh_nodes_serial[-1].rest_tail_local= rest_tail
			
			
			# Just so we can remove the Parents in a uniform way- End end never has kids
			# so this is a placeholder
			bvh_nodes_serial.append(None)
		
		if len(file_lines[lineIdx]) == 1 and file_lines[lineIdx][0] == '}': # == ['}']
			bvh_nodes_serial.pop() # Remove the last item
		
		if len(file_lines[lineIdx]) == 1 and file_lines[lineIdx][0].lower() == 'motion':
			#print '\nImporting motion data'
			lineIdx += 3 # Set the cursor to the first frame
			break
			
		lineIdx += 1
	
	
	# Remove the None value used for easy parent reference
	del bvh_nodes[None]
	# Dont use anymore
	del bvh_nodes_serial
	
	bvh_nodes_list= bvh_nodes.values()
	
	while lineIdx < len(file_lines) -1:
		line= file_lines[lineIdx]
		for bvh_node in bvh_nodes_list:
			#for bvh_node in bvh_nodes_serial:
			lx= ly= lz= rx= ry= rz= 0.0
			channels= bvh_node.channels
			anim_data= bvh_node.anim_data
			if channels[0] != -1:
				lx= GLOBAL_SCALE * float(  line[channels[0]] )
				
			if channels[1] != -1:
				ly= GLOBAL_SCALE * float(  line[channels[1]] )
			
			if channels[2] != -1:
				lz= GLOBAL_SCALE * float(  line[channels[2]] )
			
			if channels[3] != -1 or channels[4] != -1 or channels[5] != -1:						
				rx, ry, rz = eulerRotate(float( line[channels[3]] ), float( line[channels[4]] ), float( line[channels[5]] ))
				#x,y,z = x/10.0, y/10.0, z/10.0 # For IPO's 36 is 360d
				
				# Make interpolation not cross between 180d, thjis fixes sub frame interpolation and time scaling.
				# Will go from (355d to 365d) rather then to (355d to 5d) - inbetween these 2 there will now be a correct interpolation.
				
				while anim_data[-1][3] - rx >  180: rx+=360
				while anim_data[-1][3] - rx < -180: rx-=360
				
				while anim_data[-1][4] - ry >  180: ry+=360
				while anim_data[-1][4] - ry < -180: ry-=360
				
				while anim_data[-1][5] - rz >  180: rz+=360
				while anim_data[-1][5] - rz < -180: rz-=360
			
			# Done importing motion data #
			anim_data.append( (lx, ly, lz, rx, ry, rz) )
		lineIdx += 1
	
	# Assign children
	for bvh_node in bvh_nodes.itervalues():		
		bvh_node_parent= bvh_node.parent
		if bvh_node_parent:
			bvh_node_parent.children.append(bvh_node)
	
	# Now set the tip of each bvh_node
	for bvh_node in bvh_nodes.itervalues():
		
		if not bvh_node.rest_tail_world:
			if len(bvh_node.children)==1:
				bvh_node.rest_tail_world= Vector(bvh_node.children[0].rest_loc_world)
				bvh_node.rest_tail_local= Vector(bvh_node.children[0].rest_loc_local)
			else:
				if not bvh_node.children:
					raise 'error, bvh node has no end and no children. bad file'
					
				# Removed temp for now
				rest_tail_world= Vector(0,0,0)
				rest_tail_local= Vector(0,0,0)
				for bvh_node_child in bvh_node.children:
					rest_tail_world += bvh_node_child.rest_loc_world
					rest_tail_local += bvh_node_child.rest_loc_local
				
				bvh_node.rest_tail_world= rest_tail_world * (1.0/len(bvh_node.children))
				bvh_node.rest_tail_local= rest_tail_local * (1.0/len(bvh_node.children))
				
				# Zero area fix
				if (bvh_node.rest_tail_world-bvh_node.rest_tail_local).length < 0.0001:
				
					# New operation for temp fix of location setting.
					# Get the average length from my head to choldrens heads and make a
					# Z up bone from that length
					length= 0.0
					for bvh_node_child in bvh_node.children:
						length+= (bvh_node.rest_loc_world - bvh_node_child.rest_loc_world).length
					length= length/len(bvh_node.children)
					
					
					bvh_node.rest_tail_local= Vector(bvh_node.rest_loc_local)
					bvh_node.rest_tail_world= Vector(bvh_node.rest_loc_world)
					bvh_node.rest_tail_world.y = bvh_node.rest_tail_world.y + length
					bvh_node.rest_tail_local.y = bvh_node.rest_tail_local.y + length
					# END TEMP REPLACE
					

		# Make sure tail isnt the same location as the head.
		if (bvh_node.rest_tail_local-bvh_node.rest_loc_local).length <= 0.001*GLOBAL_SCALE:
			
			bvh_node.rest_tail_local.y= bvh_node.rest_tail_local.y + GLOBAL_SCALE/10
			bvh_node.rest_tail_world.y= bvh_node.rest_tail_world.y + GLOBAL_SCALE/10
			
		
		
	return bvh_nodes



def bvh_node_dict2objects(bvh_nodes, IMPORT_START_FRAME= 1):
	
	if IMPORT_START_FRAME<1:
		IMPORT_START_FRAME= 1
		
	scn= Blender.Scene.GetCurrent()
	for ob in scn.getChildren():
		ob.sel= 0
	
	objects= []
	
	def add_ob(name):
		ob= Blender.Object.New('Empty', name)
		scn.link(ob)
		ob.sel= 1
		ob.Layers= scn.Layers
		objects.append(ob)
		return ob
	
	# Add objects
	for name, bvh_node in bvh_nodes.iteritems():
		bvh_node.temp= add_ob(name)
	
	# Parent the objects
	for bvh_node in bvh_nodes.itervalues():
		bvh_node.temp.makeParent([ bvh_node_child.temp for bvh_node_child in bvh_node.children ], 1, 0) # ojbs, noninverse, 1 = not fast.
	
	# Offset
	for bvh_node in bvh_nodes.itervalues():
		# Make relative to parents offset
		bvh_node.temp.loc= bvh_node.rest_loc_local
	
	# Add tail objects
	for name, bvh_node in bvh_nodes.iteritems():
		if not bvh_node.children:
			ob_end= add_ob(name + '_end')
			bvh_node.temp.makeParent([ob_end], 1, 0) # ojbs, noninverse, 1 = not fast.
			ob_end.loc= bvh_node.rest_tail_local
	
	
	# Animate the data, the last used bvh_node will do since they all have the same number of frames
	for current_frame in xrange(len(bvh_node.anim_data)):
		Blender.Set('curframe', current_frame+IMPORT_START_FRAME)
		
		for bvh_node in bvh_nodes.itervalues():
			lx,ly,lz,rx,ry,rz= bvh_node.anim_data[current_frame]
			
			rest_loc_local= bvh_node.rest_loc_local
			bvh_node.temp.loc= rest_loc_local.x+lx, rest_loc_local.y+ly, rest_loc_local.z+lz
			
			bvh_node.temp.rot= rx*DEG2RAD,ry*DEG2RAD,rz*DEG2RAD
			
			bvh_node.temp.insertIpoKey(Blender.Object.LOCROT)
	
	scn.update(1)
	return objects
	


#TODO, armature loading
def bvh_node_dict2armature(bvh_nodes, IMPORT_START_FRAME= 1):
	
	if IMPORT_START_FRAME<1:
		IMPORT_START_FRAME= 1
		
	
	# Add the new armature, 
	arm_ob= Blender.Object.New('Armature')
	arm_data= Blender.Armature.Armature('myArmature')
	arm_ob.link(arm_data)
	
	# Put us into editmode
	arm_data.makeEditable()
	
	
	for name, bvh_node in bvh_nodes.iteritems():
		bone= bvh_node.temp= Blender.Armature.Editbone()
		bone.name= name
		arm_data.bones[name]= bone
		
		bone.head= bvh_node.rest_loc_world
		bone.tail= bvh_node.rest_tail_world
	
	for bvh_node in bvh_nodes.itervalues():
		if bvh_node.parent:
			# Set the bone parent
			bvh_node.temp.parent= bvh_node.parent.temp
	
	
	# Replace the editbone with the editbone name,
	# to avoid memory errors accessing the editbone outside editmode
	for bvh_node in bvh_nodes.itervalues():
		bvh_node.temp= bvh_node.temp.name
		
	arm_data.update()
	
	scn= Blender.Scene.GetCurrent()
	
	for ob in scn.getChildren():
		ob.sel= 0
	
	scn.link(arm_ob)
	arm_ob.sel= 1
	arm_ob.Layers= scn.Layers
	
	
	
	# Now Apply the animation to the armature
	
	# Get armature animation data
	pose= arm_ob.getPose()
	pose_bones= pose.bones
	
	action = Blender.Armature.NLA.NewAction("Action") 
	action.setActive(arm_ob)
	xformConstants= [ Blender.Object.Pose.LOC, Blender.Object.Pose.ROT ]
	
	# Replace the bvh_node.temp (currently an editbone)
	# With a tuple  (pose_bone, armature_bone, bone_rest_matrix, bone_rest_matrix_inv)
	for bvh_node in bvh_nodes.itervalues():
		bone_name= bvh_node.temp # may not be the same name as the bvh_node, could have been shortened.
		pose_bone= pose_bones[bone_name]
		rest_bone= arm_data.bones[bone_name]
		bone_rest_matrix = rest_bone.matrix['ARMATURESPACE'].rotationPart()
		
		bone_rest_matrix_inv= Matrix(bone_rest_matrix)
		bone_rest_matrix_inv.invert()
		
		bone_rest_matrix_inv.resize4x4()
		bone_rest_matrix.resize4x4()
		bvh_node.temp= (pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv)
	
	
	# Animate the data, the last used bvh_node will do since they all have the same number of frames
	for current_frame in xrange(len(bvh_node.anim_data)):
		
		# Dont neet to set the current frame
		for bvh_node in bvh_nodes.itervalues():
			pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv= bvh_node.temp
			lx,ly,lz,rx,ry,rz= bvh_node.anim_data[current_frame]
			
			# Set the rotation, not so simple			
			bone_rotation_matrix= Euler(rx,ry,rz).toMatrix()
			bone_rotation_matrix.resize4x4()
			pose_bone.quat= (bone_rest_matrix * bone_rotation_matrix * bone_rest_matrix_inv).toQuat()
			
			# Set the Location, simple too
			pose_bone.loc= (\
			TranslationMatrix(Vector(lx*10, ly*10, lz*10)) *\
			bone_rest_matrix_inv).translationPart() # WHY * 10? - just how pose works
			
			# Insert the keyframe from the loc/quat
			pose_bone.insertKey(arm_ob, current_frame+IMPORT_START_FRAME, xformConstants)
			
			
	pose.update()
	return arm_ob


#=============#
# TESTING     #
#=============#

#('/metavr/mocap/bvh/boxer.bvh')
#('/metavr/mocap/bvh/dg-306-g.bvh') # Incompleate EOF
#('/metavr/mocap/bvh/wa8lk.bvh') # duplicate joint names, \r line endings.
#('/metavr/mocap/bvh/walk4.bvh') # 0 channels
"""
import os
DIR = '/metavr/mocap/bvh/'
#for f in os.listdir(DIR)[5:6]:
for f in os.listdir(DIR):
	if f.endswith('.bvh'):
		s = Blender.Scene.New(f)
		s.makeCurrent()
		file= DIR + f
		print f
		bvh_nodes= read_bvh(file, 1.0)
		bvh_node_dict2armature(bvh_nodes, 1)
		bvh_node_dict2objects(bvh_nodes,  1)
"""

def load_bvh_ui(file):
	Draw= Blender.Draw
	
	IMPORT_SCALE = Draw.Create(0.01)
	IMPORT_START_FRAME = Draw.Create(1)
	IMPORT_AS_ARMATURE = Draw.Create(1)
	IMPORT_AS_EMPTIES = Draw.Create(0)
	
	
	# Get USER Options
	pup_block = [\
	('As Armature', IMPORT_AS_ARMATURE, 'Imports the BVH as an armature'),\
	('As Empties', IMPORT_AS_EMPTIES, 'Imports the BVH as empties'),\
	('Scale: ', IMPORT_SCALE, 0.001, 100.0, 'Scale the BVH, Use 0.01 when 1.0 is 1 metre'),\
	('Start Frame: ', IMPORT_START_FRAME, 1, 30000, 'Frame to start BVH motion'),\
	]
	
	if not Draw.PupBlock('BVH Import...', pup_block):
		return
	
	print 'Attempting import BVH', file
	
	IMPORT_SCALE = IMPORT_SCALE.val
	IMPORT_START_FRAME = IMPORT_START_FRAME.val
	IMPORT_AS_ARMATURE = IMPORT_AS_ARMATURE.val
	IMPORT_AS_EMPTIES = IMPORT_AS_EMPTIES.val
	
	if not IMPORT_AS_ARMATURE and not IMPORT_AS_EMPTIES:
		Blender.Draw.PupMenu('No import option selected')
		return
	Blender.Window.WaitCursor(1)
	# Get the BVH data and act on it.
	bvh_nodes= read_bvh(file, IMPORT_SCALE)
	if IMPORT_AS_ARMATURE:	bvh_node_dict2armature(bvh_nodes, IMPORT_START_FRAME)
	if IMPORT_AS_EMPTIES:	bvh_node_dict2objects(bvh_nodes,  IMPORT_START_FRAME)
	Blender.Window.WaitCursor(0)


def main():
	Blender.Window.FileSelector(load_bvh_ui, 'Import BVH', '*.bvh')
	
if __name__ == '__main__':
	main()
