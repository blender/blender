#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Trajectory'
Blender: 243
Group: 'Animation'
Tip: 'See Trajectory of selected object'
"""

__author__ = '3R - R3gis'
__version__ = '2.43'
__url__ = ["Script's site , http://blenderfrance.free.fr/python/Trajectory_en.htm","Author's site , http://cybercreator.free.fr", "French Blender support forum, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender"]
__email__=["3R, r3gis@free.fr"]


__bpydoc__ = """

Usage:

* Launch with alt+P (or put it in .script folder)

Allow to see in real time trajectory of selected object.

On first run, it ask you
- If you want that actually selected object have they trajectory always shown
- If you want to use Space Handler or a Scriptlink in Redraw mode
- Future and Past : it is the frame in past and future
of the beggining and the end of the path
- Width of line that represent the trajectory

Then the object's trajectory will be shown in all 3D areas.
When trajectory is red, you can modifiy it by moving object.
When trajectory is blue and you want to be able to modify it, inser a Key (I-Key)

Points appears on trajectory :
- Left Clic to modify position
- Right Clic to go to the frame it represents

Notes:<br>
In scriptlink mode, it create one script link so make sure that 'Enable Script Link' toogle is on
In SpaceHandler mode, you have to go in View>>SpaceHandlerScript menu to activate Trajectory


"""


# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004-2006: Regis Montoya
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------
#################################
# by 3R - 26/08/05
# for any problem :
#	r3gis@free.fr
#	ou sur le newsgroup:
# http://zoo-logique.org/3D.Blender/
#################################
#Many thanks to cambo for his fixes
#################################



import Blender


scene= Blender.Scene.GetCurrent()

	
#Writing
def write_script(name, script):
	global scene
	#List texts and their name
	#write : type of writing : 1->New, 2->Overwrite
	scripting= None
	for text in Blender.Text.Get():
		if text.name==name and text.asLines()[1] != "#"+str(__version__):
			scripting = text
			scripting.clear()
			scripting.write(script)
			break
	
	if not scripting:
		scripting= Blender.Text.New(name)
		scripting.write(script)

def link_script(name, type):
	global scene
	scriptlinks = scene.getScriptLinks(type) # none or list
	if not scriptlinks or name not in scriptlinks:
		scene.addScriptLink(name, type)


#Deleting of a text
def text_remove(name):
	global scene
	#try to delete text if already linked
	try:
		text= Blender.Text.Get(name)
		# Texte.clear()
		scene.clearScriptLinks([name])
		Blender.Text.unlink(text)
	except:
		print('---Initialisation of Trajectory_'+str(__version__)+'.py---')

#Whether is already running, also check if it's the last version of the script : second line contain the version fo the script
ask_modif= 0 # Default
for text in Blender.Text.Get():
	if text.name == 'Trajectory' and text.asLines()[1] == "#"+str(__version__):
		#We ask if script modify his seetings, keep it or stop script
		ask_modif= Blender.Draw.PupMenu("Script already launch %t|Modify settings%x0|Keep settings%x1|Stop script%x2|")
		if ask_modif==-1: # user canceled.
			ask_modif= 1 
		break

selection_mode= 0
future= 35
past= 20
width= 2

#In modify case
if ask_modif==0:
	handle_mode= Blender.Draw.Create(0)
	selection_mode= Blender.Draw.Create(0)
	future= Blender.Draw.Create(35)
	past= Blender.Draw.Create(20)
	width= Blender.Draw.Create(2)

	block= []
	block.append(("Space Handlers", handle_mode, "You have to activate for each area by View>>SpaceHandler")) #You can delete this option...
	block.append(("Always Draw", selection_mode, "Selected object will have their trajectory always shown"))
	block.append(("Past :", past, 1, 900))
	block.append(("Futur:", future, 1, 900))
	block.append(("Width:", width, 1,5))
	
	if not Blender.Draw.PupBlock("Trajectory seetings", block):
		ask_modif=1
	
	handle_mode= handle_mode.val
	selection_mode= selection_mode.val
	future= future.val
	past= past.val
	width= width.val


#put names of selected objects in objects_select if option choosen by user
if selection_mode==1:
	objects_select= [ob.name for ob in scene.objects.context]
else:
	objects_select= []
	

try:
	if handle_mode==1:
		DrawPart="#SPACEHANDLER.VIEW3D.DRAW\n"
	else:
		DrawPart="#!BPY\n"
except:DrawPart="#BadlyMade"
	

#Here is the script to write in Blender and to link, options are also written now
DrawPart=DrawPart+"#"+str(__version__)+"""
#This script is a part of Trajectory.py and have to be linked to the scene in Redraw if not in HANDLER mode.
#Author : 3R - Regis Montoya
#It's better to use the Trajectory_"version_number".py
#You can modify the two following value to change the path settings
future="""+str(future)+"""
past="""+str(past)+"""
object_init_names="""+str(objects_select)+"""


