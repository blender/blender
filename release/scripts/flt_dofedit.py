#!BPY

"""
Name: 'FLT DOF Editor'
Blender: 240
Group: 'Misc'
Tooltip: 'Degree of Freedom editor for FLT nodes'
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
	"DOF_MAKE" : 100,
	"DOF_UPDATE" : 138,
	"DOF_DELETE" : 101,
	"DOF_TRANSX" : 102,
	"DOF_TRANSY" : 103,
	"DOF_TRANSZ" : 104,
	"DOF_ROTX" : 105,
	"DOF_ROTY" : 106,
	"DOF_ROTZ" : 107,
	"DOF_SCALEX" : 108,
	"DOF_SCALEY" : 109,
	"DOF_SCALEZ" : 110,
	"DOF_MIN_TRANSX" : 111,
	"DOF_MIN_TRANSY" : 112,
	"DOF_MIN_TRANSZ" : 113,
	"DOF_MIN_ROTX" : 114,
	"DOF_MIN_ROTY" : 115,
	"DOF_MIN_ROTZ" : 116,
	"DOF_MIN_SCALEX" : 117,
	"DOF_MIN_SCALEY" : 118,
	"DOF_MIN_SCALEZ" : 119,
	"DOF_MAX_TRANSX" : 120,
	"DOF_MAX_TRANSY" : 121,
	"DOF_MAX_TRANSZ" : 122,
	"DOF_MAX_ROTX" : 123,
	"DOF_MAX_ROTY" : 124,
	"DOF_MAX_ROTZ" : 125,
	"DOF_MAX_SCALEX" : 126,
	"DOF_MAX_SCALEY" : 127,
	"DOF_MAX_SCALEZ" : 128,
	"DOF_STEP_TRANSX" : 129,
	"DOF_STEP_TRANSY" : 130,
	"DOF_STEP_TRANSZ" : 131,
	"DOF_STEP_ROTX" : 132,
	"DOF_STEP_ROTY" : 133,
	"DOF_STEP_ROTZ" : 134,
	"DOF_STEP_SCALEX" : 135,
	"DOF_STEP_SCALEY" : 136,
	"DOF_STEP_SCALEZ" : 137
}

#system
DOF_MAKE = None
DOF_UPDATE = None
DOF_DELETE = None

#toggle buttons
DOF_TRANSX = None
DOF_TRANSY = None
DOF_TRANSZ = None
DOF_ROTX = None
DOF_ROTY = None
DOF_ROTZ = None
DOF_SCALEX = None
DOF_SCALEY = None
DOF_SCALEZ = None

#Minimums
DOF_MIN_TRANSX = None
DOF_MIN_TRANSY = None
DOF_MIN_TRANSZ = None
DOF_MIN_ROTX = None
DOF_MIN_ROTY = None
DOF_MIN_ROTZ = None
DOF_MIN_SCALEX = None
DOF_MIN_SCALEY = None
DOF_MIN_SCALEZ = None

#maximums
DOF_MAX_TRANSX = None
DOF_MAX_TRANSY = None
DOF_MAX_TRANSZ = None
DOF_MAX_ROTX = None
DOF_MAX_ROTY = None
DOF_MAX_ROTZ = None
DOF_MAX_SCALEX = None
DOF_MAX_SCALEY = None
DOF_MAX_SCALEZ = None

#step
DOF_STEP_TRANSX = None
DOF_STEP_TRANSY = None
DOF_STEP_TRANSZ = None
DOF_STEP_ROTX = None
DOF_STEP_ROTY = None
DOF_STEP_ROTZ = None
DOF_STEP_SCALEX = None
DOF_STEP_SCALEY = None
DOF_STEP_SCALEZ = None

#labels
DOF_ROTSTRING = None
DOF_TRANSTRING = None
DOF_SCALESTRING = None
DOF_EDITLABEL = None

#make ID props easier/morereadable
zmin = '14d!ZMIN'
zmax = '15d!ZMAX'
zcur = '16d!ZCUR'
zstep = '17d!ZSTEP'
ymin = '18d!YMIN'
ymax = '19d!YMAX'
ycur = '20d!YCUR'
ystep = '21d!YSTEP'
xmin = '22d!XMIN'
xmax = '23d!XMAX'
xcur = '24d!XCUR'
xstep = '25d!XSTEP'
pitchmin = '26d!PITCH-MIN'
pitchmax = '27d!PITCH-MAX'
pitchcur = '28d!PITCH-CUR'
pitchstep = '29d!PITCH-STEP'
rollmin = '30d!ROLL-MIN'
rollmax = '31d!ROLL-MAX'
rollcur = '32d!ROLL-CUR'
rollstep = '33d!ROLL-STEP'
yawmin = '34d!YAW-MIN'
yawmax = '35d!YAW-MAX'
yawcur = '36d!YAW-CUR'
yawstep = '37d!YAW-STEP'
zscalemin = '38d!ZSIZE-MIN'
zscalemax = '39d!ZSIZE-MAX'
zscalecur = '40d!ZSIZE-CUR'
zscalestep = '41d!ZSIZE-STEP'
yscalemin = '42d!YSIZE-MIN'
yscalemax = '43d!YSIZE-MAX'
yscalecur = '44d!YSIZE-CUR'
yscalestep = '45d!YSIZE-STEP'
xscalemin = '46d!XSIZE-MIN'
xscalemax = '47d!XSIZE-MAX'
xscalecur = '48d!XSIZE-CUR'
xscalestep = '49d!XSIZE-STEP'



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

def DOF_get_frame():
	state = update_state()
	
	if not state["activeObject"] and not id_props_type(state["activeObject"], 14):
		return
	
	#Warning! assumes 1 BU == 10 meters.
	#do origin
	state["activeObject"].properties['FLT']['5d!ORIGX'] = state["activeObject"].getLocation('worldspace')[0]*10.0
	state["activeObject"].properties['FLT']['6d!ORIGY'] = state["activeObject"].getLocation('worldspace')[1]*10.0
	state["activeObject"].properties['FLT']['7d!ORIGZ'] = state["activeObject"].getLocation('worldspace')[2]*10.0
	#do X axis
	x = Blender.Mathutils.Vector(1.0,0.0,0.0)
	x = x * state["activeObject"].getMatrix('worldspace')
	x = x * 10.0
	state["activeObject"].properties['FLT']['8d!XAXIS-X'] = x[0]
	state["activeObject"].properties['FLT']['9d!XAXIS-Y'] = x[1]
	state["activeObject"].properties['FLT']['10d!XAXIS-Z'] = x[2]
	#do X/Y plane
	x = Blender.Mathutils.Vector(0.0,1.0,0.0)
	x.normalize()
	x = x * state["activeObject"].getMatrix('worldspace')
	x = x * 10.0
	state["activeObject"].properties['FLT']['11d!XYPLANE-X'] = x[0]
	state["activeObject"].properties['FLT']['12d!XYPLANE-Y'] = x[1]
	state["activeObject"].properties['FLT']['13d!XZPLANE-Z'] = x[2]

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
		props =  flt_properties.FLTDOF
		
	return props[prop]	

def set_prop(typecode, prop, value):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"],typecode):
		state["activeObject"].properties['FLT'][prop] = value		

lockxtrans = (1 << 31)
lockytrans = (1 << 30)
lockztrans = (1 << 29)
lockxrot = (1 << 28)
lockyrot = (1 << 27)
lockzrot = (1 << 26)
lockxscale = (1 << 25)
lockyscale = (1 << 24)
lockzscale = (1 << 23)

def get_lockmask(mask):	
	state = update_state()
	if state["activeObject"]:
		flag = get_prop(14,'50I!FLAG')
		if flag & mask:
			return True
	return False	

def set_lockmask(mask):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 14):
		oldvalue = state["activeObject"].properties['FLT']['50I!FLAG']
		oldvalue = struct.unpack('>I', struct.pack('>i', oldvalue))[0]
		oldvalue |= mask
		state["activeObject"].properties['FLT']['50I!FLAG'] = struct.unpack('>i', struct.pack(">I", oldvalue))[0]

def clear_lockmask(mask):
	state = update_state()
	if state["activeObject"] and idprops_type(state["activeObject"], 14):
		oldvalue = state["activeObject"].properties['FLT']['50I!FLAG']
		oldvalue = struct.unpack('>I', struct.pack('>i', oldvalue))[0]
		oldvalue &= ~mask
		state["activeObject"].properties['FLT']['50I!FLAG'] = struct.unpack('>i',struct.pack('>I',oldvalue))[0]	

	
def create_dof():
	state = update_state()
	actobj = state["activeObject"]
	if actobj and not idprops_type(actobj, 14):
		idprops_kill()
		idprops_append(actobj,14, flt_properties.FLTDOF)
		DOF_get_frame()
	
		
def event(evt,val):
	if evt == Draw.ESCKEY:
		Draw.Exit()
		
def but_event(evt):
	global DOF_MAKE
	global DOF_UPDATE
	global DOF_DELETE

	global DOF_TRANSX
	global DOF_TRANSY
	global DOF_TRANSZ
	global DOF_ROTX
	global DOF_ROTY
	global DOF_ROTZ
	global DOF_SCALEX
	global DOF_SCALEY
	global DOF_SCALEZ

	global DOF_MIN_TRANSX
	global DOF_MIN_TRANSY
	global DOF_MIN_TRANSZ
	global DOF_MIN_ROTX
	global DOF_MIN_ROTY
	global DOF_MIN_ROTZ
	global DOF_MIN_SCALEX
	global DOF_MIN_SCALEY
	global DOF_MIN_SCALEZ

	global DOF_MAX_TRANSX
	global DOF_MAX_TRANSY
	global DOF_MAX_TRANSZ
	global DOF_MAX_ROTX
	global DOF_MAX_ROTY
	global DOF_MAX_ROTZ
	global DOF_MAX_SCALEX
	global DOF_MAX_SCALEY
	global DOF_MAX_SCALEZ

	global DOF_STEP_TRANSX
	global DOF_STEP_TRANSY
	global DOF_STEP_TRANSZ
	global DOF_STEP_ROTX
	global DOF_STEP_ROTY
	global DOF_STEP_ROTZ
	global DOF_STEP_SCALEX
	global DOF_STEP_SCALEY
	global DOF_STEP_SCALEZ
	
	#labels
	global DOF_ROTSTRING
	global DOF_TRANSTRING
	global DOF_SCALESTRING
	
	
	#masks
	global lockxtrans
	global lockytrans
	global lockztrans
	global lockxrot
	global lockyrot
	global lockzrot
	global lockxscale
	global lockyscale
	global lockzscale

	global zmin
	global zmax
	global zcur
	global zstep
	global ymin
	global ymax
	global ycur
	global ystep
	global xmin
	global xmax
	global xcur
	global xstep
	global pitchmin
	global pitchmax
	global pitchcur
	global pitchstep
	global rollmin
	global rollmax
	global rollcur
	global rollstep
	global yawmin
	global yawmax
	global yawcur
	global yawstep
	global zscalemin
	global zscalemax
	global zscalecur
	global zscalestep
	global yscalemin
	global yscalemax
	global yscalecur
	global yscalestep
	global xscalemin
	global xscalemax
	global xscalecur
	global xscalestep	

	
	
	#do "system" events
	if evt == evcode["DOF_MAKE"]:
		create_dof()

	if evt == evcode["DOF_UPDATE"]:
		DOF_get_frame()
	
	if evt == evcode["DOF_DELETE"]:
		idprops_kill()
	#do translation lock events
	if evt == evcode["DOF_TRANSX"]:
		if DOF_TRANSX.val == True:
			set_lockmask(lockxtrans)
		else:
			clear_lockmask(lockxtrans)

	if evt == evcode["DOF_TRANSY"]:
		if DOF_TRANSY.val == True:
			set_lockmask(lockytrans)
		else:
			clear_lockmask(lockytrans)

	if evt == evcode["DOF_TRANSZ"]:
		if DOF_TRANSZ.val == True:
			set_lockmask(lockztrans)
		else:
			clear_lockmask(lockztrans)


	#do rotation lock events
	if evt == evcode["DOF_ROTX"]:
		if DOF_ROTX.val == True:
			set_lockmask(lockxrot)
		else:
			clear_lockmask(lockxrot)

	if evt == evcode["DOF_ROTY"]:
		if DOF_ROTY.val == True:
			set_lockmask(lockyrot)
		else:
			clear_lockmask(lockyrot)

	if evt == evcode["DOF_ROTZ"]:
		if DOF_ROTZ.val == True:
			set_lockmask(lockzrot)
		else:
			clear_lockmask(lockzrot)

	#do scale lock events
	if evt == evcode["DOF_SCALEX"]:
		if DOF_SCALEX.val == True:
			set_lockmask(lockxscale)
		else:
			clear_lockmask(lockxscale)

	if evt == evcode["DOF_SCALEY"]:
		if DOF_SCALEY.val == True:
			set_lockmask(lockyscale)
		else:
			clear_lockmask(lockyscale)

	if evt == evcode["DOF_SCALEZ"]:
		if DOF_SCALEZ.val == True:
			set_lockmask(lockzscale)
		else:
			clear_lockmask(lockzscale)
			
	
	#do translation buttons
	if evt == evcode["DOF_MIN_TRANSX"]:
		set_prop(14, xmin, DOF_MIN_TRANSX.val)
	if evt == evcode["DOF_MAX_TRANSX"]:
		set_prop(14,xmax, DOF_MAX_TRANSX.val)
	if evt == evcode["DOF_STEP_TRANSX"]:
		set_prop(14,xstep, DOF_STEP_TRANSX.val)
		
	if evt == evcode["DOF_MIN_TRANSY"]:
		set_prop(14, ymin, DOF_MIN_TRANSY.val)
	if evt == evcode["DOF_MAX_TRANSY"]:
		set_prop(14,ymax, DOF_MAX_TRANSY.val)
	if evt == evcode["DOF_STEP_TRANSY"]:
		set_prop(14,ystep, DOF_STEP_TRANSY.val)
		
	if evt == evcode["DOF_MIN_TRANSZ"]:
		set_prop(14, zmin, DOF_MIN_TRANSZ.val)
	if evt == evcode["DOF_MAX_TRANSZ"]:
		set_prop(14, zmax, DOF_MAX_TRANSZ.val)
	if evt == evcode["DOF_STEP_TRANSZ"]:
		set_prop(14, zstep, DOF_STEP_TRANSZ.val)

	#do rotation buttons
	if evt == evcode["DOF_MIN_ROTX"]:
		set_prop(14, pitchmin, DOF_MIN_ROTX.val)
	if evt == evcode["DOF_MAX_ROTX"]:
		set_prop(14, pitchmax, DOF_MAX_ROTX.val)
	if evt == evcode["DOF_STEP_ROTX"]:
		set_prop(14, pitchstep, DOF_STEP_ROTX.val)

	if evt == evcode["DOF_MIN_ROTY"]:
		set_prop(14, rollmin, DOF_MIN_ROTY.val)
	if evt == evcode["DOF_MAX_ROTY"]:
		set_prop(14, rollmax, DOF_MAX_ROTY.val)
	if evt == evcode["DOF_STEP_ROTY"]:
		set_prop(14, rollstep, DOF_STEP_ROTY.val)	

	if evt == evcode["DOF_MIN_ROTZ"]:
		set_prop(14, yawmin, DOF_MIN_ROTZ.val)
	if evt == evcode["DOF_MAX_ROTZ"]:
		set_prop(14, yawmax, DOF_MAX_ROTZ.val)
	if evt == evcode["DOF_STEP_ROTZ"]:
		set_prop(14, yawstep, DOF_STEP_ROTZ.val)	
		
	#do scale buttons
	if evt == evcode["DOF_MIN_SCALEX"]:
		set_prop(14, xscalemin, DOF_MIN_SCALEX.val)
	if evt == evcode["DOF_MAX_SCALEX"]:
		set_prop(14, xscalemax, DOF_MAX_SCALEX.val)
	if evt == evcode["DOF_STEP_SCALEX"]:
		set_prop(14, xscalestep, DOF_STEP_SCALEX.val)
	
	if evt == evcode["DOF_MIN_SCALEY"]:
		set_prop(14, yscalemin, DOF_MIN_SCALEY.val)
	if evt == evcode["DOF_MAX_SCALEY"]:
		set_prop(14, yscalemax, DOF_MAX_SCALEY.val)
	if evt == evcode["DOF_STEP_SCALEY"]:
		set_prop(14, yscalestep, DOF_STEP_SCALEY.val)	

	if evt == evcode["DOF_MIN_SCALEZ"]:
		set_prop(14, zscalemin, DOF_MIN_SCALEZ.val)
	if evt == evcode["DOF_MAX_SCALEZ"]:
		set_prop(14, zscalemax, DOF_MAX_SCALEZ.val)
	if evt == evcode["DOF_STEP_SCALEZ"]:
		set_prop(14, zscalestep, DOF_STEP_SCALEZ.val)


	Draw.Redraw(1)
	Blender.Window.RedrawAll()

def draw_propsheet(x,y):
	#UI buttons
	global DOF_MAKE
	global DOF_UPDATE
	global DOF_DELETE

	global DOF_TRANSX
	global DOF_TRANSY
	global DOF_TRANSZ
	global DOF_ROTX
	global DOF_ROTY
	global DOF_ROTZ
	global DOF_SCALEX
	global DOF_SCALEY
	global DOF_SCALEZ

	global DOF_MIN_TRANSX
	global DOF_MIN_TRANSY
	global DOF_MIN_TRANSZ
	global DOF_MIN_ROTX
	global DOF_MIN_ROTY
	global DOF_MIN_ROTZ
	global DOF_MIN_SCALEX
	global DOF_MIN_SCALEY
	global DOF_MIN_SCALEZ

	global DOF_MAX_TRANSX
	global DOF_MAX_TRANSY
	global DOF_MAX_TRANSZ
	global DOF_MAX_ROTX
	global DOF_MAX_ROTY
	global DOF_MAX_ROTZ
	global DOF_MAX_SCALEX
	global DOF_MAX_SCALEY
	global DOF_MAX_SCALEZ

	global DOF_STEP_TRANSX
	global DOF_STEP_TRANSY
	global DOF_STEP_TRANSZ
	global DOF_STEP_ROTX
	global DOF_STEP_ROTY
	global DOF_STEP_ROTZ
	global DOF_STEP_SCALEX
	global DOF_STEP_SCALEY
	global DOF_STEP_SCALEZ

	#labels
	global DOF_ROTSTRING
	global DOF_TRANSTRING
	global DOF_SCALESTRING	
	global DOF_EDITLABEL
	
	#masks
	global lockxtrans
	global lockytrans
	global lockztrans
	global lockxrot
	global lockyrot
	global lockzrot
	global lockxscale
	global lockyscale
	global lockzscale
	
	global zmin
	global zmax
	global zcur
	global zstep
	global ymin
	global ymax
	global ycur
	global ystep
	global xmin
	global xmax
	global xcur
	global xstep
	global pitchmin
	global pitchmax
	global pitchcur
	global pitchstep
	global rollmin
	global rollmax
	global rollcur
	global rollstep
	global yawmin
	global yawmax
	global yawcur
	global yawstep
	global zscalemin
	global zscalemax
	global zscalecur
	global zscalestep
	global yscalemin
	global yscalemax
	global yscalecur
	global yscalestep
	global xscalemin
	global xscalemax
	global xscalecur
	global xscalestep	

	
	global evcode
	
	state = update_state()
	
	row_height = 20
	toggle_width = 50
	input_width = 100
	pad = 10
	origx = x
	origy = (row_height * 15) + (pad * 15)


	#editor label
	x = origx
	y = origy
	#y = y - (row_height + pad)
	DOF_EDITLABEL = Blender.Draw.Label("FLT Degree of Freedom Editor", x, y, 200, row_height)


	#draw Translation limits
	x = origx
	y = y- (row_height + pad)
	DOF_TRANSTRING = Blender.Draw.Label("Translation Limits", x, y, input_width, row_height)


	#X limits
	x = origx
	y = y- (row_height + pad)
	DOF_TRANSX = Blender.Draw.Toggle("LimX", evcode["DOF_TRANSX"], x, y, toggle_width, row_height, get_lockmask(lockxtrans), "")
	x = x + (toggle_width + pad)
	DOF_MIN_TRANSX = Blender.Draw.Number("MinX", evcode["DOF_MIN_TRANSX"], x, y, input_width, row_height,get_prop(14,xmin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_TRANSX = Blender.Draw.Number("MaxX", evcode["DOF_MAX_TRANSX"], x, y, input_width, row_height,get_prop(14,xmax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_TRANSX = Blender.Draw.Number("StepX", evcode["DOF_STEP_TRANSX"], x, y, input_width, row_height,get_prop(14,xstep), -1000000.0, 1000000.0, "")
	
	#Y limits
	x = origx
	y = y- (row_height + pad)
	DOF_TRANSY = Blender.Draw.Toggle("LimY", evcode["DOF_TRANSY"], x, y, toggle_width, row_height, get_lockmask(lockytrans), "")
	x = x + (toggle_width + pad)
	DOF_MIN_TRANSY = Blender.Draw.Number("MinY", evcode["DOF_MIN_TRANSY"], x, y, input_width, row_height, get_prop(14,ymin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_TRANSY = Blender.Draw.Number("MaxY", evcode["DOF_MAX_TRANSY"], x, y, input_width, row_height, get_prop(14,ymax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_TRANSY = Blender.Draw.Number("StepY", evcode["DOF_STEP_TRANSY"], x, y, input_width, row_height, get_prop(14,ystep), -1000000.0, 1000000.0, "")	
	
	#Z limits
	x = origx
	y = y- (row_height + pad)
	DOF_TRANSZ = Blender.Draw.Toggle("LimZ", evcode["DOF_TRANSZ"], x, y, toggle_width, row_height, get_lockmask(lockztrans), "")
	x = x + (toggle_width + pad)
	DOF_MIN_TRANSZ = Blender.Draw.Number("MinZ", evcode["DOF_MIN_TRANSZ"], x, y, input_width, row_height, get_prop(14,zmin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_TRANSZ = Blender.Draw.Number("MaxZ", evcode["DOF_MAX_TRANSZ"], x, y, input_width, row_height, get_prop(14,zmax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_TRANSZ = Blender.Draw.Number("StepZ", evcode["DOF_STEP_TRANSZ"], x, y, input_width, row_height, get_prop(14,zstep), -1000000.0, 1000000.0, "")
	
	#draw Rotation limits
	x = origx
	y = y- (row_height + pad)
	DOF_ROTSTRING = Blender.Draw.Label("Rotation Limits", x, y, input_width, row_height)

	#draw Rotation limits
	#X limits
	x = origx
	y = y- (row_height + pad)
	DOF_ROTX = Blender.Draw.Toggle("LimX", evcode["DOF_ROTX"], x, y, toggle_width, row_height, get_lockmask(lockxrot), "")
	x = x + (toggle_width + pad)
	DOF_MIN_ROTX = Blender.Draw.Number("MinX", evcode["DOF_MIN_ROTX"], x, y, input_width, row_height, get_prop(14,pitchmin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_ROTX = Blender.Draw.Number("MaxX", evcode["DOF_MAX_ROTX"], x, y, input_width, row_height, get_prop(14,pitchmax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_ROTX = Blender.Draw.Number("StepX", evcode["DOF_STEP_ROTX"], x, y, input_width, row_height, get_prop(14,pitchstep), -1000000.0, 1000000.0, "")
		
	#Y limits
	x = origx
	y = y- (row_height + pad)
	DOF_ROTY = Blender.Draw.Toggle("LimY", evcode["DOF_ROTY"], x, y, toggle_width, row_height, get_lockmask(lockyrot), "")
	x = x + (toggle_width + pad)
	DOF_MIN_ROTY = Blender.Draw.Number("MinY", evcode["DOF_MIN_ROTY"], x, y, input_width, row_height, get_prop(14,rollmin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_ROTY = Blender.Draw.Number("MaxY", evcode["DOF_MAX_ROTY"], x, y, input_width, row_height, get_prop(14,rollmax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_ROTY = Blender.Draw.Number("StepY", evcode["DOF_STEP_ROTY"], x, y, input_width, row_height, get_prop(14,rollstep), -1000000.0, 1000000.0, "")
		
	#Z limits
	x = origx
	y = y- (row_height + pad)
	DOF_ROTZ = Blender.Draw.Toggle("LimZ", evcode["DOF_ROTZ"], x, y, toggle_width, row_height, get_lockmask(lockzrot), "")
	x = x + (toggle_width + pad)
	DOF_MIN_ROTZ = Blender.Draw.Number("MinZ", evcode["DOF_MIN_ROTZ"], x, y, input_width, row_height, get_prop(14, yawmin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_ROTZ = Blender.Draw.Number("MaxZ", evcode["DOF_MAX_ROTZ"], x, y, input_width, row_height, get_prop(14, yawmax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_ROTZ = Blender.Draw.Number("StepZ", evcode["DOF_STEP_ROTZ"], x, y, input_width, row_height, get_prop(14, yawstep), -1000000.0, 1000000.0, "")
			

	#draw Scale limits
	x = origx
	y = y- (row_height + pad)
	DOF_SCALESTRING = Blender.Draw.Label("Scale Limits", x, y, input_width, row_height)

	#draw Scale limits
	#X limits
	x = origx
	y = y- (row_height + pad)
	DOF_SCALEX = Blender.Draw.Toggle("LimX", evcode["DOF_SCALEX"], x, y, toggle_width, row_height, get_lockmask(lockxscale), "")
	x = x + (toggle_width + pad)
	DOF_MIN_SCALEX = Blender.Draw.Number("MinX", evcode["DOF_MIN_SCALEX"], x, y, input_width, row_height, get_prop(14, xscalemin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_SCALEX = Blender.Draw.Number("MaxX", evcode["DOF_MAX_SCALEX"], x, y, input_width, row_height, get_prop(14, xscalemax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_SCALEX = Blender.Draw.Number("StepX", evcode["DOF_STEP_SCALEX"], x, y, input_width, row_height, get_prop(14, xscalestep), -1000000.0, 1000000.0, "")
		
	#Y limits
	x = origx
	y = y- (row_height + pad)
	DOF_SCALEY = Blender.Draw.Toggle("LimY", evcode["DOF_SCALEY"], x, y, toggle_width, row_height, get_lockmask(lockyscale), "")
	x = x + (toggle_width + pad)
	DOF_MIN_SCALEY = Blender.Draw.Number("MinY", evcode["DOF_MIN_SCALEY"], x, y, input_width, row_height, get_prop(14, yscalemin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_SCALEY = Blender.Draw.Number("MaxY", evcode["DOF_MAX_SCALEY"], x, y, input_width, row_height, get_prop(14, yscalemax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_SCALEY = Blender.Draw.Number("StepY", evcode["DOF_STEP_SCALEY"], x, y, input_width, row_height, get_prop(14, yscalestep), -1000000.0, 1000000.0, "")		

	#Z limits
	x = origx
	y = y- (row_height + pad)
	DOF_SCALEZ = Blender.Draw.Toggle("LimZ", evcode["DOF_SCALEZ"], x, y, toggle_width, row_height, get_lockmask(lockzscale), "")
	x = x + (toggle_width + pad)
	DOF_MIN_SCALEZ = Blender.Draw.Number("MinZ", evcode["DOF_MIN_SCALEZ"], x, y, input_width, row_height, get_prop(14, zscalemin),  -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_MAX_SCALEZ = Blender.Draw.Number("MaxZ", evcode["DOF_MAX_SCALEZ"], x, y, input_width, row_height, get_prop(14, zscalemax), -1000000.0, 1000000.0, "")
	x = x + (input_width + pad)
	DOF_STEP_SCALEZ = Blender.Draw.Number("StepZ", evcode["DOF_STEP_SCALEZ"], x, y, input_width, row_height, get_prop(14, zscalestep), -1000000.0, 1000000.0, "")		

	#System
	x = origx
	y = y - (row_height + (pad)*3)
	DOF_MAKE = Blender.Draw.PushButton("Make DOF", evcode["DOF_MAKE"], x, y, input_width, row_height, "Make a Dof Node out of Active Object")
	x = x + (input_width + pad)
	DOF_UPDATE = Blender.Draw.PushButton("Grab Loc/Rot", evcode["DOF_UPDATE"], x, y, input_width, row_height, "Update the Dof Node position/orientation")
	x = x + (input_width + pad)
	DOF_DELETE = Blender.Draw.PushButton("Delete DOF", evcode["DOF_DELETE"], x, y, input_width, row_height, "Delete the Dof Node properties")


	
	
def gui():
	#draw the propsheet/toolbox.
	psheety = 800
	#psheetx = psheety + 10
	draw_propsheet(20,psheety)

Draw.Register(gui,event,but_event)
	