#!BPY

"""
Name: 'Motion Capture (.bvh)...'
Blender: 232
Group: 'Import'
Tip: 'Import a (.bvh) motion capture file'
"""

# $Id$
#
#===============================================#
# BVH Import script 1.0 by Campbell Barton      #
# 25/03/2004, euler rotation code taken from    #
# Reevan Mckay's BVH import script v1.1         #
# if you have any questions about this script   #
# email me ideasman@linuxmail.org               #
#===============================================#

#===============================================#
# TODO:                                         #
# * Create bones when importing                 #
# * Make an IPO jitter removal script           #
# * Work out a better naming system             #
#===============================================#

# -------------------------------------------------------------------------- 
# BVH Import v0.9 by Campbell Barton (AKA Ideasman) 
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


import string
import math
import Blender
from Blender import Window, Object, Scene, Ipo
from Blender.Scene import Render


# # PSYCO IS CRASHING ON MY SYSTEM
# # Attempt to load psyco, speed things up
# try:
#   print 'using psyco to speed up BVH importing'
#   import psyco
#   psyco.full()
#  
# except:
#   print 'psyco is not present on this system'
 

# Update as we load?
debug = 0

# Global scale facctor # sHOULD BE 1 BY DEFAULT
scale = 1

# Get the current scene.
scn = Scene.GetCurrent()
context = scn.getRenderingContext()

# Here we store the Ipo curves in the order they load.
channelCurves = []

# Object list
# We need this so we can loop through the objects and edit there IPO's 
# Chenging there rotation to EULER rotation
objectList = []

def MAT(m):
	if len(m) == 3:
		return Blender.Mathutils.Matrix(m[0], m[1], m[2])
	elif len(m) == 4:
		return Blender.Mathutils.Matrix(m[0], m[1], m[2], m[3])



#===============================================#
# eulerRotation: converts X, Y, Z rotation      #
# to eular Rotation. This entire function       #
# is copied from Reevan Mckay's BVH script      #
#===============================================#
# Vars used in eular rotation funtcion
DEG_TO_RAD = math.pi/180.0
RAD_TO_DEG = 180.0/math.pi
PI=3.14159

def eulerRotate(x,y,z): 
  #=================================
  def RVMatMult3 (mat1,mat2):
  #=================================
    mat3=[[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0]]
    for i in range(3):
      for k in range(3):
        for j in range(3):
          mat3[i][k]=mat3[i][k]+mat1[i][j]*mat2[j][k]
    mat1 = mat2 = i = k = j = None # Save memory
    return mat3
  
  
  #=================================
  def	RVAxisAngleToMat3 (rot4):
  #	Takes a direction vector and
  #	a rotation (in rads) and
  #	returns the rotation matrix.
  #	Graphics Gems I p. 466:
  #=================================
    mat3=[[0.0,0.0,0.0],[0.0,0.0,0.0],[0.0,0.0,0.0]]
    if math.fabs(rot4[3])>0.01:
      s=math.sin(rot4[3])
      c=math.cos(rot4[3])
      t=1.0-math.cos(rot4[3])
    else:
      s=rot4[3]
      c=1.0
      t=0.0

    x=rot4[0]; y=rot4[1]; z=rot4[2]
    
    mat3[0][0]=t*x*x+c
    mat3[0][1]=t*x*y+s*z
    mat3[0][2]=t*x*z-s*y 
    
    mat3[1][0]=t*x*y-s*z
    mat3[1][1]=t*y*y+c
    mat3[1][2]=t*y*z+s*x
    
    mat3[2][0]=t*x*z+s*y
    mat3[2][1]=t*y*z-s*x
    mat3[2][2]=t*z*z+c
    
    rot4 = s = c = t = x = y = z = None # Save some memory
    return mat3
 
  eul = [x,y,z]
  
  for jj in range(3):
    while eul[jj] < 0:
      eul[jj] = eul[jj] + 360.0
    while eul[jj] >= 360.0:
      eul[jj] = eul[jj] - 360.0

  eul[0] = eul[0]*DEG_TO_RAD
  eul[1] = eul[1]*DEG_TO_RAD
  eul[2] = eul[2]*DEG_TO_RAD
  
  xmat=RVAxisAngleToMat3([1,0,0,eul[0]])
  ymat=RVAxisAngleToMat3([0,1,0,eul[1]])
  zmat=RVAxisAngleToMat3([0,0,1,eul[2]])
  
  mat=[[1.0,0.0,0.0],[0.0,1.0,0.0],[0.0,0.0,1.0]]  
  
  # Standard BVH multiplication order
  mat=RVMatMult3 (zmat,mat)
  mat=RVMatMult3 (xmat,mat)
  mat=RVMatMult3 (ymat,mat)
  
  
  '''
  # Screwy Animation Master BVH multiplcation order
  mat=RVMatMult3 (ymat,mat)
  mat=RVMatMult3 (xmat,mat)
  mat=RVMatMult3 (zmat,mat)
  '''
  mat = MAT(mat)
  
  eul = mat.toEuler()
  x =- eul[0]/-10
  y =- eul[1]/-10
  z =- eul[2]/-10
  

  eul = mat = zmat = xmat = ymat = jj = None
  return x, y, z # Returm euler roration values.



