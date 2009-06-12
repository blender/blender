#!BPY

"""
Name: '2D Cutout Image Importer'
Blender: 249
Group: 'Image'
Tooltip: 'Batch UV Map images to Planes'
"""

__author__ = "Kevin Morgan (forTe)"
__url__ = ("Home page, http://gamulabs.freepgs.com")
__version__ = "1.2.1"
__bpydoc__ = """\
This Script will take an image and
UV map it to a plane sharing the same width to height ratio as the image.
Import options allow for the image to be a still or sequence type image
<br><br>
Imports can be single images or whole directories of images depending on the chosen
option.
"""

####################################################
#Copyright (C) 2008: Kevin Morgan
####################################################
#-------------GPL LICENSE BLOCK-------------
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.
#
#This program is distributed in the hopes that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of 
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program. If not, see <http://www.gnu.org/licenses>.
####################################################
####################################################
#V1.0
#Basic Functionality
#Published June 28, 2007
####################################################
#V1.1
#Added Support for enabling viewport transparency
#Added more options to the UI for materials
#Added Proportionality code (Pixels per unit)
#Added GPL License Block
#Published June 29, 2007
####################################################
#V1.2
#Added Support for Copying Existing Materials
#Import Images as Sequences
#Refreshed GUI - now with more clutter :(
#Miscellaneous and Housekeeping
#Published June 16, 2008
####################################################
#V1.2.1
#Added Extend Texture Mode option at request of a user
#Published September 24, 2008
####################################################

import Blender
from Blender import BGL, Draw, Image, Mesh, Material, Texture, Window
from Blender.Mathutils import *
import bpy

# Global Constants
DIR = 0
SINGLE = 1
CUROFFS = 0

# GUI CONSTANTS
NO_EVT = 0
SINGLE_IMG = 1
DIRECTORY_IMG = 2
CLR_PATH = 3
CHG_EXT = 4
EXIT = 5
DO_SCRIPT = 6

VERSIONSTRING = '1.2.1'

# Note the two parameter dicts could be combined, I just, liked them seperate...
# GUI Buttons Dict
GUIPARAMS = {
	'Path': Draw.Create(''),
	'ImageExt': Draw.Create(''),
	'Seq': Draw.Create(0),
	'PackImage': Draw.Create(0),
	'PPU': Draw.Create(50),
	'VPTransp': Draw.Create(1),
	'XOff': Draw.Create(0.0),
	'YOff': Draw.Create(0.0),
	'ZOff': Draw.Create(0.0),
	'CopyMat': Draw.Create(0),
	'MatId': Draw.Create(0),
	'MatCol': Draw.Create(1.0, 0.0, 0.0),
	'Ref': Draw.Create(0.8),
	'Spec': Draw.Create(0.5),
	'Hard': Draw.Create(50),
	'Alpha': Draw.Create(1.0),
	'ZTransp': Draw.Create(1),
	'Shadeless': Draw.Create(0),
	'TexChan': Draw.Create(1),
	'MPTCol': Draw.Create(1),
	'MPTAlpha': Draw.Create(1),
	'UseAlpha': Draw.Create(1),
	'CalcAlpha': Draw.Create(0),
	'ExtendMode': Draw.Create(0),
	'AutoRefresh': Draw.Create(0),
	'Cyclic': Draw.Create(0),
	'Frames': Draw.Create(100),
	'Offs': Draw.Create(0),
	'StartFr': Draw.Create(1),
	'RedrawImp': Draw.Create(0)
}