import Blender, math
from Blender import BGL, Draw, Ipo
from Blender.BGL import *
from Blender.Draw import *
from math import *

from Blender.Mathutils import Vector

#take actual frame
frameC=Blender.Get('curframe')
scene = Blender.Scene.GetCurrent()
render_context=scene.getRenderingContext()
#ajust number of frames with NewMap and OldMapvalue values
k=1.00*render_context.oldMapValue()/render_context.newMapValue()
if k<1:
	tr=-1*int(log(k*0.1, 10))
else:
	tr=-1*int(log(k, 10))
#The real and integer frame to compare to ipos keys frames
frameCtr=round(frameC*k, tr)
frameCr=frameC*k
frameC=int(round(frameC*k, 0))


#List objects that we have to show trajectory in $objects
# In this case, using a dict for unique objects is the fastest way.
object_dict= dict([(ob.name, ob) for ob in scene.objects.context])
for obname in object_init_names:
	if not object_dict.has_key(obname):
		try: # Object may be removed.
			object_dict[obname]= Blender.Object.Get(obname)
		except:
			pass # object was removed.

#This fonction give the resulting matrix of all parents at a given frame
#parent_list is the list of all parents [object, matrix, locX_ipo, locY, Z, rotX, Y, Z, sizeX, Y, Z] of current object
def matrixForTraj(frame, parent_list):
	DecMatC=Blender.Mathutils.Matrix([1,0,0,0], [0,1,0,0], [0,0,1,0], [0,0,0,1])

	for parent_data in parent_list:
		parent_ob=	parent_data[0]
		
		try:	X=	parent_data[5][frame]*pi/18
		except:	X=	parent_ob.RotX
		try:	Y=	parent_data[6][frame]*pi/18
		except:	Y=	parent_ob.RotY
		try:	Z=	parent_data[7][frame]*pi/18
		except: Z=	parent_ob.RotZ
		try:	LX=	parent_data[2][frame]
		except: LX=	parent_ob.LocX
		try:	LY=	parent_data[3][frame]
		except: LY=	parent_ob.LocY
		try:	LZ=	parent_data[4][frame]
		except:	LZ=	parent_ob.LocZ
		try:	SX=	parent_data[8][frame]
		except:	SX=	parent_ob.SizeX
		try:	SY=	parent_data[9][frame]
		except:	SY=	parent_ob.SizeY
		try:	SZ=	parent_data[10][frame]
		except:	SZ=	parent_ob.SizeZ

		NMat=Blender.Mathutils.Matrix([cos(Y)*cos(Z)*SX,SX*cos(Y)*sin(Z),-SX*sin(Y),0],
		[(-cos(X)*sin(Z)+sin(Y)*sin(X)*cos(Z))*SY,(sin(X)*sin(Y)*sin(Z)+cos(X)*cos(Z))*SY,sin(X)*cos(Y)*SY,0],
		[(cos(X)*sin(Y)*cos(Z)+sin(X)*sin(Z))*SZ,(cos(X)*sin(Y)*sin(Z)-sin(X)*cos(Z))*SZ,SZ*cos(X)*cos(Y),0],
		[LX,LY,LZ,1])
		DecMatC=DecMatC*parent_data[1]*NMat
	return DecMatC

#####
TestLIST=[]
matview=Blender.Window.GetPerspMatrix()
###########
#Fonction to draw trajectories
###########

