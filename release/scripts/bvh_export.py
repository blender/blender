#!BPY

"""
Name: 'Motion Capture (.bvh)...'
Blender: 232
Group: 'Export'
Tip: 'Export a (.bvh) motion capture file'
"""

# $Id$
#
#===============================================#
# BVH Export script 1.0 by Campbell Barton      #
# Copyright MetaVR 30/03/2004,                  #
# if you have any questions about this script   #
# email me ideasman@linuxmail.org               #
#                                               #
#===============================================#

# -------------------------------------------------------------------------- 
# BVH Export v0.9 by Campbell Barton (AKA Ideasman) 
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
from Blender import Scene, Object
import math
from math import *

# Get the current scene.
scn = Scene.GetCurrent()
context = scn.getRenderingContext()

frameRate = 0.3333 # 0.04 = 25fps
scale = 1

indent = '  ' # 2 space indent per object
prefixDelimiter = '_'

# Vars used in eular rotation funtcion
RAD_TO_DEG = 180.0/3.14159265359



#====================================================#
# Search for children of this object and return them #
#====================================================#
def getChildren(parent):
  children = [] # We'll assume none.
  for child in Object.Get():
    if child.getParent() == Object.Get(parent):
      children.append( child.getName() )
  return children

#====================================================#
# MESSY BUT WORKS: Make a string that shows the      #
# hierarchy as a list and then eval it               #
#====================================================#
def getHierarchy(root, hierarchy):
  hierarchy = hierarchy + '["' + root + '",'
  for child in getChildren(root):
    hierarchy = getHierarchy(child, hierarchy)
  hierarchy += '],' 
  return hierarchy


#====================================================#
# Strips the prefix off the name before writing      #
#====================================================#
def stripName(name): # name is a string
  
  # WARNING!!! Special case for a custom RIG for output
  # for MetaVR's HPX compatable RIG.
  print 'stripname', name[0:10]
  if name[0:10] == 'Transform(':
    name = name[10:]
    while name[-1] != ')':
      name = name[0:-1]
      print name
    name = name[:-1]
  
  
  return name[1+name.find(prefixDelimiter): ]
  

#====================================================#
# Return a 6 deciaml point floating point value      #
# as a string that dosent have any python chars      #
#====================================================#  
def saneFloat(float):
  #return '%(float)b' % vars()  # 6 fp as house.hqx
  return str('%f' % float) + ' '



#====================================================#
# Recieves an object name, gets all the data for that#
# node from blender and returns it for formatting    #
# and writing to a file.                             #
#====================================================#
def getNodeData(nodeName):  
  Object.Get(nodeName)
  # Get real location  
  offset = Object.Get(nodeName).getLocation()
  offset = (offset[0]*scale, offset[1]*scale, offset[2]*scale,)
  
  #=========================#
  # Test for X/Y/Z IPO's    #
  #=========================#
  obipo = Object.Get(nodeName).getIpo()
  
  # IF we dont have an IPO then dont check the curves.
  # This was added to catch end nodes that never have an IPO, only an offset.
  if obipo == None: 
    xloc=yloc=zloc=xrot=yrot=zrot = 0
  
  else: # Do have an IPO, checkout which curves are in use.
    # Assume the rot's/loc's exist until proven they dont
    xloc=yloc=zloc=xrot=yrot=zrot = 1
    if obipo.getCurve('LocX') == None:
      xloc = 0
    if obipo.getCurve('LocY') == None:
      yloc = 0
    if obipo.getCurve('LocZ') == None:
      zloc = 0
      
    # Now for the rotations, Because of the conversion of rotation coords
    # if there is one rotation er need to store all 3
    if obipo.getCurve('RotX') == None and \
    obipo.getCurve('RotY') == None and \
    obipo.getCurve('RotZ') == None:
      xrot=yrot=zrot = 0
  
  # DUMMY channels xloc, yloc, zloc, xrot, yrot, zrot
  # [<bool>, <bool>, <bool>, <bool>, <bool>, <bool>]
  channels = [xloc, yloc, zloc, xrot, yrot, zrot]
  
  return offset, channels


