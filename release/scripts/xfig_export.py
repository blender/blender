#!BPY

"""
Name: 'xfig export (.fig)'
Blender: 237
Group: 'Export'
Tooltip: 'Export selected mesh to xfig Format (.fig)'
"""

__author__ = "Dino Ghilardi  "
__url__ = ("blender", "elysiun")
__version__ = "1.1"

__bpydoc__ = """\
		This script exports the selected mesh to xfig (www.xfig.org) file format (i.e.: .fig)

		The starting point of this script was Anthony D'Agostino's raw triangle format export.
		(some code is still here and there, cut'n pasted from his script)

		Usage:<br>
			Select the mesh to be exported and run this script from "File->Export" menu.
			The toggle button 'export 3 files' enables the generation of 4 files: one global
			and three with the three different views of the object.
			This script is licensed under the GPL license. (c) Dino Ghilardi, 2005
			
"""

# .fig export, mostly brutally cut-n pasted from the 
# 'Raw triangle export' (Anthony D'Agostino, http://www.redrival.com/scorpius)|

import Blender
from Blender import NMesh, Draw, BGL
from Blender.Window import DrawProgressBar
#, meshtools
import sys
#import time

# =================================
# === Write xfig Format.===
# =================================

#globals definition and init
mystring = ""
mymsg = ""
toggle=0
sel3files=0
maxX=-1000000000
maxY=-1000000000
maxZ=-1000000000
minX=minY=minZ=10000000000
boolmode=0 #0= export in inches, 1= export in cm
dimscale=float(1200) #scale due to the cm/inches select. default: inches
hidden_flag=0

space = float(2) #space between figures, in blender units.
scale= float(1200) #conversion scale to xfig units.
guiscale=float(1) #scale shown on the ruler in the GUI
#return values from gui items, just to deallocate them on exit.
guiret1=guiret2=guiret3=guiret4=guiret5=guiret6=guiret7=0

ScalePopup=0
DistancePopup=0
SpacePopup=0
#end of globals definition


def getmaxmin():
	"""Gets the max-min coordinates of the mesh"""
	
	global maxX,maxY,maxZ,minX,minY,minZ
	"""Getting the extremes of the mesh to be exported"""
	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	obj = Blender.Object.Get(objname)
	#initializing max-min find.
	# ...is there a standard python function to find those values?
	face =mesh.faces[1]
	if len(face.v)==3:
		v1,v2,v3=face.v
	if len(face.v)==4:
		v1,v2,v3,v4=face.v
	if len(face.v)==2: 
		v1,v2=face.v
	#is the next condition a nonsense for  a face? ...Anyway, to be sure....
	if len(face.v)==1:
		v1=face.v
	
	maxX,maxY,maxZ	= v1
	minX, minY, minZ = v1
	
	for face in mesh.faces:
	    #if (face.flag & Blender.NMesh.FaceFlags['ACTIVE']):	
				if len(face.v) == 3:		# triangle
					v1, v2, v3 = face.v
					x1,y1,z1 = v1.co
					x2,y2,z2 = v2.co
					x3,y3,z3 = v3.co
					maxX = max (maxX, x1, x2, x3)
					maxY = max (maxY, y1, y2, y3)
					maxZ = max (maxZ, z1, z2, z3)
					minX = min (minX, x1, x2, x3)
					minY = min (minY, y1, y2, y3)
					minZ = min (minZ, z1, z2, z3)
				elif len(face.v)==2:
					v1,v2=face.v
					x1,y1,z1 = v1.co
					x2,y2,z2 = v2.co
					maxX = max (maxX, x1, x2)
					maxY = max (maxY, y1, y2)
					maxZ = max (maxZ, z1, z2)
					minX = min (minX, x1, x2)
					minY = min (minY, y1, y2)
					minZ = min (minZ, z1, z2)
				elif len(face.v)==1:
					v1=face.v
					x1,y1,z1 = v1.co
					maxX = max (maxX, x1)
					maxY = max (maxY, y1)
					maxZ = max (maxZ, z1)
					minX = min (minX, x1)
					minY = min (minY, y1)
					minZ = min (minZ, z1)
				elif len(face.v)==4: 
					v1,v2,v3,v4=face.v
					x1,y1,z1 = v1.co
					x2,y2,z2 = v2.co
					x3,y3,z3 = v3.co
					x4,y4,z4 = v4.co
					maxX = max (maxX, x1, x2, x3, x4)
					maxY = max (maxY, y1, y2, y3, y4)
					maxZ = max (maxZ, z1, z2, z3, z4)
					minX = min (minX, x1, x2, x3, x4)
					minY = min (minY, y1, y2, y3, y4)
					minZ = min (minZ, z1, z2, z3, z4)
def xfigheader():
	global export_type
	print "#FIG 3.2  Produced by xfig version 3.2.5-alpha5"
	print "Landscape"
	print"Center"
	if boolmode==0:
		print"Inches"
	else:
		print"Metric"
	#print export_type
	print"Letter"
	print"100.00"
	print"Single"
	print "-2"
	print "1200 2"