#===============================================#
# makeJoint: Here we use the node data          #
# from the BVA file to create an empty          #
#===============================================#
def makeJoint(name, parent, prefix, offset, channels):
  # Make Empty, with the prefix in front of the name
  ob = Object.New('Empty', prefix + name) # New object, ob is shorter and nicer to use.
  scn.link(ob) # place the object in the current scene
  
  # Offset Empty
  ob.setLocation(offset[0]*scale, offset[1]*scale, offset[2]*scale)

  # Make me a child of another empty.
  # Vale of None will make the empty a root node (no parent)
  if parent[-1] != None:
    obParent = Object.Get(prefix + parent[-1]) # We use this a bit so refrence it here.
    obParent.makeParent([ob], 0, 1) #ojbs, noninverse, 1 = not fast.

  # Add Ipo's for necessary channels
  newIpo = Ipo.New('Object', prefix + name)
  ob.setIpo(newIpo)
  for channelType in channels:
    if channelType == 'Xposition':
      newIpo.addCurve('LocX')
      newIpo.getCurve('LocX').setInterpolation('Linear')
    if channelType == 'Yposition':
      newIpo.addCurve('LocY')
      newIpo.getCurve('LocY').setInterpolation('Linear')
    if channelType == 'Zposition':
      newIpo.addCurve('LocZ')
      newIpo.getCurve('LocZ').setInterpolation('Linear')

    if channelType == 'Zrotation':
      newIpo.addCurve('RotZ')
      newIpo.getCurve('RotZ').setInterpolation('Linear')
    if channelType == 'Yrotation':
      newIpo.addCurve('RotY')
      newIpo.getCurve('RotY').setInterpolation('Linear')
    if channelType == 'Xrotation':
      newIpo.addCurve('RotX')
      newIpo.getCurve('RotX').setInterpolation('Linear')

  # Add to object list
  objectList.append(ob)
  
  ob = newIpo = opParent = None
  
  # Redraw if debugging
  if debug: Blender.Redraw()
  

#===============================================#
# makeEnd: Here we make an end node             #
# This is needed when adding the last bone      #
#===============================================#
def makeEnd(parent, prefix, offset):
  # Make Empty, with the prefix in front of the name, end nodes have no name so call it its parents name+'_end'
  ob = Object.New('Empty', prefix + parent[-1] + '_end') # New object, ob is shorter and nicer to use.
  scn.link(ob)
  
  # Dont check for a parent, an end node MUST have a parent
  obParent = Object.Get(prefix + parent[-1]) # We use this a bit so refrence it here.
  obParent.makeParent([ob], 0, 1) #ojbs, noninverse, 1 = not fast.

  # Offset Empty
  ob.setLocation(offset[0]*scale, offset[1]*scale, offset[2]*scale) 
  
  # Redraw if debugging
  if debug: Blender.Redraw()  
  


