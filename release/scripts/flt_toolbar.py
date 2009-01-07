#!BPY

"""
Name: 'FLT Toolbar'
Blender: 240
Group: 'Misc'
Tooltip: 'Tools for working with FLT databases'
"""

__author__ = "Geoffrey Bantle"
__version__ = "1.0 11/21/07"
__email__ = ('scripts', 'Author, ')
__url__ = ('blender', 'blenderartists.org')

__bpydoc__ ="""\
This script provides tools for working with OpenFlight databases in Blender. OpenFlight is a
registered trademark of MultiGen-Paradigm, Inc.

Feature overview and more availible at:
http://wiki.blender.org/index.php/Scripts/Manual/FLTools
"""

# --------------------------------------------------------------------------
# flt_palettemanager.py version 0.1 2005/04/08
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2007: Blender Foundation
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

import Blender.Draw as Draw
from Blender.BGL import *
import Blender
import flt_properties
reload(flt_properties)
from flt_properties import *

xrefprefix = ""
xrefstack = list()
vofsstack = list()
vquatstack = list()
prop_w = 256
prop_h = 256


#event codes
evcode = {
	"XREF_MAKE" : 100,
	"XREF_EDIT" : 101,
	"XREF_FILE" : 102,
	"XREF_PICK" : 103,
	"XREF_SELECT" : 104,
        "XREF_POP" : 105,
        "XREF_PREFIX" : 106,
	"FACE_NAME" : 200,
	"FACE_MAKESUB" : 201,
	"FACE_KILLSUB" : 202,
	"FACE_SELSUB" : 203,
	"SCENE_UPDATE" : 303,
	"IDPROP_COPY" : 501,
	"IDPROP_KILL" : 502,
	"CLIGHT_MAKE" : 700,
	"DFROMACT" : 701,
	"FIXCOL" : 702
}

XREF_PREFIX = None
XREF_MAKE = None
XREF_EDIT = None
XREF_SELECT = None
XREF_POP = None
FACE_MAKESUB = None
FACE_SELSUB = None
FACE_KILLSUB = None
IDPROP_KILL = None
IDPROP_COPY = None
SCENE_UPDATE = None
CLIGHT_MAKE = None
DFROMACT = None
FIXCOL = None


def RGBtoHSV( r, g, b):
	cmin = min( r, g, b )
	cmax = max( r, g, b )
	v = cmax				

	if(cmax!=0.0):
		s = (cmax-cmin)/cmax
	else:
		s = 0.0
		h = 0.0
	
	if(s == 0.0):
		h = -1.0
	else:
		cdelta = cmax-cmin
		rc = (cmax-r)/cdelta
		gc = (cmax-g)/cdelta
		bc = (cmax-b)/cdelta
		if(r==cmax):
			h = bc-gc
		else:
			if(g==cmax):
				h = 2.0+rc-bc
			else:
				h = 4.0+gc-rc
		h = h*60.0
		if(h<0.0):
			h += 360.0
			
		
	h = h/360.0
	if(h < 0.0): 
		h = 0.0
	return (h,s,v)


def update_state():
	state = dict()
	state["activeScene"] = Blender.Scene.GetCurrent()
	state["activeObject"] = state["activeScene"].objects.active
	if state["activeObject"] and not state["activeObject"].sel:
		state["activeObject"] = None
	state["activeMesh"] = None
	if state["activeObject"] and state["activeObject"].type == 'Mesh':
		state["activeMesh"] = state["activeObject"].getData(mesh=True)
	
	state["activeFace"] = None
	if state["activeMesh"]:
		if state["activeMesh"].faceUV and state["activeMesh"].activeFace != None:
			state["activeFace"] = state["activeMesh"].faces[state["activeMesh"].activeFace]
		
	#update editmode
	state["editmode"]	= Blender.Window.EditMode()

	return state
def pack_face_index(index, intensity):
	return ((127*intensity)+(128*index))
def unpack_face_index(face_index):
	index = face_index / 128
	intensity = float(face_index - 128.0 * index) / 127.0
	return(index,intensity)
	
