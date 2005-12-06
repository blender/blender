#!BPY

"""
Name: 'Motion Capture (.bvh)...'
Blender: 239
Group: 'Import'
Tip: 'Import a (.bvh) motion capture file'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.0.4 05/12/04"

__bpydoc__ = """\
This script imports BVH motion capture data to Blender.

Supported: Poser 3.01<br>

Missing:<br>

Known issues:<br>

Notes:<br>
	 Jean-Michel Soler improved importer to support Poser 3.01 files;<br>
	 Jean-Baptiste Perin wrote a script to create an armature out of the
Empties created by this importer, it's in the Scripts window -> Scripts -> Animation menu.
"""

# $Id$
#

#===============================================#
# BVH Import script 1.05 patched by Campbell    #
# Modified to use Mathutils for matrix math,    #
# Fixed possible joint naming bug,              #
# Imports BVH's with bad EOF gracefully         #
# Fixed duplicate joint names, make them unique #
# Use \r as well as \n for newlines             #
# Added suppot for nodes with 0 motion channels #
# Rotation IPOs never cross more then 180d      #
#    fixes sub frame tweening and time scaling  #
# 5x overall speedup.                           #
# 06/12/2005,                                   #	
#===============================================#

#===============================================#
# BVH Import script 1.04 patched by jms         #
# Small modif for blender 2.40                  #
# 04/12/2005,                                   #	
#===============================================#

#===============================================#
# BVH Import script 1.03 patched by Campbell    #
# Small optimizations and scale input           #
# 01/01/2005,                                   #	
#===============================================#

#===============================================#
# BVH Import script 1.02 patched by Jm Soler    #
# to the Poser 3.01 bvh file                    #
# 28/12/2004,                                   #	
#===============================================#

#===============================================#
# BVH Import script 1.0 by Campbell Barton      #
# 25/03/2004, euler rotation code taken from    #
# Reevan Mckay's BVH import script v1.1         #
# if you have any questions about this scrip.   #
# email me cbarton@metavr.com                   #
#===============================================#

#===============================================#
# TODO:                                         #
# * Create bones when importing                 #
# * Make an IPO jitter removal script           #
# * Work out a better naming system             #
#===============================================#

# -------------------------------------------------------------------------- 
# BVH Import v1.05 by Campbell Barton (AKA Ideasman) 
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
from Blender import Window, Object, Scene, Ipo, Draw
from Blender.Scene import Render


# Attempt to load psyco, speed things up
try:
	import psyco
	psyco.full()	
	print 'using psyco to speed up BVH importing'
except:
	#print 'psyco is not present on this system'
	pass



def main():
	global scale
	scale = None
	
	# Update as we load?
	debug = 0
	
	def getScale():
		return Draw.PupFloatInput('BVH Scale: ', 0.01, 0.001, 10.0, 0.1, 3)
	
	
	#===============================================#
	# MAIN FUNCTION - All things are done from here #
	#===============================================#
	def loadBVH(filename):
		global scale
		print '\nBVH Importer 1.05 by Campbell Barton (Ideasman) - cbarton@metavr.com'
		
		objectCurveMapping = {}
		objectNameMapping = {}
		objectMotiondataMapping = {}
		
		# Here we store the Ipo curves in the order they load.
		channelCurves = []
		
		# Object list
		# We need this so we can loop through the objects and edit there IPO's 
		# Chenging there rotation to EULER rotation
		objectList = []
		
		if scale == None:
			tempscale = getScale()
			if tempscale:
				scale = tempscale
			else:
				scale = 0.01
		
		Window.WaitCursor(1)
		# Unique names, dont reuse any of these names.
		uniqueObNames = [ob.name for ob in Object.Get()]
		
		
		# FUNCTIONS ====================================#
		def getUniqueObName(name):
			i = 0
			newname = name[:min(len(name), 12)] # Concatinate to 12 chars
			while newname in uniqueObNames:
				newname = name + str(i)
				i+=1
			return newname
			
		# Change the order rotation is applied.
		RotationMatrix = Blender.Mathutils.RotationMatrix
		MATRIX_IDENTITY_3x3 = Blender.Mathutils.Matrix([1.0,0.0,0.0],[0.0,1.0,0.0],[0.0,0.0,1.0])
		def eulerRotate(x,y,z): 
			x,y,z = x%360,y%360,z%360 # Clamp all values between 0 and 360, values outside this raise an error.
			xmat = RotationMatrix(x,3,'x')
			ymat = RotationMatrix(y,3,'y')
			zmat = RotationMatrix(z,3,'z')
			# Standard BVH multiplication order, apply the rotation in the order Z,X,Y
			return (ymat*(xmat * (zmat * MATRIX_IDENTITY_3x3))).toEuler()
		

		currentFrame = 1 # Set the initial frame to import all data to.
		
		#===============================================#
		# makeJoint: Here we use the node data          #
		# from the BVA file to create an empty          #
		#===============================================#
		BVH2BLEND_TX_NAME = {'Xposition':'LocX','Yposition':'LocY','Zposition':'LocZ','Xrotation':'RotX','Yrotation':'RotY','Zrotation':'RotZ'}
		def makeJoint(name, parent, offset, channels):
			ob = Object.New('Empty', name) # New object, ob is shorter and nicer to use.
			
			objectNameMapping[name] = ob
			scn.link(ob) # place the object in the current scene
			ob.sel = 1
			
			# Make me a child of another empty.
			# Vale of None will make the empty a root node (no parent)
			if parent[-1]: # != None
				obParent = objectNameMapping[parent[-1]] # We use this a bit so refrence it here.
				obParent.makeParent([ob], 1, 0) #ojbs, noninverse, 1 = not fast.
		
			# Offset Empty from BVH's initial joint location.
			ob.setLocation(offset[0]*scale, offset[1]*scale, offset[2]*scale)
		
			# Add Ipo's for necessary channels
			newIpo = Ipo.New('Object', name)
			ob.setIpo(newIpo)
			obname = ob.name
			for channelType in channels:
				channelType = BVH2BLEND_TX_NAME[channelType]
				curve = newIpo.addCurve(channelType)
				curve.setInterpolation('Linear')
				objectCurveMapping[(obname, channelType)] = curve
		
			# Add to object list
			objectList.append(ob)
			
			# Redraw if debugging
			if debug: Blender.Redraw()
			
			
		#===============================================#
		# makeEnd: Here we make an end node             #
		# This is needed when adding the last bone      #
		#===============================================#
		def makeEnd(parent, offset):
			new_name = parent[-1] + '_end'
			ob = Object.New('Empty', new_name) # New object, ob is shorter and nicer to use.
			objectNameMapping[new_name] = ob
			scn.link(ob)
			ob.sel = 1
			
			# Dont check for a parent, an end node MUST have a parent
			obParent = objectNameMapping[parent[-1]] # We use this a bit so refrence it here.
			obParent.makeParent([ob], 1, 0) #ojbs, noninverse, 1 = not fast.
		
			# Offset Empty
			ob.setLocation(offset[0]*scale, offset[1]*scale, offset[2]*scale) 
			
			# Redraw if debugging
			if debug: Blender.Redraw()
		# END FUNCTION DEFINITIONS ====================================#
			
		
		
		
		time1 = Blender.sys.time()
		
		# Get the current scene.
		scn = Scene.GetCurrent()
		#context = scn.getRenderingContext()
		
		# DeSelect All
		for ob in scn.getChildren():
			ob.sel = 0
		
		# File loading stuff
		# Open the file for importing
		file = open(filename, 'r')	
		
		# Seperate into a list of lists, each line a list of words.
		lines = file.readlines()
		# Non standard carrage returns?
		if len(lines) == 1:
			lines = lines[0].split('\r')
		
		# Split by whitespace.
		lines =[ll for ll in [ [w for w in l.split() if w != '\n' ] for l in lines] if ll]
		# End file loading code
	
		
		
		# Create Hirachy as empties
		if lines[0][0] == 'HIERARCHY':
			print 'Importing the BVH Hierarchy for:', filename
		else:
			return 'ERROR: This is not a BVH file'
		
		# A liniar list of ancestors to keep track of a single objects heratage
		# at any one time, this is appended and removed, dosent store tree- just a liniar list.
		# ZERO is a place holder that means we are a root node. (no parents)
		parent = [None]	
		
		#channelList, sync with objectList:  [[channelType1, channelType2...],	[channelType1, channelType2...)]
		channelList = []
		channelIndex = -1
		
		lineIdx = 0 # An index for the file.
		while lineIdx < len(lines) -1:
			#...
			if lines[lineIdx][0] == 'ROOT' or lines[lineIdx][0] == 'JOINT':
				
				# Join spaces into 1 word with underscores joining it.
				if len(lines[lineIdx]) > 2:
					lines[lineIdx][1] = '_'.join(lines[lineIdx][1:])
					lines[lineIdx] = lines[lineIdx][:2]
				
				# MAY NEED TO SUPPORT MULTIPLE ROOT's HERE!!!, Still unsure weather multiple roots are possible.??
				
				# Make sure the names are unique- Object names will match joint names exactly and both will be unique.
				name = getUniqueObName(lines[lineIdx][1])
				uniqueObNames.append(name)
				
				print '%snode: %s, parent: %s' % (len(parent) * '  ', name,  parent[-1])
				
				lineIdx += 2 # Incriment to the next line (Offset)
				offset = ( float(lines[lineIdx][1]), float(lines[lineIdx][2]), float(lines[lineIdx][3]) )
				lineIdx += 1 # Incriment to the next line (Channels)
				
				# newChannel[Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation]
				# newChannel references indecies to the motiondata,
				# if not assigned then -1 refers to the last value that will be added on loading at a value of zero, this is appended 
				# We'll add a zero value onto the end of the MotionDATA so this is always refers to a value.
				newChannel = [-1, -1, -1, -1, -1, -1] 
				for channel in lines[lineIdx][2:]:
					channelIndex += 1 # So the index points to the right channel
					if channel == 'Xposition':
						newChannel[0] = channelIndex
					elif channel == 'Yposition':
						newChannel[1] = channelIndex
					elif channel == 'Zposition':
						newChannel[2] = channelIndex
					elif channel == 'Xrotation':
						newChannel[3] = channelIndex
					elif channel == 'Yrotation':
						newChannel[4] = channelIndex
					elif channel == 'Zrotation':
						newChannel[5] = channelIndex
				
				channelList.append(newChannel)
				
				channels = lines[lineIdx][2:]
				
				# Call funtion that uses the gatrhered data to make an empty.
				makeJoint(name, parent, offset, channels)
				
				# If we have another child then we can call ourselves a parent, else 
				parent.append(name)
	
			# Account for an end node
			if lines[lineIdx][0] == 'End' and lines[lineIdx][1] == 'Site': # There is somtimes a name after 'End Site' but we will ignore it.
				lineIdx += 2 # Incriment to the next line (Offset)
				offset = ( float(lines[lineIdx][1]), float(lines[lineIdx][2]), float(lines[lineIdx][3]) )
				makeEnd(parent, offset)
				
				# Just so we can remove the Parents in a uniform way- End end never has kids
				# so this is a placeholder
				parent.append(None)
			
			if len(lines[lineIdx]) == 1 and lines[lineIdx][0] == '}': # == ['}']
				parent.pop() # Remove the last item
			
			#=============================================#
			# BVH Structure loaded, Now import motion     #
			#=============================================#		
			if len(lines[lineIdx]) == 1 and lines[lineIdx][0] == 'MOTION':
				print '\nImporting motion data'
				lineIdx += 3 # Set the cursor to the first frame
				
				#=============================================#
				# Add a ZERO keyframe, this keeps the rig     #
				# so when we export we know where all the     #
				# joints start from                           #
				#=============================================#
				
				for obIdx, ob in enumerate(objectList):
					obname = ob.name
					if channelList[obIdx][0] != -1:
						objectCurveMapping[obname, 'LocX'].addBezier((currentFrame,0))
						objectMotiondataMapping[obname, 'LocX'] = []
					if channelList[obIdx][1] != -1:
						objectCurveMapping[obname, 'LocY'].addBezier((currentFrame,0))
						objectMotiondataMapping[obname, 'LocY'] = []
					if channelList[obIdx][2] != -1:
						objectCurveMapping[obname, 'LocZ'].addBezier((currentFrame,0))
						objectMotiondataMapping[obname, 'LocZ'] = []
					if\
					channelList[obIdx][3] != -1 or\
					channelList[obIdx][4] != -1 or\
					channelList[obIdx][5] != -1:
						objectMotiondataMapping[obname, 'RotX'] = []
						objectMotiondataMapping[obname, 'RotY'] = []
						objectMotiondataMapping[obname, 'RotZ'] = []
				
				#=============================================#
				# Loop through frames, each line a frame      #
				#=============================================#			
				MOTION_DATA_LINE_LEN = len(lines[lineIdx])
				while lineIdx < len(lines):
					line = lines[lineIdx]
					if MOTION_DATA_LINE_LEN != len(line):
						print 'ERROR: Incomplete motion data on line %i, finishing import.' % lineIdx
						break
						
					# Exit loop if we are past the motiondata.
					# Some BVH's have extra tags like 'CONSTRAINTS and MOTIONTAGS'
					# I dont know what they do and I dont care, they'll be ignored here.
					if len(line) < len(objectList):
						print '...ending on unknown tags'
						break
					
					
					currentFrame += 1 # Incriment to next frame
									
					#=============================================#
					# Import motion data and assign it to an IPO	#
					#=============================================#
					line.append(0.0) # Use this as a dummy var for objects that dont have a loc/rotate channel.
					
					if debug: Blender.Redraw() 
					for obIdx, ob in enumerate(objectList):
						obname = ob.name
						obChannel = channelList[obIdx] 
						if channelList[obIdx][0] != -1:
							objectMotiondataMapping[obname, 'LocX'].append((currentFrame, scale * float(  line[obChannel[0]]  )))
							
						if channelList[obIdx][1] != -1:
							objectMotiondataMapping[obname, 'LocY'].append((currentFrame, scale * float(  line[obChannel[1]]	 )))

						if channelList[obIdx][2] != -1:
							objectMotiondataMapping[obname, 'LocZ'].append((currentFrame, scale * float(  line[obChannel[2]]  )))
						
						if obChannel[3] != -1 or obChannel[4] != -1 or obChannel[5] != -1:						
							x, y, z = eulerRotate(float( line[obChannel[3]] ), float( line[obChannel[4]] ), float( line[obChannel[5]] ))
							x,y,z = x/10.0, y/10.0, z/10.0 # For IPO's 36 is 360d
							motionMappingRotX = objectMotiondataMapping[obname, 'RotX']
							motionMappingRotY = objectMotiondataMapping[obname, 'RotY']
							motionMappingRotZ = objectMotiondataMapping[obname, 'RotZ']
							
							# Make interpolation not cross between 180d, thjis fixes sub frame interpolation and time scaling.
							# Will go from (355d to 365d) rather then to (355d to 5d) - inbetween these 2 there will now be a correct interpolation.
							if len(motionMappingRotX) > 1:
								while (motionMappingRotX[-1][1] - x) > 18: x+=36
								while (motionMappingRotX[-1][1] - x) < -18: x-=36
								
								while (motionMappingRotY[-1][1] - y) > 18: y+=36
								while (motionMappingRotY[-1][1] - y) < -18: y-=36
								
								while (motionMappingRotZ[-1][1] - z) > 18: z+=36
								while (motionMappingRotZ[-1][1] - z) < -18: z-=36
							
							motionMappingRotX.append((currentFrame, x))
							motionMappingRotY.append((currentFrame, y))
							motionMappingRotZ.append((currentFrame, z))
						# Done importing motion data #
					
					lineIdx += 1
				
				#=======================================#
				# Now Write the motion to the IPO's     #
				#=======================================#
				for key, motion_data in objectMotiondataMapping.iteritems():
					
					# Strip the motion data where all the points have the same falue.
					i = len(motion_data) -2
					while i > 0 and len(motion_data) > 2:
						if motion_data[i][1] == motion_data[i-1][1] == motion_data[i+1][1]:
							motion_data.pop(i)
						i-=1
					# Done stripping.						
					
					obname, tx_type = key
					curve = objectCurveMapping[obname, tx_type]
					for point_data in motion_data:
						curve.addBezier( point_data )
				# Imported motion to an IPO
				
				# No point in looking further, when this loop is done
				# There is nothine else left to do			
				break
				
			# Main file loop
			lineIdx += 1
			
		print 'bvh import time for %i frames: %.6f' % (currentFrame, Blender.sys.time() - time1)
		Window.RedrawAll()
		Window.WaitCursor(0)
	
	Blender.Window.FileSelector(loadBVH, "Import BVH")
	
	#=============#
	# TESTING     #
	#=============#
	'''
	#loadBVH('/metavr/mocap/bvh/boxer.bvh')
	#loadBVH('/metavr/mocap/bvh/dg-306-g.bvh') # Incompleate EOF
	#loadBVH('/metavr/mocap/bvh/wa8lk.bvh') # duplicate joint names, \r line endings.
	#loadBVH('/metavr/mocap/bvh/walk4.bvh') # 0 channels
	scale  = 0.01
	import os
	DIR = '/metavr/mocap/bvh/'
	for f in os.listdir(DIR):
		if f.endswith('.bvh'):
			s = Scene.New(f)
			s.makeCurrent()
			loadBVH(DIR + f)
	'''
if __name__ == '__main__':
	main()