# Script Execution Paramaters
PARAMS = {
	'ImagePaths': [], 									# Path to images to import
	'ImportType': SINGLE, 							# Import a Directory or a Single Image?
	'ImageProp': Image.Sources.STILL, 	# What sources for the image, still or sequence
	'PackImage': 0,											# Pack the Image(s)?
	'PPU': 20, 													# Pixels Per Blender Unit
	'MakeTransp': 1, 										# Make face transparent in viewport
	
	'NewMat': 1, 												# If true make a new material, otherwise duplicate an existing one, replacing appropriate attributes
	'MaterialId': 0,										# ID to take from the Materials list upon copy
	'Materials': None,									# Materials in Scene
	'MatProps': {'Col': [1.0, 0.0, 0.0], 'Shadeless': 1, 'Ref': 0.5, 'Spec': 0.5, 'Hard': 200, 'Alpha': 1.0, 'ZTransp': 1},
	
	'TexProps': {'UseAlpha': 1, 'CalcAlpha': 0, 'ExtendMode': 0}, # Texture Properties
	'TexChannel': 0, 										# Texture Channel
	'TexMapTo': {'Col': 1, 'Alpha': 1}, # Map to Col and/or Alpha
	'SeqProps': {'AutoRefresh': 0, 'Cyclic': 0, 'Frames': 100, 'Offs': 0, 'StartFr': 1},
	'ObOffset': Vector(1, 0, 0) 				# Offset by this vector upon creation for multifile import
}

# Get the Active Scene, of course
scn = bpy.data.scenes.active

##########################################
# MAIN SCRIPT FUNCTIONS
##########################################

def imgImport(imgPath):
	global CUROFFS, PARAMS
	######################################
	# Load the image
	######################################
	try:
		img = Image.Load(imgPath)
		imgDimensions = img.getSize() # do this to ensure the data is available
	except:
		Blender.Draw.PupMenu('Error%t|Unsupported image format for "'+ imgPath.split('\\')[-1].split('/')[-1] +'"')
		return		
	
	if PARAMS['PackImage']:
		img.pack()
	name = Blender.sys.makename(imgPath, strip = 1)
	
	######################################
	# Construct the mesh
	######################################
	
	me = Mesh.New(name)
	
	# Calculate Dimensions from Image Size
	dim = [float(i)/PARAMS['PPU'] for i in imgDimensions]
	v = [[dim[0], dim[1], 0], [-dim[0], dim[1], 0], [-dim[0], -dim[1], 0], [dim[0], -dim[1], 0]]
	me.verts.extend(v)
	me.faces.extend([0, 1, 2, 3])
	
	me.faces[0].image = img
	me.faces[0].uv = [Vector(1.0, 1.0), Vector(0.0, 1.0), Vector(0.0, 0.0), Vector(1.0, 0.0)]
	
	if PARAMS['MakeTransp']:
		me.faces[0].transp = Mesh.FaceTranspModes.ALPHA
	
	######################################
	# Modify the Material
	######################################
	
	mat = None
	if not PARAMS['NewMat']:
		mat = PARAMS['Materials'][PARAMS['MaterialId']].__copy__()
		mat.setName(name)
	else:
		mat = Material.New(name)
		properties = PARAMS['MatProps']
		mat.setRGBCol(properties['Col'])
		mat.setRef(properties['Ref'])
		mat.setSpec(properties['Spec'])
		mat.setHardness(properties['Hard'])
		mat.setAlpha(properties['Alpha'])
		
		if properties['Shadeless']:
			mat.mode |= Material.Modes.SHADELESS
		if properties['ZTransp']:
			mat.mode |= Material.Modes.ZTRANSP
	
	properties = PARAMS['TexProps']
		
	tex = Texture.New(name)
	tex.setType('Image')
	tex.setImage(img)
	if properties['UseAlpha']:
		tex.useAlpha = Texture.ImageFlags.USEALPHA
			
	if properties['CalcAlpha']:
		tex.calcAlpha = Texture.ImageFlags.CALCALPHA
		
	if properties['ExtendMode']:
		tex.setExtend('Extend')
		
	if PARAMS['ImageProp'] == Image.Sources.SEQUENCE:
		properties = PARAMS['SeqProps']
		
		img.source = PARAMS['ImageProp'] # Needs to be done here, otherwise an error with earlier getSize()
		
		tex.animStart = properties['StartFr']
		tex.animOffset = properties['Offs']
		tex.animFrames = properties['Frames']
		tex.autoRefresh = properties['AutoRefresh']
		tex.cyclic = properties['Cyclic']
			
	texMapSetters = Texture.TexCo.UV
	
	# PARAMS['TexMapTo']['Col'] (and alpha) will either be 0 or 1 because its from a toggle, otherwise this line doesn't work
	texChanSetters = Texture.MapTo.COL * PARAMS['TexMapTo']['Col'] | Texture.MapTo.ALPHA * PARAMS['TexMapTo']['Alpha']
	
	mat.setTexture(PARAMS['TexChannel'], tex, texMapSetters, texChanSetters)
	me.materials += [mat]
	
	######################################
	# Object Construction
	######################################
	
	ob = scn.objects.new(me, name)
	p = Vector(ob.getLocation()) # Should be the origin, but just to be safe, get it
	ob.setLocation((CUROFFS * PARAMS['ObOffset']) + p)
		
	return