def idprops_append(object, typecode, props):
	object.properties["FLT"] = dict()
	object.properties["FLT"]['type'] = typecode 
	for prop in props:
		object.properties["FLT"][prop] = props[prop]
	object.properties["FLT"]['3t8!id'] = object.name

def idprops_kill(object):
	state = update_state()
	if object and object.properties.has_key('FLT'):
		object.properties.pop('FLT')

def idprops_copy(source):
	state = update_state()
	if source.properties.has_key('FLT'):
		for object in state["activeScene"].objects:
			if object.sel and object != source and (state["activeScene"].Layers & object.Layers):
				idprops_kill(object)
				object.properties['FLT'] = dict()
				for key in source.properties['FLT']:
					object.properties['FLT'][key] = source.properties['FLT'][key]

def unpack_color(color):
	return struct.unpack('>BBBB',struct.pack('>I',color))


def findColorKey(colordict, hsv):
	hdelta = 0.001
	for key in colordict:
		if not (((hsv[0] < (key[0] + hdelta)) and (hsv[0] > (key[0] - hdelta))) and ((hsv[1] < (key[1] + hdelta)) and (hsv[1] > (key[1] - hdelta)))):
			return key
	return None

def hsvsort(a, b):
	(index1, mag1) = a
	(index2, mag2) = b
	if mag1 > mag2:
		return 1
	elif mag1 < mag2:
		return -1
	return 0

def fix_colors():
	
	editmode = 0
	if Blender.Window.EditMode():
		Blender.Window.EditMode(0)
		editmode = 1
	state = update_state()
	
	scene = state["activeScene"]
	colors = None
	if state["activeScene"].properties.has_key('FLT'):
		try:
			colors = state["activeScene"].properties['FLT']['Color Palette']
		except:
			pass
	if not colors:
		return
	
	#first build a HSV version of our palette
	hsvpalette = list()
	for swatch in colors:
		color = unpack_color(swatch)
		hsv = RGBtoHSV(color[0] / 255.0, color[1] / 255.0, color[2] / 255.0)
		hsvpalette.append(hsv)
		
	#collect all of our meshes
	meshes = list()
	for object in scene.objects.context:
		if object.sel and object.type == 'Mesh':
			mesh = object.getData(mesh=True)
			if "FLT_COL" in mesh.faces.properties:
				meshes.append(mesh)
				
				
	#Now go through our meshes, and build a dictionary of face lists keyed according to (hue,saturation) of the baked color
	colordict = dict()
	for mesh in meshes:
		for face in mesh.faces:
			hsv = RGBtoHSV(face.col[0].r/255.0, face.col[0].g/255.0, face.col[0].b/255.0) #retrieve baked color
			if colordict.has_key((hsv[0],hsv[1])):
				colordict[(hsv[0],hsv[1])].append(face)
			else:
				colordict[(hsv[0],hsv[1])] = [face]				
	

	#for each color key in the color dict, build a list of distances from it to the values in hsvpalette and then quicksort them for closest match
	for key in colordict:
		maglist = list()
		for i, hsv in enumerate(hsvpalette):
			norm = Blender.Mathutils.Vector(hsv[0], hsv[1]) - Blender.Mathutils.Vector(key[0],key[1])
			maglist.append((i,norm.length))
		maglist.sort(hsvsort)
		print maglist[0]
		for face in colordict[key]:
			(index, intensity) = unpack_face_index(face.getProperty("FLT_COL"))
			newfindex = pack_face_index(maglist[0][0],intensity)
			face.setProperty("FLT_COL", int(newfindex))
	
	for mesh in meshes:
		update_mesh_colors(colors,mesh)	
	
	if editmode:
		Blender.Window.EditMode(1)	
	

def update_mesh_colors(colors, mesh):
	if 'FLT_COL' in mesh.faces.properties:
		mesh.activeColorLayer = "FLT_Fcol"
		for face in mesh.faces:
			(index,intensity) = unpack_face_index(face.getProperty('FLT_COL'))
			color = struct.unpack('>BBBB',struct.pack('>I',colors[index]))
			
			if index == 0 and intensity == 0:
				color = (255,255,255)
				intensity  = 1.0
			#update the vertex colors for this face
			for col in face.col:
				col.r = int(color[0] * intensity)
				col.g = int(color[1] * intensity)
				col.b = int(color[2] * intensity)
				col.a = 255
	
		