def Trace_Traj(ob):
		global TestLIST, matview
		#we draw trajectories for all objects in list
	
		LocX=[]
		LocY=[]
		LocZ=[]
		#List with trajectories' vertexs
		vertexX=[]
		
		contextIpo= ob.ipo
		if contextIpo:
			ipoLocX=contextIpo[Ipo.OB_LOCX]
			ipoLocY=contextIpo[Ipo.OB_LOCY]
			ipoLocZ=contextIpo[Ipo.OB_LOCZ]
			ipoTime=contextIpo[Ipo.OB_TIME]
		else: # only do if there is no IPO (if no ipo curves : return None object and don't go in this except)
			ipoLocX= ipoLocY= ipoLocZ= ipoTime= None
		
		if ipoTime:
			return 0
		
		#Get all parents of ob
		parent=ob.parent
		backup_ob= ob
		child= ob
		parent_list= []
		
		#Get parents's infos :
		#list of [name, initial matrix at make parent, ipo in X,Y,Z,rotX,rotY,rotZ,sizeX,Y,Z]
		while parent:
			Init_Mat=Blender.Mathutils.Matrix(child.getMatrix('worldspace')) #must be done like it (it isn't a matrix otherwise)
			Init_Mat.invert()
			Init_Mat=Init_Mat*child.getMatrix('localspace')
			Init_Mat=parent.getMatrix()*Init_Mat
			Init_Mat.invert()
	
			contextIpo= parent.ipo # None or IPO
			if contextIpo:
				ipo_Parent_LocX=contextIpo[Ipo.OB_LOCX]
				ipo_Parent_LocY=contextIpo[Ipo.OB_LOCY]
				ipo_Parent_LocZ=contextIpo[Ipo.OB_LOCZ]
				ipo_Parent_RotX=contextIpo[Ipo.OB_ROTX]
				ipo_Parent_RotY=contextIpo[Ipo.OB_ROTY]
				ipo_Parent_RotZ=contextIpo[Ipo.OB_ROTZ]
				ipo_Parent_SizeX=contextIpo[Ipo.OB_SIZEX]
				ipo_Parent_SizeY=contextIpo[Ipo.OB_SIZEY]
				ipo_Parent_SizeZ=contextIpo[Ipo.OB_SIZEZ]
			else:
				ipo_Parent_LocX=ipo_Parent_LocY=ipo_Parent_LocZ=\
				ipo_Parent_RotX=ipo_Parent_RotY=ipo_Parent_RotZ=\
				ipo_Parent_SizeX=ipo_Parent_SizeY=ipo_Parent_SizeZ= None
			
			parent_list.append([parent, Init_Mat, ipo_Parent_LocX, ipo_Parent_LocY, ipo_Parent_LocZ, ipo_Parent_RotX, ipo_Parent_RotY, ipo_Parent_RotZ, ipo_Parent_SizeX, ipo_Parent_SizeY, ipo_Parent_SizeZ])
	
			child=parent
			parent=parent.parent
			
		#security : if one of parents object are a path>>follow : trajectory don't work properly so it have to draw nothing
		for parent in parent_list:
			if parent[0].type == 'Curve':
				if parent[0].data.flag & 1<<4: # Follow path, 4th bit
					return 1
		
		#ob >> re-assign obj and not parent
		ob= backup_ob
		ob= backup_ob
		
		
		if ipoLocX: LXC= ipoLocX[frameC]
		else: 		LXC= ob.LocX
		if ipoLocY:	LYC= ipoLocY[frameC]
		else:		LYC= ob.LocY
		if ipoLocZ:	LZC= ipoLocZ[frameC]
		else:		LZC= ob.LocZ

		vect= Vector([ob.LocX, ob.LocY, ob.LocZ, 1])
		color=[0, 1]	
	
		#If trajectory is being modified and we are at a frame where a ipo key already exist
		if round(ob.LocX, 5)!=round(LXC, 5):
			for bez in ipoLocX.bezierPoints:
				if round(bez.pt[0], tr)==frameCtr:
					bez.pt = [frameCr, vect[0]]
			ipoLocX.recalc()
		if round(ob.LocY, 5)!=round(LYC, 5):
			for bez in ipoLocY.bezierPoints:
				if round(bez.pt[0], tr)==frameCtr:
					bez.pt = [frameCr, vect[1]]
			ipoLocY.recalc()
		if round(ob.LocZ, 5)!=round(LZC, 5):
			for bez in ipoLocZ.bezierPoints:
				if round(bez.pt[0], tr)==frameCtr:
					bez.pt = [frameCr, vect[2]]
			ipoLocZ.recalc()
		
		#change trajectory color if at an ipoKey
		VertexFrame=[]
		bezier_Coord=0
		if ipoLocX: # FIXED like others it was just in case ipoLocX==None
			for bez in ipoLocX.bezierPoints:
				bezier_Coord=round(bez.pt[0], tr)
				if bezier_Coord not in VertexFrame:
					VertexFrame.append(bezier_Coord)
				if bezier_Coord==frameCtr:
						color=[1, color[1]-0.3]
		if ipoLocY: # FIXED
			for bez in ipoLocY.bezierPoints:
				bezier_Coord=round(bez.pt[0], tr)
				if bezier_Coord not in VertexFrame:
					VertexFrame.append(bezier_Coord)
				if round(bez.pt[0], tr)==frameCtr:
						color=[1, color[1]-0.3]
		if ipoLocZ: # FIXED
			for bez in ipoLocZ.bezierPoints:
				bezier_Coord=round(bez.pt[0], tr)
				if bezier_Coord not in VertexFrame:
					VertexFrame.append(bezier_Coord)
				if round(bez.pt[0], tr)==frameCtr:
						color=[1, color[1]-0.3]
		
	
		#put in LocX, LocY and LocZ all points of trajectory
		for frame in xrange(frameC-past, frameC+future):
			DecMat=matrixForTraj(frame, parent_list)

			if ipoLocX: LX= ipoLocX[frame]
			else: 		LX= ob.LocX
			if ipoLocY:	LY= ipoLocY[frame]
			else:		LY= ob.LocY
			if ipoLocZ:	LZ= ipoLocZ[frame]
			else:		LZ= ob.LocZ
			
			vect=Vector(LX, LY, LZ)*DecMat
			LocX.append(vect[0])
			LocY.append(vect[1])
			LocZ.append(vect[2])
	
		
		#draw part : get current view
		MatPreBuff= [matview[i][j] for i in xrange(4) for j in xrange(4)]
			
		MatBuff=BGL.Buffer(GL_FLOAT, 16, MatPreBuff)
		
		glLoadIdentity()
		glMatrixMode(GL_PROJECTION)
		glPushMatrix()
		glLoadMatrixf(MatBuff)
		
		#draw trajectory line
		glLineWidth("""+str(width)+""")
		
		glBegin(GL_LINE_STRIP)
		for i in xrange(len(LocX)):
			glColor3f((i+1)*1.00/len(LocX)*color[0], 0, (i+1)*1.00/len(LocX)*color[1])
			glVertex3f(LocX[i], LocY[i], LocZ[i])
		
		glEnd()	
		
		#draw trajectory's "vertexs"
		if not Blender.Window.EditMode():
			glPointSize(5)
			glBegin(GL_POINTS)
			TestPOINTS=[]
			TestFRAME=[]
			i=0
			for frame in VertexFrame:
				ix=int(frame)-frameC+past
				if ix>=0 and ix<len(LocX):
					glColor3f(1, 0.7, 0.2)
					glVertex3f(LocX[ix], LocY[ix], LocZ[ix])
					TestPOINTS.append(Vector([LocX[ix], LocY[ix], LocZ[ix], 1]))
					TestFRAME.append(int(frame))
					i+=1
			glEnd()
			#this list contains info about where to check if we click over a "vertex" in 3D view
			TestLIST.append((ob, TestPOINTS, TestFRAME))
		
		glLineWidth(1)
		return 0