#===============================================#
# MAIN FUNCTION - All things are done from here #
#===============================================#
def loadBVH(filename):
  print ''
  print 'BVH Importer 1.0 by Campbell Barton (Ideasman) - ideasman@linuxmail.org'
    
  # File loading stuff
  # Open the file for importing
  file = open(filename, 'r')  
  fileData = file.readlines()
  # Make a list of lines
  lines = []
  for fileLine in fileData:
    newLine = string.split(fileLine)
    if newLine != []:
      lines.append(string.split(fileLine))
    fileData = None
  # End file loading code

  # Call object names with this prefix, mainly for scenes with multiple BVH's - Can imagine most partr names are the same
  # So in future
  #prefix = str(len(lines)) + '_'
  
  prefix = '_'
  
  # Create Hirachy as empties
  if lines[0][0] == 'HIERARCHY':
    print 'Importing the BVH Hierarchy for:', filename
  else:
    return 'ERROR: This is not a BVH file'
  
  # A liniar list of ancestors to keep track of a single objects heratage
  # at any one time, this is appended and removed, dosent store tree- just a liniar list.
  # ZERO is a place holder that means we are a root node. (no parents)
  parent = [None]  
  
  #channelList [(<objectName>, [channelType1, channelType2...]),  (<objectName>, [channelType1, channelType2...)]
  channelList = []
  channelIndex = -1
  
  lineIdx = 1 # An index for the file.
  while lineIdx < len(lines) -1:
    #...
    if lines[lineIdx][0] == 'ROOT' or lines[lineIdx][0] == 'JOINT':
      # MAY NEED TO SUPPORT MULTIPLE ROOT's HERE!!!, Still unsure weather multiple roots are possible.??

      print len(parent) * '  ' + 'node:',lines[lineIdx][1],' parent:',parent[-1]
      
      name = lines[lineIdx][1]
      lineIdx += 2 # Incriment to the next line (Offset)
      offset = ( eval(lines[lineIdx][1]), eval(lines[lineIdx][2]), eval(lines[lineIdx][3]) )
      lineIdx += 1 # Incriment to the next line (Channels)
      
      # newChannel[Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation]
      # newChannel has Indecies to the motiondata,
      # -1 refers to the last value that will be added on loading at a value of zero
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
      makeJoint(name, parent, prefix, offset, channels)
      
      # If we have another child then we can call ourselves a parent, else 
      parent.append(name)

    # Account for an end node
    if lines[lineIdx][0] == 'End' and lines[lineIdx][1] == 'Site': # There is somtimes a name afetr 'End Site' but we will ignore it.
      lineIdx += 2 # Incriment to the next line (Offset)
      offset = ( eval(lines[lineIdx][1]), eval(lines[lineIdx][2]), eval(lines[lineIdx][3]) )
      makeEnd(parent, prefix, offset)

      # Just so we can remove the Parents in a uniform way- End end never has kids
      # so this is a placeholder
      parent.append(None)

    if lines[lineIdx] == ['}']:
      parent = parent[:-1] # Remove the last item


    #=============================================#
    # BVH Structure loaded, Now import motion     #
    #=============================================#    
    if lines[lineIdx] == ['MOTION']:
      print '\nImporting motion data'
      lineIdx += 3 # Set the cursor to the forst frame
      
      #=============================================#
      # Loop through frames, each line a frame      #
      #=============================================#      
      currentFrame = 1
      print 'frames: ',
      
      
      #=============================================#
      # Add a ZERO keyframe, this keeps the rig     #
      # so when we export we know where all the     #
      # joints start from                           #
      #=============================================#  
      obIdx = 0
      while obIdx < len(objectList) -1:
        if channelList[obIdx][0] != -1:
          objectList[obIdx].getIpo().getCurve('LocX').addBezier((currentFrame,0))
        if channelList[obIdx][1] != -1:
          objectList[obIdx].getIpo().getCurve('LocY').addBezier((currentFrame,0))
        if channelList[obIdx][2] != -1:
          objectList[obIdx].getIpo().getCurve('LocZ').addBezier((currentFrame,0))
        if channelList[obIdx][3] != '-1' or channelList[obIdx][4] != '-1' or channelList[obIdx][5] != '-1':
          objectList[obIdx].getIpo().getCurve('RotX').addBezier((currentFrame,0))
          objectList[obIdx].getIpo().getCurve('RotY').addBezier((currentFrame,0))
          objectList[obIdx].getIpo().getCurve('RotZ').addBezier((currentFrame,0))
        obIdx += 1
      
      while lineIdx < len(lines):
        
        # Exit loop if we are past the motiondata.
        # Some BVH's have extra tags like 'CONSTRAINTS and MOTIONTAGS'
        # I dont know what they do and I dont care, they'll be ignored here.
        if len(lines[lineIdx]) < len(objectList):
          print '...ending on unknown tags'
          break
        
        
        currentFrame += 1 # Incriment to next frame
                
        #=============================================#
        # Import motion data and assign it to an IPO  #
        #=============================================#
        lines[lineIdx].append('0') # Use this as a dummy var for objects that dont have a rotate channel.
        obIdx = 0
        if debug: Blender.Redraw() 
        while obIdx < len(objectList) -1:
          if channelList[obIdx][0] != -1:
            objectList[obIdx].getIpo().getCurve('LocX').addBezier((currentFrame, scale * eval(lines[lineIdx][channelList[obIdx][0]])))
          if channelList[obIdx][1] != -1:
            objectList[obIdx].getIpo().getCurve('LocY').addBezier((currentFrame, scale * eval(lines[lineIdx][channelList[obIdx][1]])))
          if channelList[obIdx][2] != -1:
            objectList[obIdx].getIpo().getCurve('LocZ').addBezier((currentFrame, scale * eval(lines[lineIdx][channelList[obIdx][2]])))
          
          if channelList[obIdx][3] != '-1' or channelList[obIdx][4] != '-1' or channelList[obIdx][5] != '-1':
            x, y, z = eulerRotate(eval(lines[lineIdx][channelList[obIdx][3]]), eval(lines[lineIdx][channelList[obIdx][4]]), eval(lines[lineIdx][channelList[obIdx][5]]))
            objectList[obIdx].getIpo().getCurve('RotX').addBezier((currentFrame, x))
            objectList[obIdx].getIpo().getCurve('RotY').addBezier((currentFrame, y))
            objectList[obIdx].getIpo().getCurve('RotZ').addBezier((currentFrame, z))
          obIdx += 1
          # Done importing motion data #
        
        lines[lineIdx] = None # Scrap old motion data, save some memory?
        lineIdx += 1
      # We have finished now
      print currentFrame, 'done.'
     
      # No point in looking further, when this loop is done
      # There is nothine else left to do      
      print 'Imported ', currentFrame, ' frames'
      break
      
    # Main file loop
    lineIdx += 1

Blender.Window.FileSelector(loadBVH, "Import BVH")