def update_all():
	
	editmode = 0
	if Blender.Window.EditMode():
		Blender.Window.EditMode(0)
		editmode = 1
	state = update_state()
	colors = None
	if state["activeScene"].properties.has_key('FLT'):
		try:
			colors = state["activeScene"].properties['FLT']['Color Palette']
		except:
			pass
	if colors:
		#update the baked FLT colors for all meshes.
		for object in state["activeScene"].objects:
			if object.type == "Mesh":
				mesh = object.getData(mesh=True)
				update_mesh_colors(colors,mesh)
	if editmode:
		Blender.Window.EditMode(1)

#Change this to find the deep parent
def xref_create():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	state = update_state()
	
	def findchildren(object):
		children = list()
		for candidate in state["activeScene"].objects:
			if candidate.parent == object:
				children.append(candidate)
		retlist = list(children)
		for child in children:
			retlist = retlist + findchildren(child)
		return retlist
		
	actObject = state["activeObject"]
        if actObject and xrefprefix:
                scenenames = list()
                for scene in Blender.Scene.Get():
                        scenenames.append(scene.name)

                if xrefprefix in scenenames:
                        #build a unique name for the xref...
                        suffix = 1
                        found = False
                        while not found:
                                candidate = xrefprefix + str(suffix)
                                if not candidate in scenenames:
                                        xrefname = candidate
                                        found = True
                                suffix+=1
                else:
                        xrefname = xrefprefix
                #create our XRef node
                xnode = state["activeScene"].objects.new('Empty')
                xnode.name = 'X:' + xrefname
                xnode.properties['FLT'] = dict()
                for prop in FLTXRef:
                        xnode.properties['FLT'][prop] = FLTXRef[prop]
                xnode.properties['FLT']['3t200!filename'] = xrefname + '.flt'
		xnode.properties['FLT']['type'] = 63
		xnode.enableDupGroup = True
		xnode.DupGroup = Blender.Group.New(xrefname) #this is dangerous... be careful!

                #copy rot and loc of actObject
                xnode.setLocation(actObject.getLocation())
                xnode.setEuler(actObject.getEuler())         
                
                #build the new scene
		xrefscene = Blender.Scene.New(xrefname)
		xrefscene.properties['FLT'] = dict()
		xrefscene.properties['FLT']['Filename'] = xrefname
		xrefscene.properties['FLT']['Main'] = 0

                #find the children of actObject so that we can add them to the group
		linkobjects = findchildren(actObject)
                linkobjects.append(actObject)
		for object in linkobjects:
                        xrefscene.objects.link(object)
                        state["activeScene"].objects.unlink(object)
                        xnode.DupGroup.objects.link(object)
                #clear rotation of actObject and location
                actObject.setLocation(0.0,0.0,0.0)
                actObject.setEuler(0.0,0.0,0.0)

                xrefscene.update(1)
                state["activeScene"].update(1)

def xref_select():
	state = update_state()
	candidates = list()
	scenelist = [scene.name for scene in Blender.Scene.Get()]
	for object in state["activeScene"].objects:
		if object.type == 'Empty' and object.enableDupGroup == True and object.DupGroup:
			candidates.append(object)
		
	for object in candidates:
		if object.DupGroup.name in scenelist:
			object.sel = 1

def xref_edit():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	state = update_state()

	actObject = state["activeObject"]

	if actObject and actObject.type == 'Empty' and actObject.DupGroup:
#		if actObject.properties.has_key('FLT') and actObject.properties['FLT']['type'] == 63:
		for FLTscene in Blender.Scene.Get():
			if FLTscene.properties.has_key('FLT') and FLTscene.name == actObject.DupGroup.name:
				actObject.sel = 0
				xrefstack.append(state["activeScene"])
				vofsstack.append(Blender.Window.GetViewOffset())
				vquatstack.append(Blender.Window.GetViewQuat())
				FLTscene.makeCurrent()
				Blender.Window.SetViewOffset(0.0,0.0,0.0)

def xref_finish():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	state = update_state() 
        if xrefstack:
                scene = xrefstack.pop()
                Blender.Window.SetViewQuat(vquatstack.pop())
                Blender.Window.SetViewOffset(vofsstack.pop())
                scene.makeCurrent()
                