def translateParams():
	# Translates (or assigns for the most part) GUI values to those that can be read by the
	# Import Function
	
	global GUIPARAMS, PARAMS
	
	if GUIPARAMS['Seq'].val and PARAMS['ImportType'] != DIR:
		PARAMS['ImageProp'] = Image.Sources.SEQUENCE
	
	PARAMS['PackImage'] = GUIPARAMS['PackImage'].val
	PARAMS['PPU'] = GUIPARAMS['PPU'].val
	PARAMS['MakeTransp'] = GUIPARAMS['VPTransp'].val
	PARAMS['ObOffset'] = Vector(GUIPARAMS['XOff'].val, GUIPARAMS['YOff'].val, GUIPARAMS['ZOff'].val)
	
	PARAMS['NewMat'] = not GUIPARAMS['CopyMat'].val
	PARAMS['MaterialId'] = GUIPARAMS['MatId'].val
	PARAMS['MatProps']['Col'] = list(GUIPARAMS['MatCol'].val)
	PARAMS['MatProps']['Ref'] = GUIPARAMS['Ref'].val
	PARAMS['MatProps']['Spec'] = GUIPARAMS['Spec'].val
	PARAMS['MatProps']['Hard'] = GUIPARAMS['Hard'].val
	PARAMS['MatProps']['Alpha'] = GUIPARAMS['Alpha'].val
	PARAMS['MatProps']['ZTransp'] = GUIPARAMS['ZTransp'].val
	PARAMS['MatProps']['Shadeless'] = GUIPARAMS['Shadeless'].val
	
	PARAMS['TexChannel'] = GUIPARAMS['TexChan'].val - 1 #Channels are 0-9, but GUI shows 1-10
	PARAMS['TexProps']['UseAlpha'] = GUIPARAMS['UseAlpha'].val
	PARAMS['TexProps']['CalcAlpha'] = GUIPARAMS['CalcAlpha'].val
	PARAMS['TexProps']['ExtendMode'] = GUIPARAMS['ExtendMode'].val
	PARAMS['TexMapTo']['Col'] = GUIPARAMS['MPTCol'].val
	PARAMS['TexMapTo']['Alpha'] = GUIPARAMS['MPTAlpha'].val
	
	PARAMS['SeqProps']['AutoRefresh'] = GUIPARAMS['AutoRefresh'].val
	PARAMS['SeqProps']['Cyclic'] = GUIPARAMS['Cyclic'].val
	PARAMS['SeqProps']['Frames'] = GUIPARAMS['Frames'].val
	PARAMS['SeqProps']['Offs'] = GUIPARAMS['Offs'].val
	PARAMS['SeqProps']['StartFr'] = GUIPARAMS['StartFr'].val
	return
	
def doScript():
	# Main script Function
	# Consists of choosing between 2 loops, one with a redraw, one without, see comments for why
	
	global CUROFFS
	
	translateParams()
	
	total = len(PARAMS['ImagePaths'])
	broken = 0
	
	if GUIPARAMS['RedrawImp'].val: # Reduces the need to compare on every go through the loop
		for i, path in enumerate(PARAMS['ImagePaths']):
			CUROFFS = i # Could be passed to the import Function, but I chose a global instead
			Window.DrawProgressBar(float(i)/total, "Importing %i of %i Images..." %(i+1, total))		
			imgImport(path)		
			Blender.Redraw()		
			if Blender.Get('version') >= 246:
				if Window.TestBreak():
					broken = 1
					break
	else:
		for i, path in enumerate(PARAMS['ImagePaths']):
			CUROFFS = i
			Window.DrawProgressBar(float(i)/total, "Importing %i of %i Images..." %(i+1, total))
			imgImport(path)
			if Blender.Get('version') >= 246:
				if Window.TestBreak():
					broken = 1
					break
				
	if broken:
		Window.DrawProgressBar(1.0, "Script Execution Aborted")
	else:
		Window.DrawProgressBar(1.0, "Finished Importing")
		
	Blender.Redraw() # Force a refresh, since the user may have chosen to not refresh as they go along
	
	return

