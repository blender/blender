import math

# import Blender
import bpy
# import BPyMessages
import Mathutils
Vector= Mathutils.Vector
Euler= Mathutils.Euler
Matrix= Mathutils.Matrix
RotationMatrix= Mathutils.RotationMatrix
TranslationMatrix= Mathutils.TranslationMatrix

# NASTY GLOBAL
ROT_STYLE = 'QUAT'

DEG2RAD = 0.017453292519943295

class bvh_node_class(object):
	__slots__=(\
	'name',# bvh joint name
	'parent',# bvh_node_class type or None for no parent
	'children',# a list of children of this type.
	'rest_head_world',# worldspace rest location for the head of this node
	'rest_head_local',# localspace rest location for the head of this node
	'rest_tail_world',# # worldspace rest location for the tail of this node
	'rest_tail_local',# # worldspace rest location for the tail of this node
	'channels',# list of 6 ints, -1 for an unused channel, otherwise an index for the BVH motion data lines, lock triple then rot triple
	'rot_order',# a triple of indicies as to the order rotation is applied. [0,1,2] is x/y/z - [None, None, None] if no rotation.
	'anim_data',# a list one tuple's one for each frame. (locx, locy, locz, rotx, roty, rotz)
	'has_loc',# Conveinience function, bool, same as (channels[0]!=-1 or channels[1]!=-1 channels[2]!=-1)
	'has_rot',# Conveinience function, bool, same as (channels[3]!=-1 or channels[4]!=-1 channels[5]!=-1)
	'temp')# use this for whatever you want
	
	def __init__(self, name, rest_head_world, rest_head_local, parent, channels, rot_order):
		self.name= name
		self.rest_head_world= rest_head_world
		self.rest_head_local= rest_head_local
		self.rest_tail_world= None
		self.rest_tail_local= None
		self.parent= parent
		self.channels= channels
		self.rot_order= rot_order
		
		# convenience functions
		self.has_loc= channels[0] != -1 or channels[1] != -1 or channels[2] != -1
		self.has_rot= channels[3] != -1 or channels[4] != -1 or channels[5] != -1
		
		
		self.children= []
		
		# list of 6 length tuples: (lx,ly,lz, rx,ry,rz)
		# even if the channels arnt used they will just be zero
		# 
		self.anim_data= [(0,0,0,0,0,0)] 
		
	
	def __repr__(self):
		return 'BVH name:"%s", rest_loc:(%.3f,%.3f,%.3f), rest_tail:(%.3f,%.3f,%.3f)' %\
		(self.name,\
		self.rest_head_world.x, self.rest_head_world.y, self.rest_head_world.z,\
		self.rest_head_world.x, self.rest_head_world.y, self.rest_head_world.z)
	


# Change the order rotation is applied.
MATRIX_IDENTITY_3x3 = Matrix([1,0,0],[0,1,0],[0,0,1])
MATRIX_IDENTITY_4x4 = Matrix([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1])

def eulerRotate(x,y,z, rot_order): 
	
	# Clamp all values between 0 and 360, values outside this raise an error.
	mats=[RotationMatrix(math.radians(x%360),3,'x'), RotationMatrix(math.radians(y%360),3,'y'), RotationMatrix(math.radians(z%360),3,'z')]
	# print rot_order
	# Standard BVH multiplication order, apply the rotation in the order Z,X,Y
	
	#XXX, order changes???
	#eul = (mats[rot_order[2]]*(mats[rot_order[1]]* (mats[rot_order[0]]* MATRIX_IDENTITY_3x3))).toEuler()	
	eul = (MATRIX_IDENTITY_3x3*mats[rot_order[0]]*(mats[rot_order[1]]* (mats[rot_order[2]]))).toEuler()	
	
	eul = math.degrees(eul.x), math.degrees(eul.y), math.degrees(eul.z) 
	
	return eul