def sortSub(a,b):
	aindex = a.getProperty("FLT_ORIGINDEX")
	bindex = b.getProperty("FLT_ORIGINDEX")
	
	if aindex > bindex:
		return 1
	elif aindex < bindex:
		return -1
	return 0

def subface_make():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	editmode = 0
	if Blender.Window.EditMode():
		Blender.Window.EditMode(0)
		editmode = 1

	state = update_state()
	
	actmesh = state["activeMesh"]
	activeFace = state["activeFace"]
	if actmesh:
		if not "FLT_ORIGINDEX" in actmesh.faces.properties:
			actmesh.faces.addPropertyLayer("FLT_ORIGINDEX",Blender.Mesh.PropertyTypes["INT"])
			for i, face in enumerate(actmesh.faces):
				face.setProperty("FLT_ORIGINDEX",i)
		if not "FLT_SFLEVEL" in actmesh.faces.properties:
			actmesh.faces.addPropertyLayer("FLT_SFLEVEL",Blender.Mesh.PropertyTypes["INT"])
			
		#attach the subfaces to the active face. Note, this doesnt really work 100 percent properly yet, just enough for one level!
		if activeFace:
			#steps:
			#remove actface and selected faces from the facelist
			#quicksort facelist
			#append actface and subfaces to end of facelist.
			#generate new indices
			facelist = list()
			sublist = list()
			for face in actmesh.faces:
				facelist.append(face)
			for face in facelist:	
				if face == activeFace:
					face.setProperty("FLT_SFLEVEL",0)
					sublist.insert(0,face)
				elif face.sel:
					face.setProperty("FLT_SFLEVEL",1)
					sublist.append(face)
			for face in sublist:
				facelist.remove(face)
			facelist.sort(sortSub)
			for face in sublist:
				facelist.append(face)
			for i, face in enumerate(facelist):
				face.setProperty("FLT_ORIGINDEX",i)
		else:
			pass
	
	if editmode:
		Blender.Window.EditMode(1)

def subface_kill():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	editmode = 0
	if Blender.Window.EditMode():
		Blender.Window.EditMode(0)
		editmode = 1
	state = update_state()
	
	actmesh = state["activeMesh"]
	if actmesh:
		if "FLT_ORIGINDEX" in actmesh.faces.properties and "FLT_SFLEVEL" in actmesh.faces.properties:
			for i,face in enumerate(actmesh.faces):
				face.setProperty("FLT_ORIGINDEX",i)
				face.setProperty("FLT_SFLEVEL",0)
	if editmode:
		Blender.Window.EditMode(1)

def subface_select():
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	editmode = 0
	if Blender.Window.EditMode():
		Blender.Window.EditMode(0)
		editmode = 1
	state = update_state()
	
	actmesh = state["activeMesh"]
	activeFace = state["activeFace"]
	if actmesh and activeFace:
		if "FLT_ORIGINDEX" in actmesh.faces.properties and "FLT_SFLEVEL" in actmesh.faces.properties:
			facelist = list()
			actIndex = None
			sublevel = None
			for face in actmesh.faces:
				facelist.append(face)
				facelist.sort(sortSub)
			for i, face in enumerate(facelist):
				if face == activeFace:
					actIndex = i
					sublevel = face.getProperty("FLT_SFLEVEL")+1
					break
			leftover = facelist[actIndex+1:]
			for face in leftover:
				if face.getProperty("FLT_SFLEVEL") == sublevel:
					face.sel = 1
				else:
					break
	if editmode:
		Blender.Window.EditMode(1)

def select_by_typecode(typecode):
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	
	state = update_state()
	
	for object in state["activeScene"].objects:
		if object.properties.has_key('FLT') and object.properties['FLT']['type'] == typecode and state["activeScene"].Layers & object.Layers:
				object.select(1)
