#!BPY

"""
Name: 'Shape Widget Wizard'
Blender: 238
Group: 'Animation'
Tip: 'Adds Widgets for Driven Shapes'
"""

__author__ = ["Johnny Matthews (guitargeek)"]
__url__ = ("blender", "blenderartists.org")
__version__ = "0.0.9 12/15/05"

__bpydoc__ = """\
"Shape Widget Wizard" creates objects that drive shape channels.

Explanation:

Shapes define morph targets and sometimes it is helpful to animate with a GUI
control panel of widgets. This script lets you define several different types
of controls that (depending on the type) control 1 to 4 shapes with a single 
controller.

Usage:

1. Click where you want the widget to go<br>
2. Highlight the object that has shapes<br>
3. Run the script<br>
4. Choose the type of widget (there are next and back buttons if you pick the wrong kind)<br>
5. Click next and choose what shapes go where on the widget<br>
6. Choose a display name for the widget<br>
7. Click finish

The widget is added and you are returned to the first screen for adding another widget.

"""

###################################################################
#                                                                 #
# Shape Widget Wizard                                             #
#                                                                 #
# all versions (C) December 2005 Johnny Matthews (guitargeek)     #
#                                                                 #
# Released under the GPL							              #
#                                                                 #
# Works in Blender 2.4 and higher                                 #
#                                                                 #
# This script can be found online at:                             #
# http://guitargeek.superihost.com/widgetmaker                    #
#                                                                 #
# email: johnny.matthews@gmail.com                                #
###################################################################
# History                                                         #
# 0.9                                                             #
#	  Added Name Objects										  #
# 0.81															  #
#     Added Single Shape Toggle									  #
#																  #
# 0.8															  #
#     Controller is Transform Locked and can only move 		      #
#     in appropriate directions                                   #
#																  #
# 0.7                                                             #
#     Controller is named the same as the range + ".ctrl"         #
#                                                                 #
###################################################################

import Blender
import bpy
from Blender import Mesh,Object,Material,Window,IpoCurve,Ipo,Text3d
from Blender.BGL import *
from Blender.Draw import *
print "----------------------"

SHAPE1_ONE_MONE  = 1 
SHAPE1_ONE_ZERO  = 2
SHAPE1_ZERO_MONE = 3
SHAPE1_TOGGLE	 = 12
SHAPE2_EXCLUSIVE = 4
SHAPE2_V         = 5
SHAPE2_T         = 6
SHAPE2_INVT      = 7
SHAPE2_PLUS      = 8
SHAPE3_T         = 9
SHAPE3_INVT      = 10
SHAPE4_X         = 11


stage = 1
numshapes = Create(1)
widmenu = Create(1)
rangename  = Create("Range")
shapes  = [Create(0),Create(0),Create(0),Create(0)]
drawtype = 0


#get rid of an ipo curve by deleting all its points
def delCurve(ipo):
	while len(ipo.bezierPoints) > 0:
		ipo.delBezier(0)
		ipo.recalc()
	
#if a given ipocurve is not there create it, otherwise get it
def verifyIpocurve(ky,index):
	ipo = ky.ipo
	if ipo == None:
		nip = bpy.data.ipos.new("keyipo", "Key")
		ky.ipo = nip
	ipo = ky.ipo
	if index == 0:
		idx = "Basis"
	else:
		idx = "Key " + str(index)
	crv = ipo[idx]
	if crv == None:
		# print idx
		crv = ipo.addCurve(idx)
	crv.interpolation = IpoCurve.InterpTypes.LINEAR
	return crv