for ob in object_dict.itervalues():
	Trace_Traj(ob)

###########
#Fonction to handle trajectories
###########

def Manip():
	#use TestLIST and matview defined by Trace_Traj
	global TestLIST, matview
	for screen in Blender.Window.GetScreenInfo(Blender.Window.Types.VIEW3D):
		if screen['id']==Blender.Window.GetAreaID():
			x0, y0, x1, y1= screen['vertices']
			break
	
	#Projection of GL matrix in 3D view
	glPushMatrix()
	glMatrixMode(GL_PROJECTION)
	glPushMatrix()
	glLoadIdentity()
	#Global coordinates' matrix
	glOrtho(x0, x1, y0, y1, -1, 0)
	glMatrixMode(GL_MODELVIEW)
	glLoadIdentity()
	#Test mouse clics and other events
	
	
	if Blender.Window.QTest():
		evt, val= Blender.Window.QRead()
		if (evt==LEFTMOUSE or evt==RIGHTMOUSE) and not Blender.Window.EditMode():
			mouse_co=Blender.Window.GetMouseCoords()
			#if click on trajectory "vertexs"...
			for ob, TestPOINTS, TestFRAME in TestLIST: # ob is now used, line 552 to know what object it had to select
				for k, Vect in enumerate(TestPOINTS):
					proj=Vect*matview
					
					pt=[(proj[0]/proj[3])*(x1-x0)/2+(x1+x0)/2, (proj[1]/proj[3])*(y1-y0)/2+(y1+y0)/2]

					if mouse_co[0]<pt[0]+4 and mouse_co[0]>pt[0]-4 and mouse_co[1]>pt[1]-4 and mouse_co[1]<pt[1]+4:
						if evt==LEFTMOUSE:
							#remember current selected object
							object_names=[obj.name for obj in Blender.Object.GetSelected()]
							#this script allow to simulate a GKey, but I have to write a script
							#another way would made a infinit redraw or don't allow to move object
							#it auto unlink and delete itself
							script=\"\"\"