def read_bvh(context, file_path, GLOBAL_SCALE=1.0):
	# File loading stuff
	# Open the file for importing
	file = open(file_path, 'rU')	
	
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
			rest_head_local = Vector( GLOBAL_SCALE*float(file_lines[lineIdx][1]), GLOBAL_SCALE*float(file_lines[lineIdx][2]), GLOBAL_SCALE*float(file_lines[lineIdx][3]) )
			lineIdx += 1 # Incriment to the next line (Channels)
			
			# newChannel[Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation]
			# newChannel references indecies to the motiondata,
			# if not assigned then -1 refers to the last value that will be added on loading at a value of zero, this is appended 
			# We'll add a zero value onto the end of the MotionDATA so this is always refers to a value.
			my_channel = [-1, -1, -1, -1, -1, -1] 
			my_rot_order= [None, None, None]
			rot_count= 0
			for channel in file_lines[lineIdx][2:]:
				channel= channel.lower()
				channelIndex += 1 # So the index points to the right channel
				if   channel == 'xposition':	my_channel[0] = channelIndex
				elif channel == 'yposition':	my_channel[1] = channelIndex
				elif channel == 'zposition':	my_channel[2] = channelIndex
				
				elif channel == 'xrotation':
					my_channel[3] = channelIndex
					my_rot_order[rot_count]= 0
					rot_count+=1
				elif channel == 'yrotation':
					my_channel[4] = channelIndex
					my_rot_order[rot_count]= 1
					rot_count+=1
				elif channel == 'zrotation':
					my_channel[5] = channelIndex
					my_rot_order[rot_count]= 2
					rot_count+=1
			
			channels = file_lines[lineIdx][2:]
			
			my_parent= bvh_nodes_serial[-1] # account for none
			
			
			# Apply the parents offset accumletivly
			if my_parent==None:
				rest_head_world= Vector(rest_head_local)
			else:
				rest_head_world= my_parent.rest_head_world + rest_head_local
			
			bvh_node= bvh_nodes[name]= bvh_node_class(name, rest_head_world, rest_head_local, my_parent, my_channel, my_rot_order)
			
			# If we have another child then we can call ourselves a parent, else 
			bvh_nodes_serial.append(bvh_node)

		# Account for an end node
		if file_lines[lineIdx][0].lower() == 'end' and file_lines[lineIdx][1].lower() == 'site': # There is somtimes a name after 'End Site' but we will ignore it.
			lineIdx += 2 # Incriment to the next line (Offset)
			rest_tail = Vector( GLOBAL_SCALE*float(file_lines[lineIdx][1]), GLOBAL_SCALE*float(file_lines[lineIdx][2]), GLOBAL_SCALE*float(file_lines[lineIdx][3]) )
			
			bvh_nodes_serial[-1].rest_tail_world= bvh_nodes_serial[-1].rest_head_world + rest_tail
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
	
	while lineIdx < len(file_lines):
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
				rx, ry, rz = float( line[channels[3]] ), float( line[channels[4]] ), float( line[channels[5]] )
				
				if ROT_STYLE != 'NATIVE':
					rx, ry, rz = eulerRotate(rx, ry, rz, bvh_node.rot_order)
				
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
	for bvh_node in bvh_nodes.values():		
		bvh_node_parent= bvh_node.parent
		if bvh_node_parent:
			bvh_node_parent.children.append(bvh_node)
	
	# Now set the tip of each bvh_node
	for bvh_node in bvh_nodes.values():
		
		if not bvh_node.rest_tail_world:
			if len(bvh_node.children)==0:
				# could just fail here, but rare BVH files have childless nodes
				bvh_node.rest_tail_world = Vector(bvh_node.rest_head_world)
				bvh_node.rest_tail_local = Vector(bvh_node.rest_head_local)
			elif len(bvh_node.children)==1:
				bvh_node.rest_tail_world= Vector(bvh_node.children[0].rest_head_world)
				bvh_node.rest_tail_local= Vector(bvh_node.children[0].rest_head_local)
			else:
				# allow this, see above
				#if not bvh_node.children:
				#	raise 'error, bvh node has no end and no children. bad file'
					
				# Removed temp for now
				rest_tail_world= Vector(0,0,0)
				rest_tail_local= Vector(0,0,0)
				for bvh_node_child in bvh_node.children:
					rest_tail_world += bvh_node_child.rest_head_world
					rest_tail_local += bvh_node_child.rest_head_local
				
				bvh_node.rest_tail_world= rest_tail_world * (1.0/len(bvh_node.children))
				bvh_node.rest_tail_local= rest_tail_local * (1.0/len(bvh_node.children))

		# Make sure tail isnt the same location as the head.
		if (bvh_node.rest_tail_local-bvh_node.rest_head_local).length <= 0.001*GLOBAL_SCALE:
			
			bvh_node.rest_tail_local.y= bvh_node.rest_tail_local.y + GLOBAL_SCALE/10
			bvh_node.rest_tail_world.y= bvh_node.rest_tail_world.y + GLOBAL_SCALE/10
			
		
		
	return bvh_nodes



