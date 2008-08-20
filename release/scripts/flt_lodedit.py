#!BPY

"""
Name: 'FLT LOD Editor'
Blender: 240
Group: 'Misc'
Tooltip: 'Level of Detail Edtior for FLT nodes'
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

#event codes
evcode = {
	"LOD_MAKE" : 100,
	"LOD_DELETE" : 101,
	"LOD_CALC_CENTER" : 102,
	"LOD_GRAB_CENTER" : 103,
	"LOD_X" : 104,
	"LOD_Y" : 105,
	"LOD_Z" : 106,
	"LOD_FREEZE" : 107,
	"LOD_SIG" : 108,
	"LOD_IN" : 109,
	"LOD_OUT" : 110,
	"LOD_TRANS" : 111,		
	"LOD_PREVIOUS" : 112
}


#system
LOD_MAKE = None			#PushButton
LOD_DELETE = None		#PushButton
LOD_CALC_CENTER = None	#PushButton
LOD_GRAB_CENTER = None	#Pushbutton
LOD_FREEZE = None		#Toggle
LOD_PREVIOUS = None		#Toggle

LOD_X = None			#Input
LOD_Y = None			#Input
LOD_Z = None			#Input

LOD_SIG = None			#Input
LOD_IN = None			#Input
LOD_OUT = None			#Input
LOD_TRANS = None		#Input

#labels
LOD_EDITLABEL = None
LOD_SWITCHLABEL = None
LOD_CENTERLABEL = None

LOD_XLABEL = None
LOD_YLABEL = None
LOD_ZLABEL = None
LOD_SIGLABEL = None
LOD_INLABEL = None
LOD_OUTLABEL = None
LOD_TRANSLABEL = None


#ID Props
switch_in = '5d!switch in'
switch_out = '6d!switch out'
xco = '10d!X co'
yco = '11d!Y co'
zco = '12d!Z co'
trans = '13d!Transition'
sig_size = '14d!Sig Size'

#Flags
lodflag = '9I!flags'
previous_mask = (1 << 31)
freeze_mask = (1 << 29)

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
	
def idprops_append(object, typecode, props):
	object.properties["FLT"] = dict()
	object.properties["FLT"]['type'] = typecode 
	for prop in props:
		object.properties["FLT"][prop] = props[prop]
	object.properties["FLT"]['3t8!id'] = object.name

def idprops_kill():
	state = update_state()
	if state["activeObject"] and state["activeObject"].properties.has_key('FLT'):
		state["activeObject"].properties.pop('FLT')

def idprops_copy(source):
	state = update_state()
	if source.properties.has_key('FLT'):
		for object in state["activeScene"].objects:
			if object.sel and object != source and (state["activeScene"].Layers & object.Layers):
				idprops_kill(object)
				object.properties['FLT'] = dict()
				for key in source.properties['FLT']:
					object.properties['FLT'][key] = source.properties['FLT'][key]

def select_by_typecode(typecode):
	state = update_state()
	
	for object in state["activeScene"].objects:
		if object.properties.has_key('FLT') and object.properties['FLT']['type'] == typecode and state["activeScene"].Layers & object.Layers:
				object.select(1)

def idprops_type(object, typecode):
	if object.properties.has_key('FLT') and object.properties['FLT'].has_key('type') and object.properties['FLT']['type'] == typecode:
		return True
	return False

#ui type code
def get_prop(typecode, prop):
	
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], typecode):
		props = state["activeObject"].properties['FLT']
	else:
		props =  flt_properties.FLTLOD
		
	return props[prop]	

def set_prop(typecode, prop, value):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"],typecode):
		state["activeObject"].properties['FLT'][prop] = value		



def get_lockmask(mask):
	global lodflag
	state = update_state()
	if state["activeObject"]:
		flag = get_prop(73,lodflag)
		if flag & mask:
			return True
	return False	

def set_lockmask(mask):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 73):
		oldvalue = state["activeObject"].properties['FLT'][lodflag]
		oldvalue = struct.unpack('>I', struct.pack('>i', oldvalue))[0]
		oldvalue |= mask
		state["activeObject"].properties['FLT'][lodflag] = struct.unpack('>i', struct.pack(">I", oldvalue))[0]

def clear_lockmask(mask):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 73):
		oldvalue = state["activeObject"].properties['FLT'][lodflag]
		oldvalue = struct.unpack('>I', struct.pack('>i', oldvalue))[0]
		oldvalue &= ~mask
		state["activeObject"].properties['FLT'][lodflag] = struct.unpack('>i',struct.pack('>I',oldvalue))[0]	

def findchildren(object):
	state = update_state()
	children = list()
	for candidate in state["activeScene"].objects:
		if candidate.parent == object:
			children.append(candidate)
	retlist = list(children)
	for child in children:
		retlist = retlist + findchildren(child)
	return retlist

def get_object_center(object):
	bbox = object.getBoundBox(1)
	average = Blender.Mathutils.Vector(0.0, 0.0, 0.0)
	
	for point in bbox:
		average[0] += point[0]
		average[1] += point[1]
		average[2] += point[2]
	
	average[0] = average[0] / 8.0
	average[1] = average[1] / 8.0
	average[2] = average[2] / 8.0
	
	return average
	

def calc_center():
	
	global xco
	global yco
	global zco
	
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 73):
		average = Blender.Mathutils.Vector(0.0, 0.0, 0.0)
		children = findchildren(state["activeObject"]) #get children objects	
		if children:
			for child in children:
				center = get_object_center(child)
				average[0] += center[0]
				average[1] += center[1]
				average[2] += center[2]
			
			average[0] = average[0] / len(children)
			average[1] = average[1] / len(children)
			average[2] = average[2] / len(children)
			
		set_prop(73, xco, average[0])
		set_prop(73, yco, average[1])
		set_prop(73, zco, average[2])
		

def grab_center():
	
	global xco
	global yco
	global zco
	
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 73):
		center = Blender.Window.GetCursorPos()
		
		set_prop(73, xco, center[0])
		set_prop(73, yco, center[1])
		set_prop(73, zco, center[2])	


def create_lod():
	state = update_state()
	actobj = state["activeObject"]
	if actobj and not idprops_type(actobj, 73):
		idprops_kill()
		idprops_append(actobj,73, flt_properties.FLTLOD)
		calc_center()

	
		
def event(evt,val):
	if evt == Draw.ESCKEY:
		Draw.Exit()
		
def but_event(evt):

	global LOD_MAKE
	global LOD_DELETE
	global LOD_CALC_CENTER
	global LOD_GRAB_CENTER
	global LOD_FREEZE
	global LOD_PREVIOUS
	global LOD_X
	global LOD_Y
	global LOD_Z
	global LOD_SIG
	global LOD_IN
	global LOD_OUT
	global LOD_TRANS
	
	global switch_in
	global switch_out
	global xco
	global yco
	global zco
	global trans
	global sig_size
	
	global lodflag
	global previous_mask
	global freeze_mask
	
	global evcode
	
	#do "system" events
	if evt == evcode["LOD_MAKE"]:
		create_lod()

	if evt == evcode["LOD_CALC_CENTER"]:
		calc_center()
	
	if evt == evcode["LOD_DELETE"]:
		idprops_kill()
	
	if evt == evcode["LOD_GRAB_CENTER"]:
		grab_center()

	#do mask events
	if evt == evcode["LOD_FREEZE"]:
		if LOD_FREEZE.val == True:
			set_lockmask(freeze_mask)
		else:
			clear_lockmask(freeze_mask)
			
	if evt == evcode["LOD_PREVIOUS"]:
		if LOD_PREVIOUS.val == True:
			set_lockmask(previous_mask)
		else:
			clear_lockmask(previous_mask)
			
	#do input events
	if evt == evcode["LOD_X"]:
		set_prop(73, xco, LOD_X.val)
	if evt == evcode["LOD_Y"]:
		set_prop(73, yco, LOD_Y.val)
	if evt == evcode["LOD_Z"]:
		set_prop(73, zco, LOD_Z.val)
	if evt == evcode["LOD_SIG"]:
		set_prop(73, sig_size, LOD_SIG.val)
	if evt == evcode["LOD_IN"]:
		set_prop(73, switch_in, LOD_IN.val)
	if evt == evcode["LOD_OUT"]:
		set_prop(73, switch_out, LOD_OUT.val)
	if evt == evcode["LOD_TRANS"]:
		set_prop(73, trans, LOD_TRANS.val)	
				

	Draw.Redraw(1)
	Blender.Window.RedrawAll()

def draw_propsheet(x,y):

	global LOD_MAKE
	global LOD_DELETE
	global LOD_CALC_CENTER
	global LOD_GRAB_CENTER
	global LOD_FREEZE
	global LOD_PREVIOUS
	global LOD_X
	global LOD_Y
	global LOD_Z
	global LOD_SIG
	global LOD_IN
	global LOD_OUT
	global LOD_TRANS

	#labels
	global LOD_EDITLABEL
	global LOD_SWITCHLABEL
	global LOD_CENTERLABEL
	global LOD_XLABEL
	global LOD_YLABEL
	global LOD_ZLABEL
	global LOD_SIGLABEL
	global LOD_INLABEL
	global LOD_OUTLABEL
	global LOD_TRANSLABEL
	
	
	global switch_in
	global switch_out
	global xco
	global yco
	global zco
	global trans
	global sig_size
	
	global lodflag
	global previous_mask
	global freeze_mask
	
	global evcode


	global evcode
	
	state = update_state()

	label_width = 100	
	row_height = 20
	toggle_width = 50
	input_width = 100
	pad = 10
	origx = x
	origy = (row_height * 16) + (pad * 16)


	#editor label
	x = origx
	y = origy
	LOD_EDITLABEL = Blender.Draw.Label("FLT Level of Detail Editor", x, y, 250, row_height)


	#Center inputs
	x = origx
	y = y- (row_height + pad)
	LOD_CENTERLABEL = Blender.Draw.Label("LOD center", x, y, label_width, row_height)
	y = y- (row_height + pad)
	LOD_XLABEL = Blender.Draw.Label("X Coordinate", x, y, label_width, row_height)
	x = origx + (label_width + pad)
	LOD_X = Blender.Draw.Number("", evcode["LOD_X"], x, y, input_width, row_height,get_prop(73,xco),  -1000000.0, 1000000.0, "")
	x = origx
	y = y- (row_height + pad)
	LOD_YLABEL = Blender.Draw.Label("Y Coordinate", x, y, label_width, row_height)	
	x = origx + (label_width + pad)
	LOD_Y = Blender.Draw.Number("", evcode["LOD_Y"], x, y, input_width, row_height,get_prop(73,yco), -1000000.0,  1000000.0, "")
	x = origx
	y = y- (row_height + pad)
	LOD_ZLABEL = Blender.Draw.Label("Z Coordinate", x, y, label_width, row_height)
	x = origx + (label_width + pad)		
	LOD_Z = Blender.Draw.Number("", evcode["LOD_Z"], x, y, input_width, row_height,get_prop(73,zco), -1000000.0, 1000000.0, "")


	#Switch inputs
	x = origx
	y = y- (row_height + pad)
	LOD_SWITCHLABEL = Blender.Draw.Label("Switch Settings", x, y, input_width, row_height)
	y = y- (row_height + pad)
	LOD_SIGLABEL = Blender.Draw.Label("Significant Size", x, y, label_width, row_height)
	x = origx + (label_width + pad)
	LOD_SIG = Blender.Draw.Number("", evcode["LOD_SIG"], x, y, input_width, row_height, get_prop(73,sig_size),  -1000000.0, 1000000.0, "")
	x = origx
	y = y- (row_height + pad)
	LOD_INLABEL = Blender.Draw.Label("Switch In", x, y, label_width, row_height)
	x = origx + (label_width + pad)
	LOD_IN = Blender.Draw.Number("", evcode["LOD_IN"], x, y, input_width, row_height, get_prop(73,switch_in), -1000000.0, 1000000.0, "")
	x = origx
	y = y- (row_height + pad)
	LOD_OUTLABEL = Blender.Draw.Label("Switch Out", x, y, label_width, row_height)	
	x = origx + (label_width + pad)
	LOD_OUT = Blender.Draw.Number("", evcode["LOD_OUT"], x, y, input_width, row_height, get_prop(73,switch_out), -1000000.0, 1000000.0, "")
	x = origx
	y = y- (row_height + pad)
	LOD_TRANSLABEL = Blender.Draw.Label("Transition", x, y, label_width, row_height)		
	x = origx + (label_width + pad)
	LOD_TRANS = Blender.Draw.Number("", evcode["LOD_TRANS"], x, y, input_width, row_height, get_prop(73,trans), -1000000.0, 1000000.0, "")	


	x = origx
	y = y - (row_height + pad)	
 	LOD_MAKE = Blender.Draw.PushButton("Make LOD", evcode["LOD_MAKE"], x, y, input_width + label_width + pad, row_height, "Make a LOD Node out of Active Object")
	y = y - (row_height + pad)	
	LOD_DELETE = Blender.Draw.PushButton("Delete LOD", evcode["LOD_DELETE"], x, y, input_width + label_width + pad, row_height, "Delete the LOD Node properties")
	y = y - (row_height + pad)
	LOD_CALC_CENTER = Blender.Draw.PushButton("Calculate Center", evcode["LOD_CALC_CENTER"], x, y, input_width + label_width + pad, row_height, "Calculate the center of this LOD")
	y = y - (row_height + pad)
	LOD_GRAB_CENTER = Blender.Draw.PushButton("Grab Center", evcode["LOD_GRAB_CENTER"], x, y, input_width + label_width + pad, row_height, "Grab center from 3d cursor")
	y = y - (row_height + pad)
	LOD_FREEZE = Blender.Draw.Toggle("Freeze Center", evcode["LOD_FREEZE"], x, y, input_width + label_width + pad, row_height, get_lockmask(freeze_mask), "")
	y = y - (row_height + pad)
	LOD_PREVIOUS = Blender.Draw.Toggle("Previous Range", evcode["LOD_PREVIOUS"], x, y, input_width + label_width + pad, row_height, get_lockmask(previous_mask), "")

def gui():
	#draw the propsheet/toolbox.
	psheety = 800
	#psheetx = psheety + 10
	draw_propsheet(20,psheety)

Draw.Register(gui,event,but_event)
	