import Blender
from Blender import Draw, Window
from Blender.Window import *
from Blender.Draw import *

from Blender.Mathutils import Vector

# The following code is a bit of a hack, it allows clicking on the points and dragging directly
#It simulate user press GKey 
#It also set the cursor position at center (because user have previously clic on area and moved the cursor): 
#And I can't get previous cursor position : redraw appear after it has been moved
#If there is no better way you can remove this comments
f= GetAreaID()
SetCursorPos(0,0,0)
#SetKeyQualifiers(1) #FIXED : the bug in older versions seems to have been fixed
SetKeyQualifiers(0)
QAdd(f, Blender.Draw.GKEY, 1, 0)
QHandle(f)
Blender.Redraw()
done=0
while not done:
	while Blender.Window.QTest():
		ev=Blender.Window.QRead()[0]
		if ev not in (4, 5, 18, 112, 213): #all event needed to move object
			#SetKeyQualifiers(1) #FIXED too, same reason that above
			#SetKeyQualifiers(0)
			SetKeyQualifiers(Blender.Window.GetKeyQualifiers())
			QAdd(f, ev, 1, 0)
			QHandle(f)
			Blender.Redraw()
		if ev in (RIGHTMOUSE, LEFTMOUSE, ESCKEY):
			done=1
Blender.Set('curframe',\"\"\"+str(Blender.Get('curframe'))+\"\"\")
Blender.Object.GetSelected()[0].sel= False
for obname in \"\"\"+str(object_names)+\"\"\":
	ob=Blender.Object.Get(obname)
	ob.sel= True
SetCursorPos(0,0,0)
scripting=Blender.Text.Get('Edit_Trajectory')
scripting.clear()
Blender.Text.unlink(scripting)
						\"\"\"
							
							#FIXED Edit_Trajectory was longer : all SetKeyQualifiers removed
							scene=Blender.Scene.GetCurrent()
							try:
								scripting=Blender.Text.Get('Edit_Trajectory')
								scripting.clear()
							except:
								scripting=Blender.Text.New('Edit_Trajectory')
							
							scripting.write(script)
							#script= scripting #FIXED seems not needed anymore
							
							#Go to frame that correspond to selected "vertex"
							Blender.Set('curframe', TestFRAME[k])
							
							scene.objects.selected = [] #un select all objects
							
							#FIXED TestLIST[j][0].sel=0, but no j. So ob.sel and above variable changed in obj
							ob.sel= True
							Blender.Run('Edit_Trajectory')
						
						#work well now !!!
						if evt==RIGHTMOUSE :
							Blender.Set('curframe', TestFRAME[k])

Manip()
#retrieve a normal matrix
glPopMatrix()
glMatrixMode(GL_PROJECTION)
glPopMatrix()
glMatrixMode(GL_MODELVIEW)
"""

if ask_modif==0:
	text_remove('Trajectory')
	write_script('Trajectory', DrawPart)
	if handle_mode==1:
		Blender.UpdateMenus()
	else:
		link_script('Trajectory', 'Redraw')
if ask_modif==2:
	text_remove('Trajectory')
	print("---End of Trajectory_"+str(__version__)+".py---\n---   Thanks for use   ---")