def bvh_node_dict2objects(context, bvh_nodes, IMPORT_START_FRAME= 1, IMPORT_LOOP= False):
	
	if IMPORT_START_FRAME<1:
		IMPORT_START_FRAME= 1
		
	scn= context.scene
	scn.objects.selected = []
	
	objects= []
	
	def add_ob(name):
		ob = scn.objects.new('Empty')
		objects.append(ob)
		return ob
	
	# Add objects
	for name, bvh_node in bvh_nodes.items():
		bvh_node.temp= add_ob(name)
	
	# Parent the objects
	for bvh_node in bvh_nodes.values():
		bvh_node.temp.makeParent([ bvh_node_child.temp for bvh_node_child in bvh_node.children ], 1, 0) # ojbs, noninverse, 1 = not fast.
	
	# Offset
	for bvh_node in bvh_nodes.values():
		# Make relative to parents offset
		bvh_node.temp.loc= bvh_node.rest_head_local
	
	# Add tail objects
	for name, bvh_node in bvh_nodes.items():
		if not bvh_node.children:
			ob_end= add_ob(name + '_end')
			bvh_node.temp.makeParent([ob_end], 1, 0) # ojbs, noninverse, 1 = not fast.
			ob_end.loc= bvh_node.rest_tail_local
	
	
	# Animate the data, the last used bvh_node will do since they all have the same number of frames
	for current_frame in range(len(bvh_node.anim_data)):
		Blender.Set('curframe', current_frame+IMPORT_START_FRAME)
		
		for bvh_node in bvh_nodes.values():
			lx,ly,lz,rx,ry,rz= bvh_node.anim_data[current_frame]
			
			rest_head_local= bvh_node.rest_head_local
			bvh_node.temp.loc= rest_head_local.x+lx, rest_head_local.y+ly, rest_head_local.z+lz
			
			bvh_node.temp.rot= rx*DEG2RAD,ry*DEG2RAD,rz*DEG2RAD
			
			bvh_node.temp.insertIpoKey(Blender.Object.IpoKeyTypes.LOCROT) # XXX invalid
	
	scn.update(1)
	return objects



def bvh_node_dict2armature(context, bvh_nodes, IMPORT_START_FRAME= 1, IMPORT_LOOP= False):
	
	if IMPORT_START_FRAME<1:
		IMPORT_START_FRAME= 1
		
	
	# Add the new armature, 
	scn = context.scene
#XXX	scn.objects.selected = []
	for ob in scn.objects:
		ob.selected = False
	
	
#XXX	arm_data= bpy.data.armatures.new()
#XXX	arm_ob = scn.objects.new(arm_data)
	bpy.ops.object.armature_add()
	arm_ob= scn.objects[-1]
	arm_data= arm_ob.data

	
	
	
#XXX	scn.objects.context = [arm_ob]
#XXX	scn.objects.active = arm_ob
	arm_ob.selected= True
	scn.objects.active= arm_ob
	print(scn.objects.active)
	
	
	# Put us into editmode