##########################################
# PATH SETTERS AND CHANGERS
##########################################

def setSinglePath(filename):
	global GUIPARAMS, PARAMS
	GUIPARAMS['Path'].val = filename
	PARAMS['ImagePaths'] = [filename]
	return

def setDirPath(filename):
	global GUIPARAMS, PARAMS
	
	try:
		import os
	except:
		Draw.PupMenu('Full install of python required to be able to set Directory Paths')
		Draw.Exit()
		return
	
	path = os.path.dirname(filename) # Blender.sys.dirname fails on '/'
	GUIPARAMS['Path'].val = path
	
	ext_lower = GUIPARAMS['ImageExt'].val.lower()
	for f in os.listdir(path):
		if f.lower().endswith(ext_lower):
			PARAMS['ImagePaths'].append(os.path.join(path, f))
	
	return

def changeExtension():
	global GUIPARAMS, PARAMS
	
	if PARAMS['ImportType'] == SINGLE:
		return
	
	try:
		import os
	except:
		Draw.PupMenu('Full install of python required to be able to set Directory Paths')
		Draw.Exit()
		return
		
	PARAMS['ImagePaths'] = []
	
	ext_lower = GUIPARAMS['ImageExt'].val.lower()
	for f in os.listdir(GUIPARAMS['Path'].val):
		if f.lower().endswith(ext_lower):
			PARAMS['ImagePaths'].append(os.path.join(GUIPARAMS['Path'].val, f))
			
	return

##########################################
# INTERFACE FUNCTIONS
##########################################
def compileMaterialList():
	# Pretty straight forward, just grabs the materials in the blend file and constructs
	# an appropriate string for use as a menu
	
	mats = [mat for mat in bpy.data.materials]
	PARAMS['Materials'] = mats
	title = 'Materials%t|'
	menStrs = [mat.name + '%x' + str(i) + '|' for i, mat in enumerate(mats)]
	return title + ''.join(menStrs)

def event(evt, val):
	# Disabled, since Esc is often used from the file browser
	#if evt == Draw.ESCKEY:
	#	Draw.Exit()
		
	return

def bevent(evt):
	global GUIPARAMS, PARAMS
	
	if evt == NO_EVT:
		Draw.Redraw()
	
	elif evt == SINGLE_IMG:
		Window.FileSelector(setSinglePath, 'Image', Blender.sys.expandpath('//'))
		Draw.Redraw()
		PARAMS['ImportType'] = SINGLE
	
	elif evt == DIRECTORY_IMG:
		Window.FileSelector(setDirPath, 'Directory', Blender.sys.expandpath('//'))
		Draw.Redraw()
		PARAMS['ImportType'] = DIR
		
	elif evt == CLR_PATH:
		GUIPARAMS['Path'].val = ''
		PARAMS['ImagePaths'] = []
		GUIPARAMS['ImageExt'].val = ''
		Draw.Redraw()
		
	elif evt == CHG_EXT:
		changeExtension()
		Draw.Redraw()
		
	elif evt == EXIT:
		Draw.Exit()
		
	elif evt == DO_SCRIPT:
		doScript()
		
	else:
		print "ERROR: UNEXPECTED BUTTON EVENT"
			
	return

# GUI Colors ######
ScreenColor = [0.7, 0.7, 0.7]
BackgroundColor = [0.8, 0.8, 0.8]
TitleBG = [0.6, 0.6, 0.6]
TitleCol = [1.0, 1.0, 1.0]
ErrCol = [1.0, 0.0, 0.0]
TextCol = [0.4, 0.4, 0.5]
###################