#====================================================#
# Return the BVH hierarchy to a file from a list     #
# hierarchy: is a list of the empty hierarcht        #
# bvhHierarchy: a string, in the bvh format to write #
# level: how many levels we are down the tree,       #
# ...used for indenting                              #
# Also gathers channelList , so we know the order to #
# write  the motiondata in                           #
#====================================================#
def hierarchy2bvh(hierarchy, bvhHierarchy, level, channelList, nodeObjectList):
  nodeName = hierarchy[0]
  
  # Add object to nodeObjectList
  nodeObjectList.append(Object.Get(nodeName))
  
  #============#
  # JOINT NAME #
  #============# 
  bvhHierarchy += level * indent
  if level == 0:
    # Add object to nodeObjectList
    nodeObjectList.append(Object.Get(nodeName))
    bvhHierarchy+= 'ROOT '
    bvhHierarchy += stripName(nodeName) + '\n'
  # If this is the last object in the list then we
  # dont bother withwriting its real name, use "End Site" instead
  elif len(hierarchy) == 1:
    bvhHierarchy+= 'End Site\n'
  # Ok This is a normal joint
  else:
    # Add object to nodeObjectList
    nodeObjectList.append(Object.Get(nodeName))
    bvhHierarchy+= 'JOINT '
    bvhHierarchy += stripName(nodeName) + '\n'
  #================#
  # END JOINT NAME #
  #================# 

  # Indent again, this line is just for the brackets
  bvhHierarchy += level * indent + '{' + '\n'

  # Indent
  level += 1   
  
  #================================================#
  # Data for writing to a file offset and channels #
  #================================================#
  offset, channels = getNodeData(nodeName)
  
  #============#
  # Offset     #
  #============# 
  bvhHierarchy += level * indent + 'OFFSET ' + saneFloat(scale * offset[0]) + ' '  + saneFloat(scale * offset[1]) + ' ' + saneFloat(scale * offset[2]) + '\n'
  
  #============#
  # Channels   #
  #============# 
  if len(hierarchy) != 1:
    # Channels, remember who is where so when we write motiondata
    bvhHierarchy += level * indent + 'CHANNELS '
    # Count the channels
    chCount = 0
    for chn in channels:
      chCount += chn
    bvhHierarchy += str(chCount) + ' '
    if channels[0]:
      bvhHierarchy += 'Xposition '
      channelList.append([len(nodeObjectList)-1, 0])
    if channels[1]:
      bvhHierarchy += 'Yposition '
      channelList.append([len(nodeObjectList)-1, 1])
    if channels[2]:
      bvhHierarchy += 'Zposition '
      channelList.append([len(nodeObjectList)-1, 2])
    if channels[5]:
      bvhHierarchy += 'Zrotation '
      channelList.append([len(nodeObjectList)-1, 5])
    if channels[3]:
      bvhHierarchy += 'Xrotation '
      channelList.append([len(nodeObjectList)-1, 3])
    if channels[4]:
      bvhHierarchy += 'Yrotation '
      channelList.append([len(nodeObjectList)-1, 4])
    
    bvhHierarchy += '\n'

  # Loop through children if any and run this function (recursively)
  for hierarchyIdx in range(len(hierarchy)-1):
    bvhHierarchy, level, channelList, nodeObjectList = hierarchy2bvh(hierarchy[hierarchyIdx+1], bvhHierarchy, level, channelList, nodeObjectList)
  # Unindent
  level -= 1
  bvhHierarchy += level * indent + '}' + '\n'
  
  return bvhHierarchy, level, channelList, nodeObjectList

# added by Ben Batt 30/3/2004 to make the exported rotations correct
def ZYXToZXY(x, y, z):
  '''
  Converts a set of Euler rotations (x, y, z) (which are intended to be
  applied in z, y, x order) into a set which are intended to be applied in
  z, x, y order (the order expected by .bvh files)
  '''
  A,B = cos(x),sin(x)
  C,D = cos(y),sin(y)
  E,F = cos(z),sin(z)

  x = asin(-B*C)
  y = atan2(D, A*C)
  z = atan2(-B*D*E + A*F, B*D*F + A*E)

  # this seems to be necessary - not sure why (right/left-handed coordinates?)
  x = -x
  return x*RAD_TO_DEG, y*RAD_TO_DEG, z*RAD_TO_DEG