#XXX	arm_data.makeEditable()
	
	bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
	bpy.ops.object.mode_set(mode='EDIT', toggle=False)

	
	
	# Get the average bone length for zero length bones, we may not use this.
	average_bone_length= 0.0
	nonzero_count= 0
	for bvh_node in bvh_nodes.values():
		l= (bvh_node.rest_head_local-bvh_node.rest_tail_local).length
		if l:
			average_bone_length+= l
			nonzero_count+=1
	
	# Very rare cases all bones couldbe zero length???
	if not average_bone_length:
		average_bone_length = 0.1
	else:
		# Normal operation
		average_bone_length = average_bone_length/nonzero_count
	
	
#XXX - sloppy operator code
	
	bpy.ops.armature.delete()
	bpy.ops.armature.select_all()
	bpy.ops.armature.delete()

	ZERO_AREA_BONES= []
	for name, bvh_node in bvh_nodes.items():
		# New editbone
		bpy.ops.armature.bone_primitive_add(name="Bone")
		
#XXX		bone= bvh_node.temp= Blender.Armature.Editbone()
		bone= bvh_node.temp= arm_data.edit_bones[-1]

		bone.name= name
#		arm_data.bones[name]= bone
		
		bone.head= bvh_node.rest_head_world
		bone.tail= bvh_node.rest_tail_world
		
		# ZERO AREA BONES.
		if (bone.head-bone.tail).length < 0.001:
			if bvh_node.parent:
				ofs= bvh_node.parent.rest_head_local- bvh_node.parent.rest_tail_local
				if ofs.length: # is our parent zero length also?? unlikely
					bone.tail= bone.tail+ofs
				else:
					bone.tail.y= bone.tail.y+average_bone_length
			else:
				bone.tail.y= bone.tail.y+average_bone_length
			
			ZERO_AREA_BONES.append(bone.name)
	
	
	for bvh_node in bvh_nodes.values():
		if bvh_node.parent:
			# bvh_node.temp is the Editbone
			
			# Set the bone parent
			bvh_node.temp.parent= bvh_node.parent.temp
			
			# Set the connection state
			if not bvh_node.has_loc and\
			bvh_node.parent and\
			bvh_node.parent.temp.name not in ZERO_AREA_BONES and\
			bvh_node.parent.rest_tail_local == bvh_node.rest_head_local:
				bvh_node.temp.connected= True
	
	# Replace the editbone with the editbone name,
	# to avoid memory errors accessing the editbone outside editmode
	for bvh_node in bvh_nodes.values():
		bvh_node.temp= bvh_node.temp.name
	
#XXX	arm_data.update()
	
	# Now Apply the animation to the armature
	
	# Get armature animation data
	bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
	bpy.ops.object.mode_set(mode='POSE', toggle=False)
	
	pose= arm_ob.pose
	pose_bones= pose.bones
	
	
	if ROT_STYLE=='NATIVE':
		eul_order_lookup = {\
			(0,1,2):'XYZ',
			(0,2,1):'XZY',
			(1,0,2):'YXZ',
			(1,2,0):'YZX',
			(2,0,1):'ZXY',
			(2,1,0):'ZYZ'
		}
		
		for bvh_node in bvh_nodes.values():
			bone_name= bvh_node.temp # may not be the same name as the bvh_node, could have been shortened.
			pose_bone= pose_bones[bone_name]
			pose_bone.rotation_mode  = eul_order_lookup[tuple(bvh_node.rot_order)]
		
	elif ROT_STYLE=='XYZ':
		for pose_bone in pose_bones:
			pose_bone.rotation_mode  = 'XYZ'
	else:
		# Quats default
		pass 
	
	
	bpy.ops.pose.select_all() # set
	bpy.ops.anim.keyframe_insert_menu(type=-4) # XXX -     -4 ???
	

	
	
	
	#for p in pose_bones:
	#	print(p)
	
	
#XXX	action = Blender.Armature.NLA.NewAction("Action") 
#XXX	action.setActive(arm_ob)
	
	#bpy.ops.action.new()
	#action = bpy.data.actions[-1]
	
	# arm_ob.animation_data.action = action
	action = arm_ob.animation_data.action
	
	
	
	
	#xformConstants= [ Blender.Object.Pose.LOC, Blender.Object.Pose.ROT ]
	
	# Replace the bvh_node.temp (currently an editbone)
	# With a tuple  (pose_bone, armature_bone, bone_rest_matrix, bone_rest_matrix_inv)
	for bvh_node in bvh_nodes.values():
		bone_name= bvh_node.temp # may not be the same name as the bvh_node, could have been shortened.
		pose_bone= pose_bones[bone_name]
		rest_bone= arm_data.bones[bone_name]