# Add the Drivers and Curves	
def setupDrivers(ob,ctrl,type):    
	global shapes
	me = ob.getData(mesh=1)	
	ky = me.key
	
	# Should we add an error here??
	if not ky:
		return
	
	if type in [SHAPE1_ONE_MONE,SHAPE1_ONE_ZERO,SHAPE1_ZERO_MONE]:
		ctrl.protectFlags = int("111111011",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z	
		ipo.recalc()

		delCurve(ipo)		
		if type == 1:
			ipo.append((-1,-1))
			ipo.append((0,0))
			ipo.append((1,1))
		if type == 2:
			ipo.append((0,0))
			ipo.append((1,1))
		if type == 3:
			ipo.append((-1,-1))
			ipo.append((0,0))
		ipo.recalc()	

	if type == SHAPE1_TOGGLE:
		ctrl.protectFlags = int("111111011",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((0.5,0))
		ipo.append((0.500001,1))		
		ipo.append((1,1))
		ipo.recalc()
			
	if type == SHAPE2_EXCLUSIVE:
		ctrl.protectFlags = int("111111011",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((1,1))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_Z
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,1))
		ipo2.append((0,0))
		ipo2.recalc()

	if type == SHAPE2_T:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((-1,-1))
		ipo.append((0,0))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,-1))
		ipo2.append((1,1))
		ipo2.recalc()

	if type == SHAPE2_INVT:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((1,1))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,-1))
		ipo2.append((1,1))
		ipo2.recalc()

	if type == SHAPE2_PLUS:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((-1,-1))
		ipo.append((1,1))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,-1))
		ipo2.append((1,1))
		ipo2.recalc()
				
	if type == SHAPE2_V: # 2 Shape Mix
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((1,1))
		ipo.recalc()		
		
		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		delCurve(ipo2)
		ipo2.append((0,0))
		ipo2.append((1,1))
		ipo2.recalc()


	if type == SHAPE3_INVT:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((1,1))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,1))
		ipo2.append((0,0))
		ipo2.recalc()

		ipo2 = verifyIpocurve(ky,shapes[2].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((0,0))
		ipo2.append((1,1))
		ipo2.recalc()

	if type == SHAPE3_T:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		ipo.recalc()
		delCurve(ipo)
		ipo.append((-1,-1))
		ipo.append((0,0))
		ipo.recalc()

		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((-1,1))
		ipo2.append((0,0))
		ipo2.recalc()

		ipo2 = verifyIpocurve(ky,shapes[2].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		ipo2.recalc()
		delCurve(ipo2)
		ipo2.append((0,0))
		ipo2.append((1,1))
		ipo2.recalc()
		
	if type == SHAPE4_X:
		ctrl.protectFlags = int("111111010",2)
		ipo = verifyIpocurve(ky,shapes[0].val)
		ipo.driver = 1
		ipo.driverObject = ctrl
		ipo.driverChannel = IpoCurve.LOC_Z
		delCurve(ipo)
		ipo.append((0,0))
		ipo.append((1,1))	
		ipo.recalc()
			
		ipo2 = verifyIpocurve(ky,shapes[1].val)
		ipo2.driver = 1
		ipo2.driverObject = ctrl
		ipo2.driverChannel = IpoCurve.LOC_X
		delCurve(ipo2)
		ipo2.append((0,0))
		ipo2.append((1,1))
		ipo2.recalc()
		
		ipo3 = verifyIpocurve(ky,shapes[2].val)
		ipo3.driver = 1
		ipo3.driverObject = ctrl
		ipo3.driverChannel = IpoCurve.LOC_X
		delCurve(ipo3)
		ipo3.append((-1,1))
		ipo3.append((0,0))
		ipo3.recalc()
			
		ipo4 = verifyIpocurve(ky,shapes[3].val)
		ipo4.driver = 1
		ipo4.driverObject = ctrl
		ipo4.driverChannel = IpoCurve.LOC_Z
		delCurve(ipo4)
		ipo4.append((-1,1))
		ipo4.append((0,0))
		ipo4.recalc()

#The Main Call to Build the Widget
		
def build(type):
	global shapes,widmenu,rangename
	sce = bpy.data.scenes.active
	ob = sce.objects.active
	
	try:
		ob.getData(mesh=1).key
	except:
		Blender.Draw.PupMenu('Aborting%t|Object has no keys')
		return
	
	loc = Window.GetCursorPos() 
	range	   = makeRange(sce, type,rangename.val)
	controller = makeController(sce, rangename.val)
	text       = makeText(sce, rangename.val)
	
	range.restrictRender = True
	controller.restrictRender = True
	text.restrictRender = True
	
	range.setLocation(loc)
	controller.setLocation(loc)
	text.setLocation(loc)
	
	range.makeParent([controller],1)
	range.makeParent([text],0)

	sce.update()
	
	setupDrivers(ob,controller,widmenu.val)
	
#Create the text

def makeText(sce, name):
	txt = bpy.data.curves.new(name+'.name', 'Text3d') 
	
	txt.setDrawMode(Text3d.DRAW3D)
	txt.setAlignment(Text3d.MIDDLE)
	txt.setText(name)
	ob = sce.objects.new(txt)
	ob.setEuler((3.14159/2,0,0))
	return ob
	

#Create the mesh controller

def makeController(sce, name):
	me = bpy.data.meshes.new(name+".ctrl")
	ob = sce.objects.new(me)
	me.verts.extend([\
		(-0.15,0,    0),\
		(    0,0, 0.15),\
		( 0.15,0,    0),\
		(    0,0,-0.15)])
	
	me.edges.extend([(0,1,2,3)])
	return ob	

#Create the mesh range

def makeRange(sce,type,name):
	#ob.setDrawMode(8)  # Draw Name
	me = bpy.data.meshes.new(name)	
	ob = sce.objects.new(me)

	if type == SHAPE1_ONE_ZERO:
		me.verts.extend([\
			(-0.15,0,0),\
			( 0.15,0,0),\
			(-0.15,0,1),\
			( 0.15,0,1),\
			(-0.25,0,.1),\
			(-0.25,0,-.10),\
			(0.25,0,.1),\
			(0.25,0,-0.10)])
		
		me.edges.extend([(0,1,3,2),(4,5,0),(6,7,1)])

	elif type == SHAPE1_TOGGLE:
		me.verts.extend([\
			(-0.15,0,-0.5),\
			( 0.15,0,-0.5),\
			( 0.15,0, 0.5),\
			(-0.15,0, 0.5),\
			(-0.15,0, 1.5),\
			( 0.15,0, 1.5)])
		
		me.edges.extend([(0,1,2,3),(3,4,5,2)])
		
	elif type == SHAPE1_ZERO_MONE:
		me.verts.extend([\
			(-0.15,0,0),\
			( 0.15,0,0),\
			(-0.15,0,-1),\
			( 0.15,0,-1),\
			(-0.25,0,.1),\
			(-0.25,0,-.10),\
			(0.25,0,.1),\
			(0.25,0,-0.10)])
		
		me.edges.extend([(0,1,3,2),(4,5,0),(6,7,1)])
		
	elif type in [SHAPE1_ONE_MONE,SHAPE2_EXCLUSIVE]:
		me.verts.extend([\
			(-0.15,0,-1),\
			( 0.15,0,-1),\
			(-0.15,0,1),\
			( 0.15,0,1),\
			(-0.25,0,.1),\
			(-0.25,0,-.10),\
			(0.25,0,.1),\
			(0.25,0,-0.10),\
			(-0.15,0,0),\
			( 0.15,0,0)])
		
		l = [(0,1,3,2),(4,5,8),(6,7,9)]
		me.edges.extend(l)

	elif type == SHAPE2_T:
		me.verts.extend([\
			(-1,0,0),\
			( 1,0,0),\
			( 1,0,-1),\
			(-1,0,-1)])
		
		me.edges.extend([(0,1,2,3)])

	elif type == SHAPE2_INVT:
		me.verts.extend([\
			(-1,0,0),\
			( 1,0,0),\
			( 1,0,1),\
			(-1,0,1)])
		
		me.edges.extend([(0,1,2,3)])

	elif type == SHAPE2_PLUS:
		me.verts.extend([\
			(-1,0,-1),\
			( 1,0,-1),\
			( 1,0,1),\
			(-1,0,1)])
		me.edges.extend([(0,1,2,3)])
		
	elif type == SHAPE2_V:
		me.verts.extend([\
			(0,0,0),\
			(1,0,0),\
			(1,0,1),\
			(0,0,1)])
		
		me.edges.extend([(0,1,2,3)])
		ob.setEuler((0,-0.78539,0))

	elif type == SHAPE3_INVT:
		me.verts.extend([\
			(-1,0,0),\
			( 1,0,0),\
			( 1,0,1),\
			(-1,0,1)])
		
		me.edges.extend([(0,1,2,3)])
		
	elif type == SHAPE3_T:
		me.verts.extend([\
			(-1,0,0),\
			( 1,0,0),\
			( 1,0,-1),\
			(-1,0,-1)])
		
		me.edges.extend([(0,1,2,3)])

	
	elif type == SHAPE4_X:
		me.verts.extend([\
			(0,0,-1),\
			(1,0,-1),\
			(1,0,0),\
			(1,0,1),\
			(0,0,1),\
			(-1,0,1),\
			(-1,0,0),\
			(-1,0,-1)])
		
		me.edges.extend([(0,1),(1,2),(2,3),(3,4),(4,5),(5,6),(6,7),(7,0)])
		ob.setEuler((0,-0.78539,0))
	
	return ob


def create():
	main()

####################### gui ######################


EVENT_NONE 			= 1
EVENT_EXIT 			= 100
EVENT_WIDGET_MENU 	= 101
EVENT_NEXT 			= 102
EVENT_BACK 			= 103

#get the list of shapes from the selected object

def shapeMenuText():
	ob = bpy.data.scenes.active.objects.active
	if not ob:
		return ""
	
	me = ob.getData(mesh=1)
	try:	key= me.key
	except:	key = None
	
	if key == None:
		return ""
	
	blocks = key.blocks
	menu = "Choose Shape %t|"
	for i, block in enumerate(blocks):
		menu = menu + block.name + " %x" + str(i) + "|"
	return menu


#draw the widget for the gui

def drawWidget(type):
	glColor3f(0.0,0.0,0.0)
	global shapes
	if type == SHAPE1_ONE_MONE:# 1 to -1 Single Shape
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(170,50)
		glVertex2i(170,150)
		glVertex2i(150,150)		
		glVertex2i(150,50)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,100)
		glVertex2i(190,100)
		glEnd()
		glRasterPos2d(180,140) 
		Text("1","normal")		
		glRasterPos2d(180,60) 
		Text("-1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 190, 100, 100, 18, shapes[0].val, "Choose Shape.")
	elif type == SHAPE1_TOGGLE:# Toggle Single Shape
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(170,50)
		glVertex2i(170,100)
		glVertex2i(150,100)	
		glVertex2i(150,50)	
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(170,100)
		glVertex2i(170,150)
		glVertex2i(150,150)
		glVertex2i(150,100)		
		glEnd()	
		glRasterPos2d(180,140) 
		Text("On","normal")		
		glRasterPos2d(180,60) 
		Text("Off","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 190, 100, 100, 18, shapes[0].val, "Choose Shape.")
	elif type == SHAPE1_ONE_ZERO: # 1 to 0 Single Shape
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(170,50)
		glVertex2i(170,150)
		glVertex2i(150,150)		
		glVertex2i(150,50)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,50)
		glVertex2i(190,50)
		glEnd()
		glRasterPos2d(180,140) 
		Text("1","normal")		
		glRasterPos2d(180,60) 
		Text("0","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 190, 100, 100, 18, shapes[0].val, "Choose Shape.")	
	elif type == SHAPE1_ZERO_MONE:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(170,50)
		glVertex2i(170,150)
		glVertex2i(150,150)		
		glVertex2i(150,50)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,150)
		glVertex2i(190,150)
		glEnd()
		glRasterPos2d(180,140) 
		Text("0","normal")		
		glRasterPos2d(180,60) 
		Text("-1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 190, 100, 100, 18, shapes[0].val, "Choose Shape.")
	elif type == SHAPE2_EXCLUSIVE:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(170,50)
		glVertex2i(170,150)
		glVertex2i(150,150)		
		glVertex2i(150,50)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,100)
		glVertex2i(190,100)
		glEnd()
		glRasterPos2d(180,140) 
		Text("1","normal")		
		glRasterPos2d(180,60) 
		Text("1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 195, 135, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 195, 52,  100, 18, shapes[1].val, "Choose Shape 2.")
	elif type == SHAPE2_T:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,75)
		glVertex2i(250,75)
		glVertex2i(250,125)
		glVertex2i(150,125)		
		glVertex2i(150,75)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,125)
		glVertex2i(260,125)
		glEnd()
		glRasterPos2d(200,140) 
		Text("0","normal")			
		glRasterPos2d(200,60) 
		Text("-1","normal")		
		glRasterPos2d(250,140) 
		Text("1","normal")	
		glRasterPos2d(150,140) 
		Text("-1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 220, 52, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 260, 135,  100, 18, shapes[1].val, "Choose Shape 2.")
	elif type == SHAPE2_INVT:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,75)
		glVertex2i(250,75)
		glVertex2i(250,125)
		glVertex2i(150,125)		
		glVertex2i(150,75)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,75)
		glVertex2i(260,75)
		glEnd()
		glRasterPos2d(200,60) 
		Text("0","normal")	
		glRasterPos2d(200,140) 
		Text("1","normal")		
		glRasterPos2d(250,60) 
		Text("1","normal")	
		glRasterPos2d(150,60) 
		Text("-1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 220, 135, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 260, 52,  100, 18, shapes[1].val, "Choose Shape 2.")
	elif type == SHAPE2_PLUS:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,50)
		glVertex2i(250,50)
		glVertex2i(250,150)
		glVertex2i(150,150)		
		glVertex2i(150,50)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,100)
		glVertex2i(260,100)
		glEnd()
		glRasterPos2d(200,105) 
		Text("0","normal")	
		glRasterPos2d(200,140) 
		Text("1","normal")		
		glRasterPos2d(200,55) 
		Text("-1","normal")
		glRasterPos2d(250,105) 
		Text("1","normal")	
		glRasterPos2d(150,105) 
		Text("-1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 220, 155, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 260, 100,  100, 18, shapes[1].val, "Choose Shape 2.")
	elif type == SHAPE2_V:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,70)
		glVertex2i(185,105)
		glVertex2i(150,141)
		glVertex2i(115,105)		
		glVertex2i(150,70)
		glEnd()
		glRasterPos2d(110,105) 
		Text("1","normal")		
		glRasterPos2d(190,105) 
		Text("1","normal")	
		glRasterPos2d(150,80) 
		Text("0","normal")
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 20, 125, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 195, 125,  100, 18, shapes[1].val, "Choose Shape 2.")



	elif type == SHAPE3_T:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,75)
		glVertex2i(250,75)
		glVertex2i(250,125)
		glVertex2i(150,125)		
		glVertex2i(150,75)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,125)
		glVertex2i(260,125)
		glEnd()
		glRasterPos2d(200,140) 
		Text("0","normal")	
		glRasterPos2d(200,60) 
		Text("-1","normal")		
		glRasterPos2d(250,140) 
		Text("1","normal")	
		glRasterPos2d(150,140) 
		Text("1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 220, 52, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE,  45, 135,  100, 18, shapes[1].val, "Choose Shape 2.")
		shapes[2] = Menu(shapeMenuText(), EVENT_NONE, 260, 135,  100, 18, shapes[2].val, "Choose Shape 3.")
	elif type == SHAPE3_INVT:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,75)
		glVertex2i(250,75)
		glVertex2i(250,125)
		glVertex2i(150,125)		
		glVertex2i(150,75)
		glEnd()
		glBegin(GL_LINE_STRIP)
		glVertex2i(140,75)
		glVertex2i(260,75)
		glEnd()
		glRasterPos2d(200,60) 
		Text("0","normal")	
		glRasterPos2d(200,140) 
		Text("1","normal")		
		glRasterPos2d(250,60) 
		Text("1","normal")	
		glRasterPos2d(150,60) 
		Text("1","normal")	
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 220, 135, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE,  45, 52,  100, 18, shapes[1].val, "Choose Shape 2.")
		shapes[2] = Menu(shapeMenuText(), EVENT_NONE, 260, 52,  100, 18, shapes[2].val, "Choose Shape 3.")


	elif type == SHAPE4_X:
		glBegin(GL_LINE_STRIP)
		glVertex2i(150,70)
		glVertex2i(185,105)
		glVertex2i(150,141)
		glVertex2i(115,105)		
		glVertex2i(150,70)
		glEnd()
		glRasterPos2d(120,125) 
		Text("1","normal")		
		glRasterPos2d(180,125) 
		Text("1","normal")	
		glRasterPos2d(120,80) 
		Text("1","normal")		
		glRasterPos2d(180,80) 
		Text("1","normal")
		
		glRasterPos2d(145,105) 
		Text("0","normal")
		shapes[0] = Menu(shapeMenuText(), EVENT_NONE, 10, 125, 100, 18, shapes[0].val, "Choose Shape 1.")
		shapes[1] = Menu(shapeMenuText(), EVENT_NONE, 195, 125,  100, 18, shapes[1].val, "Choose Shape 2.")
		shapes[2] = Menu(shapeMenuText(), EVENT_NONE, 10, 60, 100, 18, shapes[2].val, "Choose Shape 3.")
		shapes[3] = Menu(shapeMenuText(), EVENT_NONE, 195, 60,  100, 18, shapes[3].val, "Choose Shape 4.")