def xytransform (face):
	"""gives the face vertexes coordinates in the xfig format/translation (view xy)"""
	v4=None
	x4=y4=z4=None
	if len(face.v)==3:
	  v1,v2,v3=face.v
  	else:
	  v1,v2,v3,v4=face.v	
	x1,y1,z1 = v1.co
	x2,y2,z2 = v2.co
	x3,y3,z3 = v3.co
	y1=-y1
	y2=-y2
	y3=-y3
	if v4 !=None:
		x4,y4,z4 = v4.co
		y4=-y4
	return x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4 

def xztransform(face):
	"""gives the face vertexes coordinates in the xfig format/translation (view xz)"""
	v4=None
	x4=y4=z4=None
	if len(face.v)==3:
	  v1,v2,v3=face.v
  	else:
	  v1,v2,v3,v4=face.v	

	#Order vertexes
	x1,y1,z1 = v1.co
	x2,y2,z2 = v2.co
	x3,y3,z3 = v3.co
	y1=-y1
	y2=-y2
	y3=-y3
	
	z1=-z1+maxZ-minY +space
	z2=-z2+maxZ-minY +space
	z3=-z3+maxZ-minY +space

	if v4 !=None:
		x4,y4,z4 = v4.co
		y4=-y4
		z4=-z4+maxZ-minY +space 
	return x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4 

def yztransform(face):
	"""gives the face vertexes coordinates in the xfig format/translation (view xz)"""
	v4=None
	x4=y4=z4=None
	if len(face.v)==3:
	  v1,v2,v3=face.v
  	else:
	  v1,v2,v3,v4=face.v	

	#Order vertexes
	x1,y1,z1 = v1.co
	x2,y2,z2 = v2.co
	x3,y3,z3 = v3.co
	y1=-y1
	y2=-y2
	y3=-y3
	z1=-(z1-maxZ-maxX-space)
	z2=-(z2-maxZ-maxX-space)
	z3=-(z3-maxZ-maxX-space)

	if v4 !=None:
		x4,y4,z4 = v4.co
		y4=-y4
		z4=-(z4-maxZ-maxX-space)
	return x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4 

def figdata(expview):
	"""Prints all the xfig data (no header)"""
	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	obj = Blender.Object.Get(objname)
	facenumber = len(mesh.faces)
	for face in mesh.faces:
			if len(face.v) == 3:		# triangle
				print "2 3 0 1 0 7 50 -1 -1 0.000 0 0 -1 0 0 4"
				if expview=="xy":
 					x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=xytransform(face)
					faceverts = int(x1*scale),int(y1*scale),int(x2*scale),int(y2*scale),int(x3*scale),int(y3*scale), int(x1*scale),int(y1*scale)
				elif expview=="xz":
					x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=xztransform(face)
					faceverts = int(x1*scale),int(z1*scale),int(x2*scale),int(z2*scale),int(x3*scale),int(z3*scale), int(x1*scale),int(z1*scale)
				elif expview=="yz":
					x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=yztransform(face)
					faceverts = int(z1*scale),int(y1*scale),int(z2*scale),int(y2*scale),int(z3*scale),int(y3*scale),int(z1*scale),int(y1*scale)
				print "\t% i % i % i % i % i % i % i % i" % faceverts
			else:
				if len(face.v) == 4: #quadrilateral	
					print "2 3 0 1 0 7 50 -1 -1 0.000 0 0 -1 0 0 5"
					if expview=="xy":
 					  x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=xytransform(face)
					  faceverts = int(x1*scale),int(y1*scale),int(x2*scale),int(y2*scale),int(x3*scale),int(y3*scale),int(x4*scale),int(y4*scale), int(x1*scale),int(y1*scale)

					elif expview=="xz":
					  x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=xztransform(face)
					  faceverts = int(x1*scale),int(z1*scale),int(x2*scale),int(z2*scale),int(x3*scale),int(z3*scale),int(x4*scale),int(z4*scale), int(x1*scale),int(z1*scale)

					elif expview=="yz":
					  x1,y1,z1,x2,y2,z2,x3,y3,z3,x4,y4,z4=yztransform(face)
					  faceverts = int(z1*scale),int(y1*scale),int(z2*scale),int(y2*scale),int(z3*scale),int(y3*scale),int(z4*scale),int(y4*scale), int(z1*scale),int(y1*scale)
					print "\t% i % i % i % i % i % i % i % i % i % i" % faceverts
				else: 
					pass #it should not occour, but....


def writexy(filename):
	"""writes the x-y view file exported"""
	global maxX, maxY, maxZ
	global minX, minY, minZ
	global space
	global scale
	#start = time.clock()
	file = open(filename, "wb")
	std=sys.stdout
	sys.stdout=file
	xfigheader()
	figdata("xy")# xydata()
	sys.stdout=std
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	print message 

def writexz(filename):
	"""writes the x-z view file exported"""
	global space,maxX,maxY,maxZ, scale
	#start = time.clock()
	file = open(filename, "wb")
	std=sys.stdout
	sys.stdout=file
	xfigheader()
	figdata("xz")#xzdata()
	sys.stdout=std
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	print message 