#XXX		bone_rest_matrix = rest_bone.matrix['ARMATURESPACE'].rotationPart()
		bone_rest_matrix = rest_bone.matrix.rotationPart()
		
		
		bone_rest_matrix_inv= Matrix(bone_rest_matrix)
		bone_rest_matrix_inv.invert()
		
		bone_rest_matrix_inv.resize4x4()
		bone_rest_matrix.resize4x4()
		bvh_node.temp= (pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv)
		
	
	# Make a dict for fast access without rebuilding a list all the time.
	'''
	xformConstants_dict={
	(True,True):	[Blender.Object.Pose.LOC, Blender.Object.Pose.ROT],\
	(False,True):	[Blender.Object.Pose.ROT],\
	(True,False):	[Blender.Object.Pose.LOC],\
	(False,False):	[],\
	}
	'''
	
	# KEYFRAME METHOD, SLOW, USE IPOS DIRECT
	# TODO: use f-point samples instead (Aligorith)
	
	# Animate the data, the last used bvh_node will do since they all have the same number of frames
	for current_frame in range(len(bvh_node.anim_data)-1): # skip the first frame (rest frame)
		# print current_frame
		
		#if current_frame==150: # debugging
		#	break
		
		# Dont neet to set the current frame
		for bvh_node in bvh_nodes.values():
			pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv= bvh_node.temp
			lx,ly,lz,rx,ry,rz= bvh_node.anim_data[current_frame+1]
			
			if bvh_node.has_rot:
				
				if ROT_STYLE=='QUAT':
					# Set the rotation, not so simple
					bone_rotation_matrix= Euler(math.radians(rx), math.radians(ry), math.radians(rz)).toMatrix()
					
					bone_rotation_matrix.resize4x4()
					#XXX ORDER CHANGE???
					#pose_bone.rotation_quaternion= (bone_rest_matrix * bone_rotation_matrix * bone_rest_matrix_inv).toQuat() # ORIGINAL
					# pose_bone.rotation_quaternion= (bone_rest_matrix_inv * bone_rotation_matrix * bone_rest_matrix).toQuat()
					# pose_bone.rotation_quaternion= (bone_rotation_matrix * bone_rest_matrix).toQuat() # BAD
					# pose_bone.rotation_quaternion= bone_rotation_matrix.toQuat() # NOT GOOD
					# pose_bone.rotation_quaternion= bone_rotation_matrix.toQuat() # NOT GOOD
					
					#pose_bone.rotation_quaternion= (bone_rotation_matrix * bone_rest_matrix_inv * bone_rest_matrix).toQuat()
					#pose_bone.rotation_quaternion= (bone_rest_matrix_inv * bone_rest_matrix * bone_rotation_matrix).toQuat()
					#pose_bone.rotation_quaternion= (bone_rest_matrix * bone_rotation_matrix * bone_rest_matrix_inv).toQuat()
					
					#pose_bone.rotation_quaternion= ( bone_rest_matrix* bone_rest_matrix_inv * bone_rotation_matrix).toQuat()
					#pose_bone.rotation_quaternion= (bone_rotation_matrix * bone_rest_matrix  * bone_rest_matrix_inv).toQuat()
					#pose_bone.rotation_quaternion= (bone_rest_matrix_inv * bone_rotation_matrix  * bone_rest_matrix ).toQuat()
					
					pose_bone.rotation_quaternion= (bone_rest_matrix_inv * bone_rotation_matrix * bone_rest_matrix).toQuat()
					
				else:
					bone_rotation_matrix= Euler(math.radians(rx), math.radians(ry), math.radians(rz)).toMatrix()
					bone_rotation_matrix.resize4x4()
					
					eul= (bone_rest_matrix * bone_rotation_matrix * bone_rest_matrix_inv).toEuler()
					
					#pose_bone.rotation_euler = math.radians(rx), math.radians(ry), math.radians(rz)
					pose_bone.rotation_euler = eul
				
				print("ROTATION" + str(Euler(math.radians(rx), math.radians(ry), math.radians(rz))))
			
			if bvh_node.has_loc:
				# Set the Location, simple too
				
				#XXX ORDER CHANGE
				# pose_bone.location= (TranslationMatrix(Vector(lx, ly, lz) - bvh_node.rest_head_local ) * bone_rest_matrix_inv).translationPart() # WHY * 10? - just how pose works
				# pose_bone.location= (bone_rest_matrix_inv * TranslationMatrix(Vector(lx, ly, lz) - bvh_node.rest_head_local )).translationPart()
				# pose_bone.location= lx, ly, lz
				pose_bone.location= Vector(lx, ly, lz) - bvh_node.rest_head_local
				