def clight_make():
	state = update_state()
	actmesh = state["activeMesh"]
	actobj = state["activeObject"]
	
	if actobj and actmesh:
		actobj.properties['FLT'] = dict()
		actobj.properties['FLT']['type'] = 111
		for prop in FLTInlineLP:
			actobj.properties['FLT'][prop] = FLTInlineLP[prop]
	
		actmesh.verts.addPropertyLayer("FLT_VCOL", Blender.Mesh.PropertyTypes["INT"])
		for v in actmesh.verts:
			v.setProperty("FLT_VCOL", 83815)
			
def dfromact():
	state = update_state()
	actobj = state["activeObject"]
	actscene = state["activeScene"]
	dof = None
	
	for object in actscene.objects.context:
		if object.sel and (object != actobj):
			if not dof:
				dof = object
			else:
				break
	
	if not dof:
		return
	
	if 'FLT' not in dof.properties:
		dof.properties['FLT'] = dict()	
	
	#Warning! assumes 1 BU == 10 meters.
	#do origin
	dof.properties['FLT']['5d!ORIGX'] = actobj.getLocation('worldspace')[0]*10.0
	dof.properties['FLT']['6d!ORIGY'] = actobj.getLocation('worldspace')[1]*10.0
	dof.properties['FLT']['7d!ORIGZ'] = actobj.getLocation('worldspace')[2]*10.0
	#do X axis
	x = Blender.Mathutils.Vector(1.0,0.0,0.0)
	x = x * actobj.getMatrix('worldspace')
	x = x * 10.0
	dof.properties['FLT']['8d!XAXIS-X'] = x[0]
	dof.properties['FLT']['9d!XAXIS-Y'] = x[1]
	dof.properties['FLT']['10d!XAXIS-Z'] = x[2]
	#do X/Y plane
	x = Blender.Mathutils.Vector(1.0,1.0,0.0)
	x.normalize()
	x = x * actobj.getMatrix('worldspace')
	x = x * 10.0
	dof.properties['FLT']['11d!XYPLANE-X'] = x[0]
	dof.properties['FLT']['12d!XYPLANE-Y'] = x[1]
	dof.properties['FLT']['13d!XZPLANE-Z'] = x[2]
	
	
	
	
	
def event(evt,val):
	if evt == Draw.ESCKEY:
		Draw.Exit()
		
def but_event(evt):
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	global evcode
	
	state = update_state()
	
	#do Xref buttons
        if evt == evcode["XREF_PREFIX"]:
                xrefprefix = XREF_PREFIX.val
	if evt == evcode["XREF_EDIT"]:
		xref_edit()
	if evt == evcode["XREF_SELECT"]:
		xref_select()
	if evt == evcode["XREF_MAKE"]:
		xref_create()
	#do scene buttons				
	if evt == evcode["SCENE_UPDATE"]:
		update_all()
	#do face buttons
	if evt == evcode["FACE_MAKESUB"]:
		subface_make()
	if evt== evcode["FACE_KILLSUB"]:
		subface_kill()
	if evt== evcode["FACE_SELSUB"]:
		subface_select()
	#common buttons
	if evt == evcode["IDPROP_KILL"]:
		if state["activeObject"]:
			idprops_kill(state["activeObject"])
        if evt == evcode["IDPROP_COPY"]:
                if state["activeObject"]:
                        idprops_copy(state["activeObject"])
        if evt == evcode["XREF_POP"]:
                xref_finish()
	if evt == evcode["CLIGHT_MAKE"]:
		clight_make()
	if evt == evcode["DFROMACT"]:
		dfromact()
	if evt == evcode["FIXCOL"]:
		fix_colors()
	Draw.Redraw(1)
	Blender.Window.RedrawAll()


def box(x,y,w,h,c,mode):
        glColor3f(c[0],c[1],c[2])
	if mode == "outline":
                glBegin(GL_LINE_LOOP)
	else:
                glBegin(GL_POLYGON)
	glVertex2i(x,y)
	glVertex2i(x+w,y)
	glVertex2i(x+w,y+h)
	glVertex2i(x,y+h)
	glEnd()