def writeyz(filename):
	"""writes the y-z view file exported"""
	global maxX, maxY, maxZ, minX, minY, minZ,scale
	#start = time.clock()
	file = open(filename, "wb")

	std=sys.stdout
	sys.stdout=file

	xfigheader()
  	figdata("yz")#yzdata()	
	sys.stdout=std
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	print message 

def writeall(filename):
	"""writes all 3 views
	
	Every view is a combined object in the resulting xfig. file."""
	global maxX, maxY, maxZ, minX, minY, minZ,scale
	#start = time.clock()
	file = open(filename, "wb")

	std=sys.stdout
	sys.stdout=file

	xfigheader()
	print "#upper view (7)"
	print "6 % i % i % i % i ",  minX, minY, maxX, maxY
	figdata("xy") #xydata()
	print "-6"
	print "#bottom view (1)"
	print "6 %i %i %i %i", minX, -minZ+maxZ-minY +space, maxX,-maxZ+maxZ-minY +space
	figdata ("xz") #xzdata()
  	print "-6"
	
	print "#right view (3)"
	print "6 %i %i %i %i", minX, minZ-maxZ-maxX-space, maxX,maxZ-maxZ-maxX-space
	figdata ("yz") #yzdata()	
  	print "-6"
	
	sys.stdout=std
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds


#********************************************************USER INTERFACE*****************************************************
#********************************************************USER INTERFACE*****************************************************
#********************************************************USER INTERFACE*****************************************************
def gui():              # the function to draw the screen
  global mystring, mymsg, toggle, sel3files, scale 
  global guiret1, guiret2, guiret3, guiret4, guiret5, guiret6, guiret7
  global ScalePopup, SpacePopup, boolmode, guiscale,hidden_flag
  if len(mystring) > 90: mystring = ""
  BGL.glClearColor(0,0,1,1)
  BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
  BGL.glColor3f(1,1,1)
  guiret2=Draw.PushButton("Cancel", 2, 10, 10, 55, 20,"Cancel")
  guiret3=Draw.Toggle("1 file per view",    3, 10, 40, 110,20, sel3files, "3 files")
  guiret4=Draw.PushButton("Export", 4, 70, 10, 70, 20, "Select filename and export") 

  ScalePopup=Draw.Number("Scale", 5, 10,70, 110,20, guiscale, 0.0001, 1000.1, "Scaling factor")
  SpacePopup=Draw.Number("Space", 6, 10,90, 110,20, space, 0, 10000, "Space between projections")
  
  guiret5=Draw.Toggle("cm", 7, 120,70, 40,20, boolmode, "set scale to 1 blender unit = 1 cm in xfig")
  guiret6=Draw.Toggle("in", 8, 162,70, 40,20, not boolmode, "set scale to 1 blender unit = 1 in in xfig")
#  guiret7 = guiret6=Draw.Toggle("only visible", 9, 120,90, 82,20, hidden_flag, "hidden faces")


  BGL.glRasterPos2i(72, 16)
  if toggle: toggle_state = "down"
  else: toggle_state = "up"
  #Draw.Text("The toggle button is %s." % toggle_state, "small")
  BGL.glRasterPos2i(10, 230)
  #Draw.Text("Type letters from a to z, ESC to leave.")
  BGL.glRasterPos2i(20, 200)
  Draw.Text(mystring)
  BGL.glColor3f(1,0.4,0.3)
  BGL.glRasterPos2i(340, 70)
  Draw.Text(mymsg, "tiny")


def event(evt, val):    # the function to handle input events
  Draw.Redraw(1)

def button_event(evt):  # the function to handle Draw Button events
	global toggle, guiret5,scale, space, SpacePopup, boolmode, dimscale, guiscale
	global hidden_flag, sel3files
	if evt==1:
	  toggle = 1 - toggle
	  Draw.Redraw(1)
	if evt==2:
	  Draw.Exit()
	  return
	if evt==3:
	  sel3files = 1-sel3files
	  Draw.Redraw(1)	
	if evt==4:	
	  Blender.Window.FileSelector(fs_callback, "Export fig")
	  Draw.Exit()
	  return 
	if evt==5:
	  guiscale = ScalePopup.val
	  scale=dimscale*guiscale
	  Draw.Redraw(1)
	if evt==6:
	  space =SpacePopup.val
	if evt==7: 
		boolmode=1
		dimscale=450 #converting to cm
		scale = dimscale*guiscale
		Draw.Redraw(1)
	if evt==8:
		boolmode=0
		dimscale = 1200
		scale = dimscale*guiscale
		Draw.Redraw(1)
	if evt==9:
		hidden_flag=1-hidden_flag
		Draw.Redraw(1)

Draw.Register(gui, event, button_event)  # registering the 3 callbacks
	
def fs_callback(filename):
	if filename.find('.fig', -4) > 0: filename = filename[:-4]
	getmaxmin()
	if sel3files:
		writexy(filename+"_XY.fig")
		writexz(filename+"_XZ.fig")
		writeyz(filename+"_YZ.fig")
	writeall(filename+"_ALL.fig")
	print  scale
	Draw.Exit()