#XXX		# TODO- add in 2.5
			if 0:
				# Get the transform 
				xformConstants= xformConstants_dict[bvh_node.has_loc, bvh_node.has_rot]
				
				if xformConstants:
					# Insert the keyframe from the loc/quat
					pose_bone.insertKey(arm_ob, current_frame+IMPORT_START_FRAME, xformConstants, True )
			else:
				
				if bvh_node.has_loc:
					pose_bone.keyframe_insert("location")
				if bvh_node.has_rot:
					if ROT_STYLE=='QUAT':
						pose_bone.keyframe_insert("rotation_quaternion")
					else:
						pose_bone.keyframe_insert("rotation_euler")
				
				
			
		# bpy.ops.anim.keyframe_insert_menu(type=-4) # XXX -     -4 ???
		bpy.ops.screen.frame_offset(delta=1)
		
		# First time, set the IPO's to linear
#XXX	#TODO
		if 0:
			if current_frame==0:
				for ipo in action.getAllChannelIpos().values():
					if ipo:
						for cur in ipo:
							cur.interpolation = Blender.IpoCurve.InterpTypes.LINEAR
							if IMPORT_LOOP:
								cur.extend = Blender.IpoCurve.ExtendTypes.CYCLIC
							
						
		else:
			for cu in action.fcurves:
				for bez in cu.keyframe_points:
					bez.interpolation = 'CONSTANT'
		
	# END KEYFRAME METHOD
	
	
	"""
	# IPO KEYFRAME SETTING
	# Add in the IPOs by adding keyframes, AFAIK theres no way to add IPOs to an action so I do this :/
	for bvh_node in bvh_nodes.values():
		pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv= bvh_node.temp
		
		# Get the transform 
		xformConstants= xformConstants_dict[bvh_node.has_loc, bvh_node.has_rot]
		if xformConstants:
			pose_bone.loc[:]= 0,0,0
			pose_bone.quat[:]= 0,0,1,0
			# Insert the keyframe from the loc/quat
			pose_bone.insertKey(arm_ob, IMPORT_START_FRAME, xformConstants)

	
	action_ipos= action.getAllChannelIpos()
	
	
	for bvh_node in bvh_nodes.values():
		has_loc= bvh_node.has_loc
		has_rot= bvh_node.has_rot
		
		if not has_rot and not has_loc:
			# No animation data
			continue
		
		ipo= action_ipos[bvh_node.temp[0].name] # posebones name as key
		
		if has_loc:
			curve_xloc= ipo[Blender.Ipo.PO_LOCX]
			curve_yloc= ipo[Blender.Ipo.PO_LOCY]
			curve_zloc= ipo[Blender.Ipo.PO_LOCZ]
			
			curve_xloc.interpolation= \
			curve_yloc.interpolation= \
			curve_zloc.interpolation= \
			Blender.IpoCurve.InterpTypes.LINEAR
			
		
		if has_rot:
			curve_wquat= ipo[Blender.Ipo.PO_QUATW]
			curve_xquat= ipo[Blender.Ipo.PO_QUATX]
			curve_yquat= ipo[Blender.Ipo.PO_QUATY]
			curve_zquat= ipo[Blender.Ipo.PO_QUATZ]
			
			curve_wquat.interpolation= \
			curve_xquat.interpolation= \
			curve_yquat.interpolation= \
			curve_zquat.interpolation= \
			Blender.IpoCurve.InterpTypes.LINEAR
		
		# Get the bone 
		pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv= bvh_node.temp
		
		
		def pose_rot(anim_data):
			bone_rotation_matrix= Euler(anim_data[3], anim_data[4], anim_data[5]).toMatrix()
			bone_rotation_matrix.resize4x4()
			return tuple((bone_rest_matrix * bone_rotation_matrix * bone_rest_matrix_inv).toQuat()) # qw,qx,qy,qz
		
		def pose_loc(anim_data):
			return tuple((TranslationMatrix(Vector(anim_data[0], anim_data[1], anim_data[2])) * bone_rest_matrix_inv).translationPart())
		
		
		last_frame= len(bvh_node.anim_data)+IMPORT_START_FRAME-1
		
		if has_loc:
			pose_locations= [pose_loc(anim_key) for anim_key in bvh_node.anim_data]
			
			# Add the start at the end, we know the start is just 0,0,0 anyway
			curve_xloc.append((last_frame, pose_locations[-1][0]))
			curve_yloc.append((last_frame, pose_locations[-1][1]))
			curve_zloc.append((last_frame, pose_locations[-1][2]))
			
			if len(pose_locations) > 1:
				ox,oy,oz= pose_locations[0]
				x,y,z= pose_locations[1]
				
				for i in range(1, len(pose_locations)-1): # from second frame to second last frame
					
					nx,ny,nz= pose_locations[i+1]
					xset= yset= zset= True # we set all these by default
					if abs((ox+nx)/2 - x) < 0.00001:	xset= False
					if abs((oy+ny)/2 - y) < 0.00001:	yset= False
					if abs((oz+nz)/2 - z) < 0.00001:	zset= False
					
					if xset: curve_xloc.append((i+IMPORT_START_FRAME, x))
					if yset: curve_yloc.append((i+IMPORT_START_FRAME, y))
					if zset: curve_zloc.append((i+IMPORT_START_FRAME, z))
					
					# Set the old and use the new
					ox,oy,oz=	x,y,z
					x,y,z=		nx,ny,nz
		
		
		if has_rot:
			pose_rotations= [pose_rot(anim_key) for anim_key in bvh_node.anim_data]
			
			# Add the start at the end, we know the start is just 0,0,0 anyway
			curve_wquat.append((last_frame, pose_rotations[-1][0]))
			curve_xquat.append((last_frame, pose_rotations[-1][1]))
			curve_yquat.append((last_frame, pose_rotations[-1][2]))
			curve_zquat.append((last_frame, pose_rotations[-1][3]))
			
			
			if len(pose_rotations) > 1:
				ow,ox,oy,oz= pose_rotations[0]
				w,x,y,z= pose_rotations[1]
				
				for i in range(1, len(pose_rotations)-1): # from second frame to second last frame
					
					nw, nx,ny,nz= pose_rotations[i+1]
					wset= xset= yset= zset= True # we set all these by default
					if abs((ow+nw)/2 - w) < 0.00001:	wset= False
					if abs((ox+nx)/2 - x) < 0.00001:	xset= False
					if abs((oy+ny)/2 - y) < 0.00001:	yset= False
					if abs((oz+nz)/2 - z) < 0.00001:	zset= False
					
					if wset: curve_wquat.append((i+IMPORT_START_FRAME, w))
					if xset: curve_xquat.append((i+IMPORT_START_FRAME, x))
					if yset: curve_yquat.append((i+IMPORT_START_FRAME, y))
					if zset: curve_zquat.append((i+IMPORT_START_FRAME, z))
					
					# Set the old and use the new
					ow,ox,oy,oz=	w,x,y,z
					w,x,y,z=		nw,nx,ny,nz

	# IPO KEYFRAME SETTING
	"""
	