#the gui callback

def draw():
	global widmenu,numshapes,stage,type, shapes,rangename
	#glRasterPos2d(5,200) 
	#Text("Shape Widget Wizard","large")
	Label("Shape Widget Wizard", 5,200, 200, 12)
	
	PushButton("Quit", EVENT_EXIT, 5, 5, 50, 18) 

	if stage == 1:
		name = "Choose Widget Type %t|\
1 Shape: 1 / -1 %x"  +str(SHAPE1_ONE_MONE) +"|\
1 Shape: 1,0 %x"     +str(SHAPE1_ONE_ZERO) +"|\
1 Shape: 0,-1 %x"    +str(SHAPE1_ZERO_MONE)+"|\
1 Shape: Toggle %x"  +str(SHAPE1_TOGGLE)   +"|\
2 Shape Exclusive %x"+str(SHAPE2_EXCLUSIVE)+"|\
2 Shape - V %x"      +str(SHAPE2_V)        +"|\
2 Shape - T %x"      +str(SHAPE2_T)        +"|\
2 Shape - Inv T %x"  +str(SHAPE2_INVT)     +"|\
2 Shape - + %x"      +str(SHAPE2_PLUS)     +"|\
3 Shape - T %x"      +str(SHAPE3_T)        +"|\
3 Shape - Inv T%x"   +str(SHAPE3_INVT)     +"|\
4 Shape - Mix %x"    +str(SHAPE4_X)
		widmenu = Menu(name, EVENT_NONE, 5, 120, 200, 40, widmenu.val, "Choose Widget Type.")
		PushButton("Next", EVENT_NEXT, 5, 25, 50, 18) 

	elif stage == 2:
		glRasterPos2d(60,140) 
		rangename = String("Name: ", EVENT_NONE, 5, 170, 200, 18, rangename.val, 50, "Name for Range Object") 
		drawWidget(widmenu.val)	
		BeginAlign()
		PushButton("Back",   EVENT_BACK, 5, 25, 50, 18, "Choose another shape type")
		PushButton("Finish", EVENT_NEXT, 55, 25, 50, 18, "Add Objects at the cursor location")
		EndAlign()
	return	 



def event(evt, val):	
	if (evt == QKEY and not val): 
		Exit()


def bevent(evt):
	global widmenu,stage,drawtype
	######### Manages GUI events
	if evt==EVENT_EXIT: 
		Exit()
	elif evt==EVENT_BACK:
		if stage == 2:
			stage = 1
			Redraw()
	elif evt==EVENT_NEXT: 
		if stage == 1:
			stage = 2
			Redraw()
		elif stage == 2:
			build(widmenu.val)
			stage = 1
			Window.RedrawAll()
		
		
Register(draw, event, bevent)
