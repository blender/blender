#!BPY

"""
Name: 'Motion Capture (.bvh)...'
Blender: 232
Group: 'Export'
Tip: 'Export a (.bvh) motion capture file'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "elysiun")
__version__ = "1.1 12/16/05"

__bpydoc__ = """\
This script exports animation data to BVH motion capture file format.

Supported:<br>

Missing:<br>

Known issues:<br>

Notes:<br>

"""

# $Id$
#
#===============================================#
# BVH Export script 1.0 by Campbell Barton      #
# Copyright MetaVR 30/03/2004,                  #
# if you have any questions about this script   #
# email me cbarton@metavr.com                   #
#===============================================#

# -------------------------------------------------------------------------- 
# BVH Export v1.1 by Campbell Barton (AKA Ideasman) 
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
from Blender import Scene, Object
import math
time = Blender.sys.time
from math import *

# Get the current scene.
scn = Scene.GetCurrent()
context = scn.getRenderingContext()

frameRate = 1.0/context.framesPerSec() # 0.04 = 25fps
scale = 1.0

indent = '\t' # 2 space indent per object
prefixDelimiter = '_'

# Vars used in eular rotation funtcion
RAD_TO_DEG = 180.0/3.14159265359
DEG_TO_RAD = math.pi/180.0


#====================================================#
# Search for children of this object and return them #
#====================================================#
def getChildren(parent):
	children = [] # We'll assume none.
	for child in Object.Get():
		if child.parent == parent:
			children.append( child )
	return children

#====================================================#
# MESSY BUT WORKS: Make a string that shows the			#
# hierarchy as a list and then eval it							 #
#====================================================#
def getHierarchy(root, hierarchy):
	hierarchy = '%s["%s",' % (hierarchy, root.name)
	for child in getChildren(root):
		hierarchy = getHierarchy(child, hierarchy)
	hierarchy = '%s],' % hierarchy
	return hierarchy


#====================================================#
# Strips the prefix off the name before writing			#
#====================================================#
def stripName(name): # name is a string
	
	# WARNING!!! Special case for a custom RIG for output
	# for MetaVR's HPX compatable RIG.
	# print 'stripname', name[0:10]
	if name.lower().startswith('transform('):
		name = name[10:].split(prefixDelimiter)[0]
	return name.split('_')[0]
	

#====================================================#
# Recieves an object name, gets all the data for that#
# node from blender and returns it for formatting    #
# and writing to a file.                             #
#====================================================#
def getNodeData(nodeOb):	
	ob = nodeOb
	obipo = ob.getIpo()
	# Get real location	
	offset = [o*scale for o in ob.getLocation()]
	
	#=========================#
	# Test for X/Y/Z IPO's		#
	#=========================#
	
	# IF we dont have an IPO then dont check the curves.
	# This was added to catch end nodes that never have an IPO, only an offset.
	
	# DUMMY channels xloc, yloc, zloc, xrot, yrot, zrot
	# [<bool>, <bool>, <bool>, <bool>, <bool>, <bool>]
	channels = [0,0,0,0,0,0] # xloc,yloc,zloc,xrot,yrot,zrot
	if obipo != None: # Do have an IPO, checkout which curves are in use.
		# Assume the rot's/loc's dont exist until they proven they do.
		if obipo.getCurve('LocX') != None:
			channels[0] = 1
		if obipo.getCurve('LocY') != None:
			channels[1] = 1
		if obipo.getCurve('LocZ') != None:
			channels[2] = 1
			
		# Now for the rotations, Because of the conversion of rotation coords
		# if there is one rotation er need to store all 3
		if obipo.getCurve('RotX') != None or \
		obipo.getCurve('RotY') != None or \
		obipo.getCurve('RotZ') != None:
			channels[3] = channels[4] = channels[5] = 1
	#print ob, channels
	return offset, channels


#====================================================#
# Writes the BVH hierarchy to a file                 #
# hierarchy: is a list of the empty hierarcht        #
# level: how many levels we are down the tree,       #
# ...used for indenting                              #
# Also gathers channelList , so we know the order to #
# write	the motiondata in                            #
#====================================================#
def hierarchy2bvh(file, hierarchy, level, channelList, nodeObjectList):
	nodeName = hierarchy[0]
	ob = Object.Get(nodeName)
	'''
	obipo = ob.getIpo()
	if obipo != None:
		obcurves = obipo.getCurves()
	else:
		obcurves = None
	'''
	#============#
	# JOINT NAME #
	#============# 
	file.write(level * indent)
	if level == 0:
		# Add object to nodeObjectList
		#nodeObjectList.append( (ob, obipo, obcurves) )
		nodeObjectList.append( ob )
		file.write( 'ROOT %s\n' % stripName(nodeName) )
	# If this is the last object in the list then we
	# dont bother withwriting its real name, use "End Site" instead
	elif len(hierarchy) == 1:
		file.write( 'End Site\n' )
	# Ok This is a normal joint
	else:
		# Add object to nodeObjectList
		#nodeObjectList.append((ob, obipo, obcurves))
		nodeObjectList.append( ob )
		file.write( 'JOINT %s\n' % stripName(nodeName) )
	#================#
	# END JOINT NAME #
	#================#
	
	# Indent again, this line is just for the brackets
	file.write( '%s{\n' % (level * indent) )

	# Indent
	level += 1
	
	#================================================#
	# Data for writing to a file offset and channels #
	#================================================#
	offset, channels = getNodeData(ob)
	
	#============#
	# Offset     #
	#============# 
	file.write( '%sOFFSET %.6f %.6f %.6f\n' %\
	(level*indent, scale*offset[0], scale*offset[1], scale*offset[2]) )
	
	#============#
	# Channels   #
	#============# 
	if len(hierarchy) != 1:
		# Channels, remember who is where so when we write motiondata
		file.write('%sCHANNELS %i ' % (level*indent, len([c for c in channels if c ==1]) ))
		if channels[0]:
			file.write('Xposition ')
			channelList.append([len(nodeObjectList)-1, 0])
		if channels[1]:
			file.write('Yposition ')
			channelList.append([len(nodeObjectList)-1, 1])
		if channels[2]:
			file.write('Zposition ')
			channelList.append([len(nodeObjectList)-1, 2])
		if channels[5]:
			file.write('Zrotation ')
			channelList.append([len(nodeObjectList)-1, 5])
		if channels[3]:
			file.write('Xrotation ')
			channelList.append([len(nodeObjectList)-1, 3])
		if channels[4]:
			file.write('Yrotation ')
			channelList.append([len(nodeObjectList)-1, 4])
		file.write('\n')

	# Loop through children if any and run this function (recursively)
	for hierarchyIdx in range(len(hierarchy)-1):
		level = hierarchy2bvh(file, hierarchy[hierarchyIdx+1], level, channelList, nodeObjectList)
	# Unindent
	level -= 1
	file.write('%s}\n' % (level * indent))
	
	return level

# added by Ben Batt 30/3/2004 to make the exported rotations correct
def ZYXToZXY(x, y, z):
	'''
	Converts a set of Euler rotations (x, y, z) (which are intended to be
	applied in z, y, x order, into a set which are intended to be applied in
	z, x, y order (the order expected by .bvh files)
	'''
	A,B = cos(x),sin(x)
	C,D = cos(y),sin(y)
	E,F = cos(z),sin(z)

	x = asin(-B*C)
	y = atan2(D, A*C)
	z = atan2(-B*D*E + A*F, B*D*F + A*E)

	# this seems to be necessary - not sure why (right/left-handed coordinates?)
	# x = -x # x is negative, see below.
	return -x*RAD_TO_DEG, y*RAD_TO_DEG, z*RAD_TO_DEG


''' # UNUSED, JUST GET OBJECT LOC/ROT
def getIpoLocation(object, obipo, curves, frame):
	x =	y = z = rx = ry = rz =0
	if obipo:
		for i in range(obipo.getNcurves()):
			if curves[i].getName() =='LocX':
				x = obipo.EvaluateCurveOn(i,frame)
			elif curves[i].getName() =='LocY':
				y = obipo.EvaluateCurveOn(i,frame)
			elif curves[i].getName() =='LocZ':
				z = obipo.EvaluateCurveOn(i,frame)
			elif curves[i].getName() =='RotX':
				rx = obipo.EvaluateCurveOn(i,frame)
			elif curves[i].getName() =='RotY':
				ry = obipo.EvaluateCurveOn(i,frame)
			elif curves[i].getName() =='RotZ':
				rz = obipo.EvaluateCurveOn(i,frame)
	return x, y, z, rx*10*DEG_TO_RAD, ry*10*DEG_TO_RAD, rz*10*DEG_TO_RAD
'''

#====================================================#
# Return the BVH motion for the spesified frame      #
# hierarchy: is a list of the empty hierarcht        #
# level: how many levels we are down the tree,       #
# ...used for indenting                              #
#====================================================#
def motion2bvh(file, frame, chennelList, nodeObjectList):	
	for chIdx in chennelList:
		#ob, obipo, obcurves = nodeObjectList[chIdx[0]]
		ob = nodeObjectList[chIdx[0]]
		chType = chIdx[1]
		
		# Get object rotation
		x, y, z = ob.getEuler()
		
		# Convert the rotation from ZYX order to ZXY order
		x, y, z = ZYXToZXY(x, y, z)
		
		# Location
		xloc, yloc, zloc = ob.matrixLocal[3][:3]
		
		# Using regular Locations stuffs upIPO locations stuffs up
		# Get IPO locations instead
		#xloc, yloc, zloc, x, y, z = getIpoLocation(ob, obipo, obcurves, frame)
		# Convert the rotation from ZYX order to ZXY order
		#x, y, z = ZYXToZXY(x, y, z)
		
		
		# WARNING non standard Location
		# xloc, zloc, yloc = -xloc, yloc, zloc
		
		if chType == 0:
			file.write('%.6f ' % (scale * xloc))
		if chType == 1:
			file.write('%.6f ' % (scale * yloc))
		if chType == 2:
			file.write('%.6f ' % (scale * zloc))
		if chType == 3:
			file.write('%.6f ' % x)
		if chType == 4:
			file.write('%.6f ' % y)
		if chType == 5:
			file.write('%.6f ' % z)
	file.write('\n')
	

def saveBVH(filename):
	t = time()
	if not filename.lower().endswith('.bvh'):
		filename += '.bvh' # for safety
	
	# Here we store a serialized list of blender objects as they appier
	# in the hierarchy, this is refred to when writing motiondata
	nodeObjectList = []
	
	# In this list we store a 2 values for each node
	# 1) An index pointing to a blender object
	# in objectList
	# 2) The type if channel x/y/z rot:x/y/z - Use 0-5 to indicate this
	chennelList = []
	
	print '\nBVH  1.1 by Campbell Barton (Ideasman) - cbarton@metavr.com'
	
	# Get the active object and recursively traverse its kids to build
	# the BVH hierarchy, then eval the string to make a hierarchy list.
	hierarchy = eval(getHierarchy(scn.getActiveObject(),''))[0] # somhow this returns a tuple with one list in it.
	
	# Put all data in the file we have selected file.
	file = open(filename, "w")
	file.write('HIERARCHY\n') # all bvh files have this on the first line
	
	# Write the whole hirarchy to a list
	level = 0 # Indenting level, start with no indent
	level = hierarchy2bvh(file, hierarchy, level, chennelList, nodeObjectList)
	
	#====================================================#
	# MOTION: Loop through the frames ande write out     #
	# the motion data for each                           #
	#====================================================#
	# Do some basic motion file header stuff
	file.write( 'MOTION\n' )
	file.write( 'Frames: %i\n'	% ( 1 + context.endFrame() - context.startFrame() ) )
	file.write( 'Frame Time: %.6f\n' % frameRate )
	
	frames = range(context.startFrame()+1, context.endFrame()+1)
	print 'exporting %i of motion...' % len(frames)
	
	for frame in frames:
		context.currentFrame(frame)
		scn.update(1) # Update locations so we can write the new locations. This is the SLOW part.
		# Blender.Window.RedrawAll() # Debugging.
		
		motion2bvh(file, frame, chennelList, nodeObjectList) # Write the motion to a file.
	
	file.write('\n') # newline
	file.close()
	print '...Done in %.4f seconds.' % (time()-t)
	
Blender.Window.FileSelector(saveBVH, 'Export BVH')
