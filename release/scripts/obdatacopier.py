#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Data Copier'
Blender: 232
Group: 'Object'
Tip: 'Copy data from active object to other selected ones.'
"""

__author__ = "Jean-Michel Soler (jms), Campbell Barton (Ideasman42)"
__url__ = ("blender", "blenderartists.org",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_lampdatacopier.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.1.2"

__bpydoc__ = """\
Use "Data Copier" to copy attributes from the active object to other selected ones of
its same type.

This script is still in an early version but is already useful for copying
attributes for some types of objects like lamps and cameras.

Usage:

Select the objects that will be updated, select the object whose data will
be copied (they must all be of the same type, of course), then run this script.
Toggle the buttons representing the attributes to be copied and press "Copy".
"""

# ----------------------------------------------------------
# Object DATA copier 0.1.2
# (c) 2004 jean-michel soler
# -----------------------------------------------------------
#----------------------------------------------
# Page officielle/official page du blender python Object DATA copier:
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_lampdatacopier.htm
# Communiquer les problemes et erreurs sur:
# To Communicate problems and errors on:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
# Blender Artistic License
# http://download.blender.org/documentation/html/x21254.html
#---------------------------------------------

import Blender
from Blender import *
from Blender.Draw import *
from Blender.BGL import *


scn= Blender.Scene.GetCurrent()

type_func_method= type(dir)
type_func= type(lambda:i)
type_dict= type({})
# type_list= type([])

IGNORE_VARS = 'users', 'fakeUser', 'edges', 'faces', 'verts',  'elements'

def renew():
	scn= Blender.Scene.GetCurrent()
	act_ob= scn.objects.active
	if act_ob==None:
		return {}

	act_ob_type= act_ob.getType()
	act_ob_data= act_ob.getData(mesh=1)

	if act_ob_data==None: # Surf?
		return {}
	
	PARAM={}
	evt=4
	doc='doc' 
	
	for prop_name in dir(act_ob_data):
		if not prop_name.startswith('__') and prop_name not in IGNORE_VARS:
			# Get the type
			try:		exec 'prop_type= type(act_ob_data.%s)' % prop_name
			except:		prop_type= None
			
			if prop_type != None and prop_type not in (type_func_method, type_func, type_dict):
				
				# Now we know that the attribute can be listed in the UI Create a button and tooltip.
				
				# Tooltip
				try:
					if prop_name=='mode':
						try:
							exec "doc=str(%s.Modes)+' ; value : %s'"%( act_ob_type, str(act_ob_data.mode) )
						except:
							exec """doc= '%s'+' value = '+ str(act_ob.getData(mesh=1).%s)"""%(prop_name, prop_name) 
					elif prop_name=='type':
						try:
							exec "doc=str(%s.Types)+' ; value : %s'"%( act_ob_type, str(act_ob_data.type) )
						except:
							exec """doc= '%s'+' value = '+ str(act_ob.getData(mesh=1).%s)"""%(prop_name, prop_name) 
					else:
						exec """doc= '%s'+' value = '+ str(act_ob_data.%s)"""%(prop_name, prop_name)
						if doc.find('built-in')!=-1:
							exec """doc= 'This is a function ! Doc = '+ str(act_ob_data.%s.__doc__)"""% prop_name
				except:    
					 doc='Doc...' 
				
				# Button
				PARAM[prop_name]= [Create(0), evt, doc]
				evt+=1

	return PARAM

def copy():
	global PARAM
	
	scn= Blender.Scene.GetCurrent()
	act_ob= scn.getActiveObject()
	if act_ob==None:
		Blender.Draw.PupMenu('Error|No Active Object.')
		return
	
	act_ob_type= act_ob.getType()
	
	if act_ob_type in ('Empty', 'Surf'):
		Blender.Draw.PupMenu('Error|Copying Empty or Surf object data isnt supported.')
		return   
	
	act_ob_data= act_ob.getData(mesh=1)
	
	print '\n\nStarting copy for object "%s"' % act_ob.name
	some_errors= False
	for ob in scn.objects.context:
		if ob != act_ob and ob.getType() == act_ob_type:
			ob_data= None
			for prop_name, value in PARAM.iteritems():
				if value[0].val==1:
					
					# Init the object data if we havnt alredy
					if ob_data==None:
						ob_data= ob.getData(mesh=1)
					
					try:
						exec "ob_data.%s = act_ob_data.%s"%(prop_name, prop_name) 
					except:
						some_errors= True
						print 'Cant copy property "%s" for type "%s"' % (prop_name, act_ob_type)
	if some_errors:
		Blender.Draw.PupMenu('Some attributes could not be copied, see console for details.')
	
PARAM= renew()

def EVENT(evt,val):
   pass

def BUTTON(evt):
	global PARAM   
	if (evt==1):
		Exit()

	if (evt==2):
		copy()
		Blender.Redraw()

	if (evt==3):
		PARAM= renew()
		Blender.Redraw()

def DRAW():
	global PARAM
	
	scn= Blender.Scene.GetCurrent()
	act_ob= scn.objects.active
	
	glColor3f(0.7, 0.7, 0.7)
	glClear(GL_COLOR_BUFFER_BIT)
	glColor3f(0.1, 0.1, 0.15)    

	size=Buffer(GL_FLOAT, 4)
	glGetFloatv(GL_SCISSOR_BOX, size)
	size= size.list
	for s in [0,1,2,3]: size[s]=int(size[s])
	ligne=20

	Button("Exit",1,20,4,80,ligne)
	Button("Copy",2,102,4,80,ligne)
	Button("Renew",3,184,4,80,ligne)

	glRasterPos2f(20, ligne*2-8)
	if act_ob:
		Text(act_ob.getType()+" DATA copier")
	else:
		Text("Please select an object")


	max=size[3] / 22 -2
	pos   = 0
	decal = 20
	key=PARAM.keys()
	key.sort()
	for p in key:
		if  pos==max:
			decal+=102
			pos=1
		else:
			pos+=1       
		
		PARAM[p][0]=Toggle(p,
			PARAM[p][1],
			decal,
			pos*22+22,
			100,
			20, 
			PARAM[p][0].val,
			str(PARAM[p][2]))

  
Register(DRAW,EVENT,BUTTON)