# XXX NOT NEEDED NOW?
	# pose.update()
	return arm_ob


#=============#
# TESTING     #
#=============#

#('/metavr/mocap/bvh/boxer.bvh')
#('/d/staggered_walk.bvh')
#('/metavr/mocap/bvh/dg-306-g.bvh') # Incompleate EOF
#('/metavr/mocap/bvh/wa8lk.bvh') # duplicate joint names, \r line endings.
#('/metavr/mocap/bvh/walk4.bvh') # 0 channels

'''
import os
DIR = '/metavr/mocap/bvh/'
for f in ('/d/staggered_walk.bvh',):
	#for f in os.listdir(DIR)[5:6]:
	#for f in os.listdir(DIR):
	if f.endswith('.bvh'):
		s = Blender.Scene.New(f)
		s.makeCurrent()
		#file= DIR + f
		file= f
		print f
		bvh_nodes= read_bvh(file, 1.0)
		bvh_node_dict2armature(bvh_nodes, 1)
'''

def load_bvh_ui(context, file, PREF_UI= False):
	
#XXX	if BPyMessages.Error_NoFile(file):
#XXX		return
	
#XXX	Draw= Blender.Draw
	
	IMPORT_SCALE = 0.1
	IMPORT_START_FRAME = 1
	IMPORT_AS_ARMATURE = 1
	IMPORT_AS_EMPTIES = 0
	IMPORT_LOOP = 0
	
	# Get USER Options
	if PREF_UI:
		pup_block = [\
		('As Armature', IMPORT_AS_ARMATURE, 'Imports the BVH as an armature'),\
		('As Empties', IMPORT_AS_EMPTIES, 'Imports the BVH as empties'),\
		('Scale: ', IMPORT_SCALE, 0.001, 100.0, 'Scale the BVH, Use 0.01 when 1.0 is 1 metre'),\
		('Start Frame: ', IMPORT_START_FRAME, 1, 30000, 'Frame to start BVH motion'),\
		('Loop Animation', IMPORT_LOOP, 'Enable cyclic IPOs'),\
		]
		