def draw_postcommon(x,y,finaly):
	global sheetlabel
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	global evcode
	
	state = update_state()
	
	width = prop_w
        height = prop_h
        
	#draw the header
	glColor3f(0.15,0.15,0.15)
	glBegin(GL_POLYGON)
	glVertex2i(x-1,y)
	glVertex2i(x+width+1,y)
	glVertex2i(x+width+1,y-25)
	glVertex2i(x-1,y-25)
	glEnd()
	glColor3f(1,1,1)
	glRasterPos2i(x,y-20)
	sheetlabel = Blender.Draw.Text("FLT Tools Panel")
	#draw the box outline
	glColor3f(0,0,0)
	glBegin(GL_LINE_LOOP)
	glVertex2i(x-1,y)
	glVertex2i(x+1+width,y)
	glVertex2i(x+1+width,finaly-1)
	glVertex2i(x-1,finaly-1)
	glEnd()
	return finaly


def draw_propsheet(x,y):
	global XREF_PREFIX
	global XREF_MAKE
	global XREF_EDIT
	global XREF_SELECT
	global XREF_POP
	global FACE_MAKESUB
	global FACE_SELSUB
	global FACE_KILLSUB
	global IDPROP_KILL
	global IDPROP_COPY
	global SCENE_UPDATE
	global DFROMACT
	global FIXCOL
		
	global CLIGHT_MAKE
	global xrefprefix
	global xrefstack
	global vofsstack
	global vquatstack
	global prop_w
	global prop_h
	global evcode
	
	state = update_state()

	width = prop_w
	height = prop_h
	origx = x
	origy = y
	
         #draw Xref tools
        y = y-20
        XREF_PREFIX = Blender.Draw.String("XRef Name:",evcode["XREF_PREFIX"],x,y,width,20,xrefprefix,18,"Xref prefix name, Actual name is generated from this")
	y = y-20
	XREF_MAKE = Blender.Draw.PushButton("Make XRef",evcode["XREF_MAKE"],x,y,width,20,"Make External Reference")
        y = y-20
	XREF_EDIT = Blender.Draw.PushButton("Edit XRef",evcode["XREF_EDIT"],x,y,width,20,"Edit External Reference")
	y = y-20
	XREF_SELECT = Blender.Draw.PushButton("Select XRefs",evcode["XREF_SELECT"],x,y,width,20,"Select External References")
        y = y - 20
        XREF_POP = Blender.Draw.PushButton("Return to previous scene",evcode["XREF_POP"],x,y,width,20,"Go up one level in xref hierarchy")

        #Draw facetools
	y = y-20
	FACE_MAKESUB = Blender.Draw.PushButton("Make Subfaces",evcode["FACE_MAKESUB"],x,y,width,20,"Make subfaces")
	y = y-20
        FACE_SELSUB = Blender.Draw.PushButton("Select Subfaces",evcode["FACE_SELSUB"],x,y,width,20,"Select subfaces")
	y = y-20
	FACE_KILLSUB = Blender.Draw.PushButton("Kill Subfaces",evcode["FACE_KILLSUB"],x,y,width,20,"Kill subfaces")

        #Draw ID Property tools
	y = y - 20
	IDPROP_KILL = Blender.Draw.PushButton("Delete ID props",evcode["IDPROP_KILL"],x,y,width,20,"Delete ID props")
	y = y - 20
	IDPROP_COPY = Blender.Draw.PushButton("Copy to selected",evcode["IDPROP_COPY"],x,y,width,20, "Copy from active to all selected")
	
	y= y - 20
	CLIGHT_MAKE = Blender.Draw.PushButton("Make Light Point", evcode["CLIGHT_MAKE"],x,y,width,20,"Create inline light points from current mesh")
        #General tools
        y = y-20
	SCENE_UPDATE = Blender.Draw.PushButton("Update All",evcode["SCENE_UPDATE"],x,y,width,20,"Update all vertex colors")
	
	y=y-20
	DFROMACT = Blender.Draw.PushButton("Dof from Active", evcode["DFROMACT"],x,y,width,20,"Get Dof origin from active object")
	y=y-20
	FIXCOL = Blender.Draw.PushButton("Fix Colors", evcode["FIXCOL"],x,y,width,20,"Fix baked FLT colors of selected meshes") 	
	draw_postcommon(origx, origy,y)
		
def gui():
	#draw the propsheet/toolbox.
	psheety = 300
	#psheetx = psheety + 10
	draw_propsheet(0,psheety)
Draw.Register(gui,event,but_event)
	