def getIpoLocation(object, frame):
  x =  y = z = 0 
  obipo = object.getIpo()
  for i in range(object.getIpo().getNcurves()):
    if obipo.getCurves()[i].getName() =='LocX':
      x = object.getIpo().EvaluateCurveOn(i,frame)
    elif obipo.getCurves()[i].getName() =='LocY':
      y = object.getIpo().EvaluateCurveOn(i,frame)
    elif obipo.getCurves()[i].getName() =='LocZ':
      z = object.getIpo().EvaluateCurveOn(i,frame)
  return x, y, z


#====================================================#
# Return the BVH motion for the spesified frame      #
# hierarchy: is a list of the empty hierarcht        #
# bvhHierarchy: a string, in the bvh format to write #
# level: how many levels we are down the tree,       #
# ...used for indenting                              #
#====================================================#
def motion2bvh(frame, chennelList, nodeObjectList):
  
  motionData = '' # We'll append the frames to the string.
  
  for chIdx in chennelList:
    ob = nodeObjectList[chIdx[0]]
    chType = chIdx[1]
    
    # Get object rotation
    x, y, z = ob.getEuler()
    
    # Convert the rotation from ZYX order to ZXY order
    x, y, z = ZYXToZXY(x, y, z)
     
    
    # Using regular Locations stuffs upIPO locations stuffs up
    # Get IPO locations instead
    xloc, yloc, zloc = getIpoLocation(ob, frame)

    # WARNING non standard Location
    xloc, zloc, yloc = -xloc, yloc, zloc
    

    if chType == 0:
      motionData += saneFloat(scale * (xloc))
    if chType == 1:
      motionData += saneFloat(scale * (yloc))
    if chType == 2:
      motionData += saneFloat(scale * (zloc))      
    if chType == 3:
      motionData += saneFloat(x)
    if chType == 4:
      motionData += saneFloat(y)
    if chType == 5:
      motionData += saneFloat(z)
    
    motionData += ' '
     
  motionData += '\n'
  return motionData

def saveBVH(filename):

  if filename.find('.bvh', -4) <= 0: filename += '.bvh' # for safety

  # Here we store a serialized list of blender objects as they appier
  # in the hierarchy, this is refred to when writing motiondata
  nodeObjectList = []
  
  # In this list we store a 2 values for each node
  # 1) An index pointing to a blender object
  # in objectList
  # 2) The type if channel x/y/z rot:x/y/z - Use 0-5 to indicate this
  chennelList = []
  
  print ''
  print 'BVH  1.0 by Campbell Barton (Ideasman) - ideasman@linuxmail.org'
  
  # Get the active object and recursively traverse its kids to build
  # the BVH hierarchy, then eval the string to make a hierarchy list.
  hierarchy = eval(getHierarchy(Object.GetSelected()[0].getName(),''))[0] # somhow this returns a tuple with one list in it.
  
  # Put all data in the file we have selected file.
  file = open(filename, "w")
  file.write('HIERARCHY\n') # all bvh files have this on the first line
  
  # Write the whole hirarchy to a list
  bvhHierarchy, level, chennelList, nodeObjectList = hierarchy2bvh(hierarchy, '', 0, chennelList, nodeObjectList)
  file.write( bvhHierarchy ) # Rwite the var fileBlock to the output.
  bvhHierarchy = None # Save a tit bit of memory
  
  #====================================================#
  # MOTION: Loop through the frames ande write out     #
  # the motion data for each                           #
  #====================================================#
  # Do some basic motion file header stuff
  file.write('MOTION\n')
  file.write( 'Frames: ' + str(1 + context.endFrame() - context.startFrame()) + '\n'  )
  file.write( 'Frame Time: ' + saneFloat(frameRate) + '\n'  ) 
  
  #print 'WARNING- exact frames might be stuffed up- inclusive whatever, do some tests later on.'
  frames = range(context.startFrame(), context.endFrame()+1)
  print 'exporting ' + str(len(frames)) + ' of motion...'
  
  for frame in frames:
    context.currentFrame(frame)
    scn.update(1) # Update locations so we can write the new locations
    #Blender.Window.RedrawAll() # causes crash
    
    file.write(  motion2bvh(frame, chennelList, nodeObjectList)  )
     
  file.write('\n') # newline
  file.close()
  print 'done'
  
Blender.Window.FileSelector(saveBVH, 'Export BVH')