def GUI():
	global GUIPARAMS, PARAMS
	
	BGL.glClearColor(*(ScreenColor + [1.0]))
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
	
	minx = 5
	maxx = 500
	miny = 5
	maxy = 450
	
	lineheight = 24
	buPad = 5 # Generic Button Padding, most buttons should have 24-19 (or 5) px space around them
	
	lP = 5 # Left Padding
	rP = 5 # Right Padding
	
	# Draw Background Box
	BGL.glColor3f(*BackgroundColor)
	BGL.glRecti(minx, miny, maxx, maxy)
	
	# Draw Title
	BGL.glColor3f(*TitleBG)
	BGL.glRecti(minx, maxy - (lineheight), maxx, maxy)
	BGL.glColor3f(*TitleCol)
	
	title = "2D Cutout Image Importer v" + VERSIONSTRING
	BGL.glRasterPos2i(minx + lP, maxy - 15)
	Draw.Text(title, 'large')
	
	Draw.PushButton('Exit', EXIT, maxx-50-rP, maxy - lineheight + 2, 50, 19, "Exit Script")
		
	# Path Buttons
	if GUIPARAMS['Path'].val == '':
		Draw.PushButton('Single Image', SINGLE_IMG, minx + lP, maxy - (2*lineheight), 150, 19, "Select a Single Image to Import")
		Draw.PushButton('Directory', DIRECTORY_IMG, minx + lP + 150, maxy - (2*lineheight), 150, 19, "Select a Directory of Images to Import")
		
	else:
		Draw.PushButton('Clear', CLR_PATH, minx+lP, maxy - (2*lineheight), 50, 19, "Clear Path and Change Import Options")

	GUIPARAMS['Path'] = Draw.String('Path: ', NO_EVT, minx + lP, maxy - (3*lineheight), (maxx-minx-lP-rP), 19, GUIPARAMS['Path'].val, 399, 'Path to Import From')
	if PARAMS['ImportType'] == DIR:
		GUIPARAMS['ImageExt'] = Draw.String('Image Ext: ', CHG_EXT, minx + lP, maxy - (4*lineheight), 110, 19,  GUIPARAMS['ImageExt'].val, 6, 'Image extension for batch directory importing (case insensitive)')
	GUIPARAMS['PackImage'] = Draw.Toggle('Pack', NO_EVT, maxx - rP - 50, maxy - (4*lineheight), 50, 19, GUIPARAMS['PackImage'].val, 'Pack Image(s) into .Blend File')
	
	# Geometry and Viewport Options
	BGL.glColor3f(*TextCol)
	BGL.glRecti(minx+lP, maxy - (5*lineheight), maxx-rP, maxy - (5*lineheight) + 1)
	BGL.glRasterPos2i(minx + lP, maxy-(5*lineheight) + 3)
	Draw.Text('Geometry and Display Options', 'small')
	
	GUIPARAMS['PPU'] = Draw.Slider('Pixels Per Unit: ', NO_EVT, minx + lP, maxy - (6*lineheight), (maxx-minx)/2 - lP, 19, GUIPARAMS['PPU'].val, 1, 5000, 0, 'Set the Number of Pixels Per Blender Unit to preserve Image Size Relations') 
	GUIPARAMS['VPTransp'] = Draw.Toggle('Viewport Transparency', NO_EVT, minx + lP, maxy - (8*lineheight),  (maxx-minx)/2 - lP, 2*lineheight - buPad, GUIPARAMS['VPTransp'].val, 'Display Alpha Transparency in the Viewport')

	GUIPARAMS['XOff'] = Draw.Slider('Offs X: ', NO_EVT, minx + lP + (maxx-minx)/2, maxy - (6*lineheight), (maxx-minx)/2 - lP - rP, 19, GUIPARAMS['XOff'].val, 0, 5.0, 0, 'Amount to Offset Each Imported in the X-Direction if Importing Multiple Images')
	GUIPARAMS['YOff'] = Draw.Slider('Offs Y: ', NO_EVT, minx + lP + (maxx-minx)/2, maxy - (7*lineheight), (maxx-minx)/2 - lP - rP, 19, GUIPARAMS['YOff'].val, 0, 5.0, 0, 'Amount to Offset Each Imported in the Y-Direction if Importing Multiple Images')
	GUIPARAMS['ZOff'] = Draw.Slider('Offs Z: ', NO_EVT, minx + lP + (maxx-minx)/2, maxy - (8*lineheight), (maxx-minx)/2 - lP - rP, 19, GUIPARAMS['ZOff'].val, 0, 5.0, 0, 'Amount to Offset Each Imported in the Z-Direction if Importing Multiple Images')

	# Material and Texture Options
	BGL.glColor3f(*TextCol)
	BGL.glRecti(minx+lP, maxy - (9*lineheight), maxx-rP, maxy - (9*lineheight) + 1)
	BGL.glRasterPos2i(minx + lP, maxy-(9*lineheight) + 3)
	Draw.Text('Material and Texture Options', 'small')
	
	half = (maxx-minx-lP-rP)/2
	GUIPARAMS['CopyMat'] = Draw.Toggle('Copy Existing Material', NO_EVT, minx + lP, maxy-(10*lineheight), half, 19, GUIPARAMS['CopyMat'].val, 'Copy an Existing Material')
	if GUIPARAMS['CopyMat'].val:
		menStr = compileMaterialList()
		GUIPARAMS['MatId'] = Draw.Menu(menStr, NO_EVT, minx + lP, maxy - (11*lineheight), half, 19, GUIPARAMS['MatId'].val, 'Material to Copy Settings From') 
	else:
		GUIPARAMS['MatCol'] = Draw.ColorPicker(NO_EVT, minx+lP, maxy - (13*lineheight), 40, (3*lineheight) - buPad, GUIPARAMS['MatCol'].val, 'Color of Newly Created Material')
		GUIPARAMS['Ref'] = Draw.Slider('Ref: ', NO_EVT, minx +lP+45, maxy - (11*lineheight), half-45, 19, GUIPARAMS['Ref'].val, 0.0, 1.0, 0, 'Set the Ref Value for Created Materials')
		GUIPARAMS['Spec'] = Draw.Slider('Spec: ', NO_EVT, minx +lP+45, maxy - (12*lineheight), half-45, 19, GUIPARAMS['Spec'].val, 0.0, 2.0, 0, 'Set the Spec Value for Created Materials')
		GUIPARAMS['Hard'] = Draw.Slider('Hard: ', NO_EVT, minx +lP+45, maxy - (13*lineheight), half-45, 19, GUIPARAMS['Hard'].val, 1, 500, 0, 'Set the Hardness Value for Created Materials')
		GUIPARAMS['Alpha'] = Draw.Slider('A: ', NO_EVT, minx +lP, maxy - (14*lineheight), half, 19, GUIPARAMS['Alpha'].val, 0.0, 1.0, 0, 'Set the Alpha Value for Created Materials')
		
		GUIPARAMS['ZTransp'] = Draw.Toggle('ZTransparency', NO_EVT, minx + lP, maxy - (15*lineheight), half, 19, GUIPARAMS['ZTransp'].val, 'Enable ZTransparency')
		GUIPARAMS['Shadeless'] = Draw.Toggle('Shadeless', NO_EVT, minx + lP, maxy - (16*lineheight), half, 19, GUIPARAMS['Shadeless'].val, 'Enable Shadeless')

	GUIPARAMS['TexChan'] = Draw.Number('Texture Channel: ', NO_EVT, minx + lP+ half + buPad, maxy - (10*lineheight), half-rP, 19, GUIPARAMS['TexChan'].val, 1, 10, 'Texture Channel for Image Texture')
	
	GUIPARAMS['MPTCol'] = Draw.Toggle('Color', NO_EVT, minx + lP + half + buPad, maxy - (11*lineheight), half/2, 19, GUIPARAMS['MPTCol'].val, 'Map To Color Channel')
	GUIPARAMS['MPTAlpha'] = Draw.Toggle('Alpha', NO_EVT, minx + lP + int((1.5)*half) + buPad, maxy - (11*lineheight), half/2 - rP, 19, GUIPARAMS['MPTAlpha'].val, 'Map To Alpha Channel')
	
	third = int((maxx-minx-lP-rP)/6)
	GUIPARAMS['UseAlpha'] = Draw.Toggle('Use Alpha', NO_EVT, minx + lP + half + buPad, maxy - (12*lineheight), third, 19, GUIPARAMS['UseAlpha'].val, "Use the Images' Alpha Values")
	GUIPARAMS['CalcAlpha'] = Draw.Toggle('Calc Alpha', NO_EVT, minx + lP + half + third + buPad, maxy - (12*lineheight), third, 19, GUIPARAMS['CalcAlpha'].val, "Calculate Images' Alpha Values")
	GUIPARAMS['ExtendMode'] = Draw.Toggle('Extend', NO_EVT, minx+lP+half+third+third+buPad, maxy - (12*lineheight), third-3, 19, GUIPARAMS['ExtendMode'].val, "Use Extend texture mode. If deselected, Repeat is used")
	GUIPARAMS['Seq'] = Draw.Toggle('Sequence', NO_EVT, minx + lP + half + buPad, maxy - (13*lineheight), half-rP, 19, GUIPARAMS['Seq'].val, 'Set the Image(s) to use a Sequence instead of a Still')
	
	if GUIPARAMS['Seq'].val and not PARAMS['ImportType'] == DIR:
		GUIPARAMS['AutoRefresh'] = Draw.Toggle('Auto Refresh', NO_EVT, minx + lP + half + buPad, maxy - (14*lineheight), half/2, 19, GUIPARAMS['AutoRefresh'].val, 'Use Auto Refresh')
		GUIPARAMS['Cyclic'] = Draw.Toggle('Cyclic', NO_EVT, minx + lP + half + buPad + half/2, maxy - (14*lineheight), half/2 - rP, 19, GUIPARAMS['Cyclic'].val, 'Repeat Frames Cyclically`')

		GUIPARAMS['Frames'] = Draw.Number('Frames: ', NO_EVT, minx +lP + half + buPad, maxy - (15*lineheight), half - rP, 19, GUIPARAMS['Frames'].val, 1, 30000, 'Sets the Number of Images of a Movie to Use')
		GUIPARAMS['Offs'] = Draw.Number('Offs: ', NO_EVT, minx +lP + half + buPad, maxy - (16*lineheight), half/2, 19, GUIPARAMS['Offs'].val, -30000, 30000, 'Offsets the Number of the Frame to use in the Animation')
		GUIPARAMS['StartFr'] = Draw.Number('StartFr: ', NO_EVT, minx +lP + half + buPad + half/2, maxy - (16*lineheight), half/2 - rP, 19, GUIPARAMS['StartFr'].val, 1, 30000, 'Sets the Global Starting Frame of the Movie')
	elif GUIPARAMS['Seq'].val and PARAMS['ImportType'] == DIR:
		BGL.glColor3f(*ErrCol)
		BGL.glRasterPos2i(minx + lP + half + buPad + 7, maxy-(14 * lineheight) + 5)
		Draw.Text('Sequence only available for Single Image Import', 'small')
		
	# Import Options
	BGL.glColor3f(*TextCol)
	BGL.glRecti(minx+lP, maxy - (17*lineheight), maxx-rP, maxy - (17*lineheight) + 1)
	BGL.glRasterPos2i(minx + lP, maxy-(17*lineheight) + 3)
	Draw.Text('Import', 'small')

	if GUIPARAMS['Path'].val and GUIPARAMS['ImageExt'].val or GUIPARAMS['Path'].val and PARAMS['ImportType'] == SINGLE:
		Draw.PushButton('Import', DO_SCRIPT, minx + lP, maxy - (18*lineheight), 75, 19, "Import Image(s)")
	else:
		BGL.glColor3f(*ErrCol)
		BGL.glRasterPos2i(minx+lP, maxy - (18*lineheight) + 5)
		Draw.Text('A path and image type must be specified to import images')
		
	GUIPARAMS['RedrawImp'] = Draw.Toggle('Redraw During Import', NO_EVT, maxx - rP - 150, maxy - (18*lineheight), 150, 19, GUIPARAMS['RedrawImp'].val, 'Redraw the View as Images Import')

Draw.Register(GUI, event, bevent)