#XXX		if not Draw.PupBlock('BVH Import...', pup_block):
#XXX			return
	
	# print('Attempting import BVH', file)
	
	if not IMPORT_AS_ARMATURE and not IMPORT_AS_EMPTIES:
		raise('No import option selected')

#XXX	Blender.Window.WaitCursor(1)
	# Get the BVH data and act on it.
	import time
	t1= time.time()
	print('\tparsing bvh...', end= "")
	bvh_nodes= read_bvh(context, file, IMPORT_SCALE)
	print('%.4f' % (time.time()-t1))
	t1= time.time()
	print('\timporting to blender...', end="")
	if IMPORT_AS_ARMATURE:	bvh_node_dict2armature(context, bvh_nodes, IMPORT_START_FRAME, IMPORT_LOOP)
	if IMPORT_AS_EMPTIES:	bvh_node_dict2objects(context, bvh_nodes,  IMPORT_START_FRAME, IMPORT_LOOP)
	
	print('Done in %.4f\n' % (time.time()-t1))
#XXX	Blender.Window.WaitCursor(0)

def main():
	Blender.Window.FileSelector(load_bvh_ui, 'Import BVH', '*.bvh')

from bpy.props import *

class BvhImporter(bpy.types.Operator):
	'''Load a Wavefront OBJ File.'''
	bl_idname = "import_anim.bvh"
	bl_label = "Import BVH"
	
	path = StringProperty(name="File Path", description="File path used for importing the OBJ file", maxlen= 1024, default= "")
	
	def execute(self, context):
		# print("Selected: " + context.active_object.name)

		read_bvh(context, self.properties.path)

		return ('FINISHED',)
	
	def invoke(self, context, event):	
		wm = context.manager
		wm.add_fileselect(self)
		return ('RUNNING_MODAL',)


bpy.ops.add(BvhImporter)


import dynamic_menu
menu_func = lambda self, context: self.layout.operator(BvhImporter.bl_idname, text="Motion Capture (.bvh)...")
menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_import, menu_func)
