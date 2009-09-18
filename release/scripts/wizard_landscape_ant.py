#!BPY
"""
Name: 'Landscape Generator (A.N.T)'
Blender: 248
Group: 'Wizards'
Tip: 'Create landscape mesh.'
"""

__author__ = "Jimmy Hazevoet"
__url__     = ('http://wiki.blender.org/index.php/Scripts/Manual/Wizards/ANTLandscape','elysiun')
__version__ = "v.1.05 03-2007"
__bpydoc__ = """\

Another Noise Tool 'Landscape' v.1.05

This script uses noise functions to create a terrain from a grid mesh.

Usage:

Make new terrain:  press the "Add New Grid" button, change some noise settings and press the "Update" button.

Work on previous made grid:  select the grid/terrain in the 3D view, and press the "Assign" button.

Tip: use a low resolution grid mesh and add some Multires levels for detail

Note: when using Multires materials can get weird,
only apply materials when your terrain is ready, or not use Multires.

This Script creates a landscape mesh.
Press the Auto button when you start the script.
Then the Generate Button & read it's tooltip.
The mesh created is average 16000 verts.
To me the mesh appears quite small in the 3d view.
Just Press S in the 3d view to scale it up.
This saves overhead drawing the Mesh.

Known Issues:
If the mesh is not drawn in the 3d view first time, 
Move your mouse to the 3d view, or press the button again.

Not really an issue, more to be aware.
Due to the complex nature & design of the script, it only creates one mesh at a time.
When you press Generate or Reset, 
Even if you have Closed then Opened the script or .blend file,
The mesh Will be Overwritten.
To create Multiple Landscapes you Must Re-Name or save the Mesh
in Blender's F7 menu Links & Materials Panel.

Readme:
v.1.04:
_ New G.U.I.
_ New noise types like: 
Double_Terrain, 
StatsByAlt_Terrain, 
slickRock, 
ditorted_heteroTerrain, 
vlNoise_turbulence, 
and many more.

New fractalized Effect functions.     
Effect types such as: gradient,
waves and bumps, dome, piramide, 
squares, grid, shattered rocks, 
lunar, and many more.
     
Bias types: Sin, Cos, Tri, Saw, 
and Default(no bias).
    
For example the 'Rings' effect 
with 'Sin Bias' makes 'Rings' 
and 'Default Bias' makes 'Dome'.
The Effect 'Mix factor' Slider gives control over how much of the Effect is vissible, 
-1.0=noise, 0.0=average, 1.0=effect
this slider controls also the 'Warp amount' if mix type 'Warp' is selected.

Image effect: mix image with noise
_ IPOCurve Filter: use Ipo curve to filter terrain height.
I know it's a bit strange to use animation curves for landscape modelling, (actualy i want a curves panel in my G.U.I. but i dont know how to do that)
the downside of this method is that you have this 'Empty' moving around in your scene, so put it on another layer and 'Pin' the Ipo block so that it stays visible.
Usage:
Add one 'Empty' Object to your scene, now add one Loc Key at Frame 1, go to Frame 101 (UpArrow ten times),
move the 'Empty' +100 units in X+ direction and add another Loc Key, open the Ipo Curve Editor Window, rename this IpoBlock if you want,
Copie the first curve to buffer and Paste it in the empty slots (from top to bottom), now you can edit the curves freely.
(use the 'Pin' option to keep the curves visible while other objects are selected)
Set the CurveLength and CurveHeight Button value's according to the length and height of the selected curve.
A curve filter is very versatile when it comes to 'height' filtering.

_ PreView in UV/Image Editor Window:
The preview image is now rendered as Blender.Image and will be saved to file directory as 'ANTview_size.tga'
you have to select 'ANTview_size.tga' in the UV/Image Editor Window
now it's posible to render and save a large HeightMap image (you can also create nice texture images when the VertCol gradient is enabled),
! If you Change the preview image Size a New Blender.Image object is created. (one for size 256, one for size 512 one for.....) !

_ VertexColours: use any image as colour gradient.
This function actualy uses one 'row' of pixels from a image to produce the color gradient,
Make one or more custom gradient images with the Gimp or any other graphics software, the gradients must go from left to right (left is bottom and right is top.
you only need one row of pixels so you can put 10 gradients in a 256*10 image.(higher resolutions like 512*n or 1024*n gives smoother result) 
Set Window DrawMode 'Textured' , and set the 'VCol Paint' option in the Materials panel !

_ Mesh Tiles: Create large scale terrains.

_ Vertices Selection: select flat areas.

_ Keyboard HotKeys:
SPACE = Update mesh.
R = Randomise.
V = Redraw preview.

_ and more...

"""




###
#
# Alt+P to start script.
#
###
# scroll down to see info about updates
##
################################################################################################################
# Another Noise Tool 'Landscape'
# Jimmy Hazevoet
# license: Do whatever you want with it.
################################################################################################################

################################################################################################################
#   v.1.04:
#
# _ New G.U.I.
# _ New noise types like: Double_Terrain, StatsByAlt_Terrain, slickRock, ditorted_heteroTerrain, vlNoise_turbulence, and many more.
# _ New fractalized Effect functions.
#      Effect types such as: gradient, waves and bumps, dome, piramide, squares, grid, shattered rocks, lunar, and many more.
#      Bias types: Sin, Cos, Tri, Saw, and Default(no bias).
#      For example the 'Rings' effect with 'Sin Bias' makes 'Rings' and 'Default Bias' makes 'Dome'.
# _ The Effect 'Mix factor' Slider gives control over how much of the Effect is vissible, -1.0=noise, 0.0=average, 1.0=effect
#      this slider controls also the 'Warp amount' if mix type 'Warp' is selected.
# _ Image effect: mix image with noise
# _ IPOCurve Filter: use Ipo curve to filter terrain height.
#      I know it's a bit strange to use animation curves for landscape modelling, (actualy i want a curves panel in my G.U.I. but i dont know how to do that)
#      the downside of this method is that you have this 'Empty' moving around in your scene, so put it on another layer and 'Pin' the Ipo block so that it stays visible.
#     Usage:
#      Add one 'Empty' Object to your scene, now add one Loc Key at Frame 1, go to Frame 101 (UpArrow ten times),
#      move the 'Empty' +100 units in X+ direction and add another Loc Key, open the Ipo Curve Editor Window, rename this IpoBlock if you want,
#      Copie the first curve to buffer and Paste it in the empty slots (from top to bottom), now you can edit the curves freely.
#      (use the 'Pin' option to keep the curves visible while other objects are selected)
#      Set the CurveLength and CurveHeight Button value's according to the length and height of the selected curve.
#      A curve filter is very versatile when it comes to 'height' filtering.
# _ PreView in UV/Image Editor Window:
#      The preview image is now rendered as Blender.Image and will be saved to file directory as 'ANTview_size.tga'
#      you have to select 'ANTview_size.tga' in the UV/Image Editor Window
#      now it's posible to render and save a large HeightMap image (you can also create nice texture images when the VertCol gradient is enabled),
#      ! If you Change the preview image Size a New Blender.Image object is created. (one for size 256, one for size 512 one for.....) !
# _ VertexColours: use any image as colour gradient.
#      This function actualy uses one 'row' of pixels from a image to produce the color gradient,
#      Make one or more custom gradient images with the Gimp or any other graphics software, the gradients must go from left to right (left is bottom and right is top.
#      you only need one row of pixels so you can put 10 gradients in a 256*10 image.(higher resolutions like 512*n or 1024*n gives smoother result) 
#      Set Window DrawMode 'Textured' , and set the 'VCol Paint' option in the Materials panel !
# _ Mesh Tiles: Create large scale terrains.
# _ Vertices Selection: select flat areas.
# _ Keyboard HotKeys:
#      SPACE = Update mesh.
#      R = Randomise.
#      V = Redraw preview.
# _ and more...
################################################################################################################

################################################################################################################
# BugFix: Sept./2006  v.1.04a
#-----------------------------
# _Image Effect did not worked well with tiled mesh. Fixed (now use Freq. and Loc. buttons to scale and position image).
#
################################################################################################################


################################################################################################################
# UPDATE: v.1.05  03-2007
#---------------------------------------------------------------------------------------------------------------
#
# _ New: Save and Load function, save your settings to a .ant file.
# __NOTE: when saving/loading settings to/from a file,
#         make sure the filename/path is not too long!
# __HOTKEY__ Load from file: L
# __HOTKEY__ Save to file  : S
#
# _ New mesh code, uses Mesh instead of NMesh,
#        this gives a small speed improvement and alows you to use Multires.
#
#   Usage: Select a Grid/Terrain mesh and hit the Assign button, now you can work on it, when ready you assign another.
#
# _ New: 'Musgrave' noise types, 'Random noise' and 'Constant' in 'Effects' section.
# _ New: 'Custom Effect', write custom formulae ( x,y, a,b, from math import *, from Blender.Noise import * )
# _ New: 'Custom Height Filter', write custom formulae ( x,y, h, a,b, from math import *, from Blender.Noise import * )
# _ New: 'Change Filter Order', Toggle: OFF = Noise+Effect+Falloff+FILTER / ON = Noise+FILTER+Effect+Falloff
#
# _ If you want to make a tiled terrain, you need to set the coordinates to "WorldSpace" or "Center at Cursor" (in G.U.I.Noise panel),
#   create and place the grid meshes. now one by one select, assign and update, you may need to adjust the "EdgeFalloff" size.
#
# WARNING!: when using Multires, materials can get weird (?), so apply materials when your terrain is finnished.
#
###############################################################################################################


###############################################################################################################
#
##  Execute Script: Alt P 
#


import Blender
from Blender import * 
from math import *
from Blender.Noise import * 
from Blender.Draw import *
from Blender.BGL import *
from Blender import Image
import string
from string import strip
import BPyMathutils
from BPyMathutils import genrand
from random import choice
scene = Scene.GetCurrent()

###---------------------------------------------------------------------------

CurVersion = 'A.N.T.Landscape v.1.05'

##---------------------------------------------------------------------------
# Customise default settings: ----------------------------------------------

# Names:
antfilename = 'Terrain'   # Default filename
previewname = Create('')  # Default preview Image name
DefaultIpoName  = ''      # Default Ipo DataBlock name (for height filter)
# G.U.I.:
FullScreen = Create( 0 )            # FullScreen on/off
# gui colors:
ledcolors = [ [1.0,0.498,0.498], [1.0,0.698,0.498], [1.0,0.898,0.498], [0.898,1.0,0.498], [0.698,1.0,0.498], [0.498,1.0,0.498], [0.498,1.0,0.698], [0.498,1.0,0.898], [0.600,0.918,1.0], [0.6,0.757,1.0], [0.6,0.6,1.0], [0.757,0.6,1.0], [0.898,0.498,1.0], [1.0,0.498,0.898] ]
#ledcolor   = [ 1.0, 0.5, 0.0 ]
lightgrey  = [ 0.76, 0.76, 0.76 ]   # gui col.
grey       = [ 0.6, 0.6, 0.6 ]      # panel col.
background = [ 0.7, 0.7, 0.7, 1.0 ] # background col.
black      = [ 0.0, 0.0, 0.0 ]      # text col.
white      = [ 1.0, 1.0, 1.0 ]
# gui size
size_x  = 320                       # gui x size
size_y  = 280                       # gui y size
# tabs
guitabs = [ Create( 1 ), Create( 0 ), Create( 0 ), Create( 0 ), Create( 0 ) ]  # gui Tabs
# How long does it take to generate a mesh or image ?
print_time = 0 # 1 = Print time in console.

# end customise. ----------------------------------------------------------
##--------------------------------------------------------------------------
###--------------------------------------------------------------------------
####--------------------------------------------------------------------------
##---------------------------------------------------------------------------

dirpath=Blender.sys.dirname(Blender.Get('filename'))
fname=dirpath.replace('\\','/')+'/' + antfilename + '.ant'
txtFile = Create( fname )

###---------------------------------------------------------------------------
columns = 10 # gui columns
rows    = 13 # gui rows
actob = [] # active object
actme = [] # active mesh
ipoblockname=''
thiscurve=[]
selectedcurve=0
phi=3.14159265359
# events
App_Evt = 144
New_Evt = 166
SelFile_Evt  = 71
LoadFile_Evt = 72
SaveFile_Evt = 73
UseMe_Evt = 74
No_Evt   = 1
Btn_Evt  = 2
Msh_Evt  = 12
Upd_Evt  = 3
Rndm_Evt = 4
Load_Evt = 5
Sel_Evt  = 6
Save_Evt = 7
Rend_Evt = 8
End_Evt  = 9
Scrn_Evt = 15
Im_Evt   = 16
gt0_Evt  = 20
gt1_Evt  = 21
gt2_Evt  = 22
gt3_Evt  = 23
gt4_Evt  = 24
Ipo_Evt  = 17
New_Ipo_Evt=700

###---------------------------------------------------------------------------
# menus
noisetypemenu  = "Noise type: %t|multiFractal %x0|ridgedMFractal %x1|hybridMFractal %x2|heteroTerrain %x3|fBm %x4|turbulence %x5|Voronoi turb. %x6|vlNoise turb. %x7|noise %x8|cellNoise %x9|Marble %x10|lava_multiFractal %x11|slopey_noise %x12|duo_multiFractal %x13|distorted_heteroTerrain %x14|slickRock %x15|terra_turbulence %x16|rocky_fBm %x17|StatsByAlt_Terrain %x18|Double_Terrain %x19|Shattered_hTerrain %x20|vlhTerrain %x21"
noisebasismenu = "Basis %t|Blender Original%x0|Original Perlin%x1|Improved Perlin%x2|Voronoi_F1%x3|Voronoi_F2%x4|Voronoi_F3%x5|Voronoi_F4%x6|Voronoi_F2-F1%x7|Voronoi Crackle%x8|CellNoise%x9"
voronitypemenu = "Voronoi type %t|Distance %x0|Distance Squared %x1|Manhattan %x2|Chebychev %x3|Minkovsky 1/2 %x4|Minkovsky 4 %x5|Minkovsky %x6"
tBasismodemenu = "Terrain basis mode: %t|noise %x0|ridged noise %x1|vlNoise %x2|ridged vlNoise %x3"
effecttypemenu = ['Effect Type %t','No Effect %x0','Image %x1','Turbulence %x2','vlNoise %x3','Marble %x4', 'multiFractal %x5','ridgedMFractal %x6','hybridMFractal %x7','heteroTerrain %x8','fBm %x9', 'Gradient %x10','Waves and Bumps %x11','ZigZag %x12','Wavy %x13','Sine Bump %x14','Dots %x15','Rings / Dome %x16','Spiral %x17','Square / Piramide %x18','Blocks %x19','Grid %x20','Tech %x21','Crackle %x22','Sparse Cracks %x23','Shattered Rocks %x24','Lunar %x25','Cosine noise %x26','Spike noise %x27','Stone noise %x28','Flat Turb %x29','Flat Voroni %x30','Random noise %x31','Constant %x32','Custom Effect %x33' ]
mixtypemenu    = ['Mix Type %t','Effect only %x0','%l','Mix %x1','Add %x2','Subtract %x3','Multiply %x4','Difference %x5','Screen %x6','addmodulo %x7','Minimum %x8','Maximum %x9','%l','Warp Effect %x10','Warp Noise %x11']
biastypemenu   = "Bias %t|Sin bias %x0|Cos bias %x1|Tri bias %x2|Saw bias %x3|Default (no bias)%x4"
sharptypemenu  = "Sharpen %t|Soft %x0|Sharp %x1|Sharper %x2"
filtermodemenu = "Filter Mode %t|No Filter %x0| %l|Default Filters %x1|IPOCurve Filter %x2|Custom Filter %x3"
filtertypemenu = "Filter Type %t|Default Terrace %x0|Sharper Terrace %x1|Rounded Terrace %x2|Posterise Mixed %x3|Posterise %x4|Layered Peaks %x5|Peaked %x6|Smooth-thing %x7|Sin bias %x8|Cos bias %x9|Tri bias %x10|Saw bias %x11|Clamp Max. %x12"
falloftypemenu = "Edge Falloff %t|No Edge Falloff %x0| %l|Soft Falloff %x1|Default Falloff %x2|Hard Falloff %x3|Linear Falloff Y %x4|Linear Falloff X %x5|Diagonal Falloff + %x6|Diagonal Falloff - %x7|Square %x8|Round %x9"
randomtypemenu = "Random type: %t|setRandomSeed() : Blender.Noise %x0|Rand() : Blender.Mathutils %x1|genrand() : BPyMathutils MersenneTwister %x2"

##--------------------------------------------------
def Set_ReSet_Values():
	global fileinfo, filemessage
	global iScale, Offset, Invert, NSize, Sx, Sy, Lx, Ly, WorldSpaceCo
	global NType, Basis, musgr, vlnoi, vlnoiTwo, voron, turbOne, turbTwo, marbleOne, marbleTwo, tBasismod, musgrTwo
	global CustomFX, effect_image, Effect_Ctrl, Min, Max, Falloff, CustomFilt, Filter_Mode, Def_Filter_Ctrl, Ipo_Filter_Ctrl, Filter_Order
	global RandMod, RSeed, rand_H, rand_S, rand_L, rand_I, AutoUpd, PreView, DefaultIpoName

	filemessage = ''
	fileinfo    = ''
	effect_image = 'Load and Select image.'
	AutoUpd  = Create( 0 )
	PreView = [ Create( 0 ), Create( 1.0 ), Create( 0.0 ) ]
	## Coords controls:
	WorldSpaceCo = Create(0)
	iScale = [ Create( 1.0 ), Create( 1.0 ), Create( 0.25) ]
	Offset = [ Create( 0.0 ), Create( 0.0), Create( 0.0) ]
	Invert = [ Create( 0 ), Create( 0 ), Create( 0 ) ]
	NSize = [ Create( 1.0 ), Create( 2.0 ) ]
	Sx = [ Create( 1.0 ), Create( 1.0 ) ]
	Sy = [ Create( 1.0 ), Create( 1.0 ) ]
	Lx = [ Create( 0.0 ), Create( 0.0 ) ]
	Ly = [ Create( 0.0 ), Create( 0.0 ) ]
	## Noise controls:
	NType = Create( 3 )
	Basis = [ Create( 0 ), Create( 0 ) ]
	musgr = [ Create( 1.0 ), Create( 2.0 ), Create( 8 ), Create( 1.0 ), Create( 1.0 ), Create( 0.5 ) ]
	vlnoi = [ Create( 1.0 ), Create( 0 ) ]
	vlnoiTwo = [ Create( 1.0 ), Create( 0 ) ]
	voron = [ Create( 0 ), Create( 2.5 ) ]
	turbOne = [ Create( 6 ), Create( 0 ), Create( 0.5 ), Create( 2.0 ) ]
	marbleOne = [ Create( 6 ), Create( 0 ), Create( 2.0 ), Create( 0 ), Create( 0 ), Create( 1.0 ) ]
	tBasismod = Create(0)
	## Effect controls:
	musgrTwo = [ Create( 1.0 ), Create( 2.0 ), Create( 8 ), Create( 1.0 ), Create( 1.0 ) ]
	turbTwo = [ Create( 6 ), Create( 0 ), Create( 0.5 ), Create( 2.0 ) ]
	marbleTwo = [ Create( 6 ), Create( 0 ), Create( 2.0 ), Create( 0 ), Create( 0 ), Create( 1.0 ) ]
	Effect_Ctrl = [ Create( 0 ),Create( 2 ),Create( 0.0 ),Create( 0 ),Create( 0.0 ) ,Create( 0 ),Create( 2.0 ),Create( 0.5 ),Create( 0.5 ) ]
	CustomFX = [ Create('sin(x*pi)'), Create('cos(y*pi)'), Create('abs(a*b)*0.5') ]
	## Filter controls:
	Min = Create( 0.0 )
	Max = Create( 1.0 )
	Falloff = [ Create( 2 ), Create( 1.0 ), Create( 1.0 ), Create( 0 ) , Create( 0 ) ]
	Filter_Mode = Create( 0 )
	Def_Filter_Ctrl = [ Create( 0 ), Create( 3.0 ) ]
	Ipo_Filter_Ctrl = [ Create( DefaultIpoName ), Create( 0 ), Create( 100.0 ), Create( 100.0 ) ]
	Filter_Order =  Create( 0 )
	CustomFilt = [ Create('sqrt(h*h)**2'), Create('0'), Create('a') ]
	## Randomise noise buttons:
	RandMod = Create( 1 )
	RSeed   = Create( 0 )
	rand_I  = Create( 0 )
	rand_H  = Create( 0 )
	rand_S  = Create( 0 )
	rand_L  = Create( 1 )

##-------------------------
Set_ReSet_Values()


####----------------------------------------------------------------------------------------------------
###----------------------------------------------------------------------------------------------------
## G.U.I.: text,backpanel,panel
#--------------------------------------------------
def draw_Text( ( x, y ), text, color, size ):
	glColor3f( color[0],color[1],color[2] )
	glRasterPos2d(x,y)
	txtsize = 'small', 'normal', 'large'
	Text( text, txtsize[ size ] )
def draw_BackPanel( text, x, y, w, h, colors ):
	glColor3f( colors[0]*0.76, colors[1]*0.76, colors[2]*0.76 )
	glRecti( x, h, w, h+20 )
	glColor3f( colors[0], colors[1], colors[2] )
	glRecti( x, y, w, h )
	glColor3f( colors[0], colors[1], colors[2] )
	glRasterPos2d( x+10, h+5 )
	Text( text, 'small' )
def draw_Panel( x, y, w, h, colors ):
	glColor3f( colors[0], colors[1], colors[2] )
	glRecti( x,y, w,h )
def draw_Frame( text, x, y, w, h, color ):
   glColor3f( color[0], color[1], color[2] )
   glRasterPos2i(x+3,h-3)
   Text(text ,'small')
   stringwidth = GetStringWidth( text,'small' )
   glColor3f( color[0], color[1], color[2]  )
   glBegin(Blender.BGL.GL_LINE_STRIP)
   glVertex2i(x,h)
   glVertex2i(x,y)
   glVertex2i(w,y)
   glVertex2i(w,h)
   glVertex2i(x+stringwidth+10,h)
   glEnd()
def draw_Led( x, y, colors ):
	glColor3f( colors[0], colors[1], colors[2] )
	glRecti( x,y, x+4,y+4 )


###----------------------------------------------------------------------------------------------------
## G.U.I. Buttons:
#----------------------------------------------------------------------------------------------------

###-------------------------
## Main / Mesh Buttons:
#
def MeshButtons( col, row, width, height ):
	global actme, actob, AutoUpd, txtFile, filemessage, fileinfo

	PushButton("I", UseMe_Evt, col[8], row[3], width[0], height[1], "Info: Here you can write some text to save with the file." )
	draw_Text( ( col[0], row[1]+5 ), 'Info: ' + fileinfo, black, 0 )
	txtFile = String("",  No_Evt,      col[0], row[2], width[9], height[1],  txtFile.val, 256, "File: Full path and filename" )
	PushButton( "Select", SelFile_Evt, col[1], row[3], width[0], height[1], "File: Open FileBrowser and select *.ant file" )
	PushButton( "Load",   LoadFile_Evt,col[2], row[3], width[2], height[1], "File: Load settings from file ( HotKey: L )" )
	PushButton( "Save",   SaveFile_Evt,col[5], row[3], width[2], height[1], "File: Save settings to file ( HotKey: S )" )

	activeobname = ''
	if actme !=[]:
		activeobname = actob[0].name
	draw_Text( ( col[5]+5, row[7]-5 ), 'OB: ' + activeobname, [0.0,0.0,1.0], 1 )
	PushButton( "Add New Grid",    New_Evt, col[0], row[6], width[4], height[2] )
	PushButton( "Assign Selected", App_Evt, col[5], row[6], width[4], height[2], 'Assign selected terrain')

###-------------------------
## Noise Buttons:
#
def NoiseButtons( col, row, width, height ):
	global NSize, iScale, Offset, Invert, Lx, Ly, Sx, Sy, WorldSpaceCo
	global Ha, La, Oc, Of, Ga, Basis, NType, musgr, vlnoi, voron, turbOne, tBasismod 
	global Depth, Hard, Amp, Freq, vlBasis, Distort, VFunc, VExp, VDep, marbleOne
	global RandMod, RSeed, rand_H, rand_S, rand_L, rand_I

	bth = height[1]/2+5
	iScale[0] = Number("iScale:", Btn_Evt, col[5], row[2]+bth, width[3], height[1], iScale[0].val,   -10.0, 10.0 , "Noise: Intensity Scale." )
	Invert[0] = Toggle("Inv.",    Btn_Evt, col[9], row[2]+bth, width[0], height[1], Invert[0].val, "Noise: Invert")
	Offset[0] = Number("Offset:", Btn_Evt, col[5], row[3]+bth, width[4], height[1], Offset[0].val,   -10.0, 10.0 , "Noise: Offset " )
	NSize[0] = Number("Noise Size:",Btn_Evt, col[5], row[5], width[4], height[2], NSize[0].val, 0.001, 10.0 , "Noise Size" )
	Sx[0]     = Number("Size X:", Btn_Evt, col[5], row[6], width[4], height[1], Sx[0].val,    0.001, 10.0 , "Size X" )
	Sy[0]     = Number("Size Y:", Btn_Evt, col[5], row[7], width[4], height[1], Sy[0].val,    0.001, 10.0 , "Size Y" )
	Lx[0]     = Number("Loc X:",  Btn_Evt, col[5], row[8], width[4], height[1], Lx[0].val,  -10000.0, 10000.0 , "Loc X" )
	Ly[0]     = Number("Loc Y:",  Btn_Evt, col[5], row[9],width[4], height[1], Ly[0].val,  -10000.0, 10000.0 , "Loc Y" )
	WorldSpaceCo = Menu( "Coordinates %t|Local Space %x0|World Space %x1|Center at CursorPos %x2", Btn_Evt, col[5], row[10], width[4], height[1], WorldSpaceCo.val, "x,y,z coordinates for noise, effect and height falloff " )

	NType = Menu( noisetypemenu,       Btn_Evt, col[0], row[2], width[4], height[2], NType.val, "Noise type" )
	if NType.val == 6:
		voron[0] = Menu( voronitypemenu, Btn_Evt, col[0], row[3], width[4], height[1], voron[0].val, "Voronoi type" )
	else:
		if NType.val != 9:
			Basis[0] = Menu( noisebasismenu, Btn_Evt, col[0], row[3], width[4], height[1], Basis[0].val, "Noise Basis" )

	if NType.val in [0,1,2,3,4,11,12,13,14,15,17,18,19,20,21]:
		musgr[0] = Slider( "H: ",          Btn_Evt, col[0], row[5], width[4], height[1], musgr[0].val,  0.0, 3.0, 0 ,  "H" )
		musgr[1] = Slider( "Lacu: ",       Btn_Evt, col[0], row[6], width[4], height[1], musgr[1].val,  0.0, 6.0, 0 ,  "Lacunarity" )
		musgr[2] = Slider( "Octs: ",       Btn_Evt, col[0], row[4], width[4], height[1], musgr[2].val,    0, 12,  0 , "Octaves" )
	if NType.val in [1,2,3,13,14,15,18,19,20,21]:
		musgr[3] = Slider( "Offst: ",      Btn_Evt, col[0], row[7], width[4], height[1], musgr[3].val,  0.0, 6.0, 0 ,  "Offset" )
	if NType.val in [1,2,13,15,18]:
		musgr[4] = Slider( "Gain: ",       Btn_Evt, col[0], row[8], width[4], height[1], musgr[4].val,  0.0, 6.0, 0 ,  "Gain" )
	if NType.val == 19:
		musgr[5] = Slider( "Thresh: ",     Btn_Evt, col[0], row[8], width[4], height[1], musgr[5].val,  0.001, 2.0, 0 ,  "Threshold" )
	if NType.val in [5,6,7,16]:
		turbOne[0] = Number( "Depth:",     Btn_Evt, col[0], row[4], width[4], height[1], turbOne[0].val, 0, 12, "Octaves")
		turbOne[1] = Toggle( "Hard noise", Btn_Evt, col[0], row[5], width[4], height[1], turbOne[1].val,        "Soft noise / Hard noise")
		turbOne[2] = Slider( "Amp:",       Btn_Evt, col[0], row[6], width[4], height[1], turbOne[2].val, 0.0, 3.0, 0, "Ampscale ")
		turbOne[3] = Slider( "Freq:",      Btn_Evt, col[0], row[7], width[4], height[1], turbOne[3].val, 0.0, 6.0, 0, "Freqscale")
	if NType.val in [18,19]:
		tBasismod = Menu( tBasismodemenu,  Btn_Evt, col[0], row[9], width[4], height[1], tBasismod.val, "Terrain basis mode.")
	if NType.val == 6:
		if voron[0].val == 6:
			voron[1] = Slider( "Exp: ",      Btn_Evt, col[0], row[8], width[4], height[1], voron[1].val, 0.0,10.0, 0, "Minkovsky exponent")
	if NType.val in [7,11,12,14,20,21]:
		vlnoi[0] = Slider( "Dist: ",       Btn_Evt, col[0], row[8], width[4], height[1], vlnoi[0].val,  0.0, 10.0, 0 , "Distort" )
	if NType.val in [7,13,14,15,21]:
		vlnoi[1] = Menu(noisebasismenu,    Btn_Evt, col[0], row[9], width[4], height[1], vlnoi[1].val, "Distortion Noise")
	if NType.val == 10:
		marbleOne[0] = Number(  "Depth: ", Btn_Evt, col[0], row[6], width[4], height[1], marbleOne[0].val, 0, 12, "Octaves")
		marbleOne[2] = Slider(  "Turb: ",  Btn_Evt, col[0], row[7], width[4], height[1], marbleOne[2].val,  0.0,20.0, 0, "Turbulence ")
		marbleOne[3] = Menu( biastypemenu, Btn_Evt ,col[0], row[4], width[4], height[1], marbleOne[3].val, "Bias")
		marbleOne[5] = Slider("ReScale: ", Btn_Evt, col[0], row[8], width[4], height[1], marbleOne[5].val,  0.0,20.0, 0, "ReScale")
		if marbleOne[3].val != 4:
			marbleOne[4] = Menu(sharptypemenu, Btn_Evt ,col[0], row[5], width[4], height[1], marbleOne[4].val, "Sharpen")

	RandMod = Menu( randomtypemenu, No_Evt, col[0], row[10], width[0], height[1], RandMod.val, "Random Type" )
	rand_H = Toggle("TH",No_Evt ,col[1], row[10], width[0], height[1], rand_H.val, "Randomise Terrain Height ( in Height panel )")
	rand_I = Toggle("NH",No_Evt ,col[2], row[10], width[0], height[1], rand_I.val, "Randomise Noise Height")
	rand_S = Toggle("NS",No_Evt ,col[3], row[10], width[0], height[1], rand_S.val, "Randomise Noise Size")
	rand_L = Toggle("NL",No_Evt ,col[4], row[10], width[0], height[1], rand_L.val, "Randomise Noise Location")

###-------------------------
## Effect Buttons:
#
def EffectButtons( col, row, width, height ):
	global Effect_Type, Effect_Ctrl, Blend_Effect, CustomFX
	global NSize, iScale, Offset, Invert, Lx, Ly, Sx, Sy
	global BasisTwo, turbTwo, marbleTwo, vlnoiTwo, musgrTwo

	Effect_Ctrl[0] = Menu( '|'.join( effecttypemenu ), Btn_Evt, col[0], row[2], width[4], height[2], Effect_Ctrl[0].val, "Effect: Type" )
	if Effect_Ctrl[0].val != 0:
		Effect_Ctrl[1] = Menu( '|'.join( mixtypemenu ),    Btn_Evt, col[5], row[2], width[4], height[2], Effect_Ctrl[1].val, "Mix: Type" )
		if Effect_Ctrl[1].val in [10,11]:
			Effect_Ctrl[2] = Slider("Warp: ",   Btn_Evt, col[5], row[3], width[4], height[1], Effect_Ctrl[2].val, -1.0, 1.0, 0, "Mix factor / Warp amount" )
		else: Effect_Ctrl[2] = Slider("Mix: ",Btn_Evt, col[5], row[3], width[4], height[1], Effect_Ctrl[2].val, -1.0, 1.0, 0, "Mix factor / Warp amount" )

		iScale[1] = Number("iScale:",   Btn_Evt, col[5], row[4], width[3], height[1], iScale[1].val,   -20.0, 20.0 , "Effect: Intensity Scale " )
		Invert[1] = Toggle("Inv.",      Btn_Evt, col[9], row[4], width[0], height[1], Invert[1].val, "Effect: Invert")
		Offset[1] = Number("Offset:",   Btn_Evt, col[5], row[5], width[4], height[1], Offset[1].val,   -20.0, 20.0 , "Effect: Offset " )
		NSize[1]  = Number("Frequency:",Btn_Evt, col[5], row[6], width[4], height[1], NSize[1].val, 0.001, 100.0, "Effect Frequency ( Scale )" )
		Sx[1]     = Number("Freq X:",   Btn_Evt, col[5], row[7], width[4], height[1],  Sx[1].val,    -50.0, 50.0 , "Effect Frequency X ( ScaleX )" )
		Sy[1]     = Number("Freq Y:",   Btn_Evt, col[5], row[8], width[4], height[1],  Sy[1].val,    -50.0, 50.0 , "Effect Frequency Y ( ScaleY )" )
		Lx[1]     = Number("Loc X:",    Btn_Evt, col[5], row[9], width[4], height[1],  Lx[1].val, -1000.0, 1000.0 , "Effect Loc X" )
		Ly[1]     = Number("Loc Y:",    Btn_Evt, col[5], row[10], width[4], height[1], Ly[1].val, -1000.0, 1000.0 , "Effect Loc Y" )

		if Effect_Ctrl[0].val == 1:
			PushButton("Load Image",   Load_Evt, col[0], row[4], width[4], height[2] , "Load Image")
			PushButton("Select Image", Sel_Evt,  col[0], row[6], width[4], height[3] , "Select Image")
			draw_Text( ( col[0]+5, row[7] ), effect_image, black, 1 )

		if Effect_Ctrl[0].val in [2,3,4,5,6,7,8,9]:
				Basis[1] = Menu( noisebasismenu,   Btn_Evt, col[0], row[3], width[4], height[1], Basis[1].val, "Basis" )

		if Effect_Ctrl[0].val == 2:
				turbTwo[0] = Number(  "Depth:",    Btn_Evt, col[0], row[4], width[4], height[1], turbTwo[0].val, 1, 12, "Octaves")
				turbTwo[1] = Toggle("Hard noise",  Btn_Evt, col[0], row[5], width[4], height[1], turbTwo[1].val,        "Hard noise")
				turbTwo[2] = Slider(    "Amp:",    Btn_Evt, col[0], row[6], width[4], height[1], turbTwo[2].val, 0.0, 3.0, 0, "Ampscale ")
				turbTwo[3] = Slider(   "Freq:",    Btn_Evt, col[0], row[7], width[4], height[1], turbTwo[3].val, 0.0, 6.0, 0, "Freqscale")
		if Effect_Ctrl[0].val == 3:
				vlnoiTwo[1] = Menu(noisebasismenu, Btn_Evt, col[0], row[4], width[4], height[1], vlnoiTwo[1].val, "Distortion Noise")
				vlnoiTwo[0] = Slider( "Dist: ",    Btn_Evt, col[0], row[5], width[4], height[1], vlnoiTwo[0].val,  0.0, 10.0, 0 , "Distort" )
		if Effect_Ctrl[0].val == 4:
				marbleTwo[0] = Number(  "Depth: ", Btn_Evt, col[0], row[6], width[4], height[1], marbleTwo[0].val, 1, 12, "Octaves")
				marbleTwo[2] = Slider(  "Turb: ",  Btn_Evt, col[0], row[7], width[4], height[1], marbleTwo[2].val,  0.0,20.0, 0, "Turbulence")
				marbleTwo[3] = Menu( biastypemenu, Btn_Evt ,col[0], row[4], width[4], height[1], marbleTwo[3].val, "Bias")
				marbleTwo[5] = Slider("ReScale: ", Btn_Evt, col[0], row[8], width[4], height[1], marbleTwo[5].val,  0.0,20.0, 0, "ReScale")
				if marbleTwo[3].val != 4:
					marbleTwo[4] = Menu(sharptypemenu,Btn_Evt ,col[0], row[5], width[4], height[1], marbleTwo[4].val, "Sharpen")

		if Effect_Ctrl[0].val in [5,6,7,8,9]:
			musgrTwo[0] = Slider( "H: ",     Btn_Evt, col[0], row[5], width[4], height[1], musgrTwo[0].val,  0.0, 3.0, 0 ,  "H" )
			musgrTwo[1] = Slider( "Lacu: ",  Btn_Evt, col[0], row[6], width[4], height[1], musgrTwo[1].val,  0.0, 6.0, 0 ,  "Lacunarity" )
			musgrTwo[2] = Slider( "Octs: ",  Btn_Evt, col[0], row[4], width[4], height[1], musgrTwo[2].val,    0, 12,  0 , "Octaves" )
		if Effect_Ctrl[0].val in [6,7,8]:
			musgrTwo[3] = Slider( "Offst: ", Btn_Evt, col[0], row[7], width[4], height[1], musgrTwo[3].val,  0.0, 6.0, 0 ,  "Offset" )
		if Effect_Ctrl[0].val in [6,7]:
			musgrTwo[4] = Slider( "Gain: ",  Btn_Evt, col[0], row[8], width[4], height[1], musgrTwo[4].val,  0.0, 6.0, 0 ,  "Gain" )

		if Effect_Ctrl[0].val > 9 and Effect_Ctrl[0].val < 31:
				Effect_Ctrl[5] = Number("Depth:",  Btn_Evt, col[0], row[4], width[4], height[1], Effect_Ctrl[5].val,   0, 12   ,  "Fractalize Effect: Octaves" )
				Effect_Ctrl[4] = Number("Distort:",Btn_Evt, col[0], row[7], width[4], height[1], Effect_Ctrl[4].val, 0.0, 50.0 ,  "Distort Effect: Amount" )
				Effect_Ctrl[6] = Slider("Freq:",   Btn_Evt, col[0], row[5], width[4], height[1], Effect_Ctrl[6].val, 0.0, 6.0, 0, "Fractalize Effect: Frequency" )
				Effect_Ctrl[7] = Slider("Amp:",    Btn_Evt, col[0], row[6], width[4], height[1], Effect_Ctrl[7].val, 0.0, 3.0, 0, "Fractalize Effect: Amplitude" )
				if Effect_Ctrl[0].val < 22:
					Effect_Ctrl[3] = Menu(biastypemenu, Btn_Evt ,col[0], row[3], width[4], height[1], Effect_Ctrl[3].val, "Effect bias")
		if Effect_Ctrl[0].val in [31,32]:
				Effect_Ctrl[8] = Number("Amount:", Btn_Evt, col[0], row[4], width[4], height[1], Effect_Ctrl[8].val, -20.0, 20.0, "Effect: Amount" )
		if Effect_Ctrl[0].val == 33:
			draw_Text( ( col[0]+5, row[4] ), 'Custom math ( h, x,y, a,b )' , black, 0 )
			CustomFX[0] = String(      "a = ", Btn_Evt, col[0], row[5], width[4], height[1] ,CustomFX[0].val,96, "a" )
			CustomFX[1] = String(      "b = ", Btn_Evt, col[0], row[6], width[4], height[1] ,CustomFX[1].val,96, "b" )
			CustomFX[2] = String( "result = ", Btn_Evt, col[0], row[7], width[4], height[1] ,CustomFX[2].val,96, "result" )
	
###-------------------------
## Filter / Height Buttons:
#
def FilterButtons( col, row, width, height ):
	global iScale, Offset, Invert, Min, Max, Falloff, CustomFilt
	global Filter_Mode, Def_Filter_Ctrl, Ipo_Filter_Ctrl, DefaultIpoName, Filter_Order

	iScale[2] = Number("Height:", Btn_Evt, col[5], row[2], width[3], height[2], iScale[2].val,   -10.0, 10.0 , "Terrain Height:  Scale" )
	Invert[2] = Toggle("Inv.",    Btn_Evt, col[9], row[2], width[0], height[2], Invert[2].val, "Terrain Height:  Invert")
	Offset[2] = Number("Offset:", Btn_Evt, col[5], row[3], width[4], height[1], Offset[2].val,   -10.0, 10.0 , "Terrain Height:  Offset" )
	Max = Number(    "Plateau:",  Btn_Evt, col[5], row[5], width[4], height[1], Max.val, Min.val, 1.0 , "Terrain Height:  Clamp Max. ( Plateau )" )
	Min = Number(    "Sealevel:", Btn_Evt, col[5], row[6], width[4], height[1], Min.val, -1.0, Max.val , "Terrain Height:  Clamp Min. ( Sealevel )" )
	Falloff[0] = Menu( falloftypemenu, Btn_Evt ,col[5], row[9], width[4], height[2], Falloff[0].val, "Terrain Height:  Edge falloff")
	if Falloff[0].val !=0:
		Falloff[1] = Number("X:",   Btn_Evt, col[5], row[10], width[1], height[1], Falloff[1].val , 0.01, 100.0 , "Edge falloff:  X Size" )		
		Falloff[2] = Number("Y:",   Btn_Evt, col[8], row[10], width[1], height[1], Falloff[2].val , 0.01, 100.0 , "Edge falloff:  Y Size" )
		Falloff[4] = Toggle("Inv.", Btn_Evt, col[7], row[10], width[0], height[1], Falloff[4].val, "Edge falloff:  Invert")
		Falloff[3] = Toggle("Edge At Sealevel", Btn_Evt, col[5], row[7], width[4], height[1], Falloff[3].val, "Edge falloff:  Edge at Sealevel")

	Filter_Mode = Menu( filtermodemenu,               No_Evt, col[0], row[2], width[4], height[2], Filter_Mode.val, "Filter:  Mode")
	if Filter_Mode.val ==1:
		Def_Filter_Ctrl[0] = Menu( filtertypemenu,     Btn_Evt, col[0], row[5], width[4], height[2], Def_Filter_Ctrl[0].val, "Filter:  Type")
		Def_Filter_Ctrl[1] = Number("Amount: ",        Btn_Evt, col[0], row[6], width[4], height[1], Def_Filter_Ctrl[1].val, 0.1, 100.0 , "Filter:  Amount" )

	if Filter_Mode.val ==2:
		Ipo_Filter_Ctrl[0] = String("IP:",              Ipo_Evt, col[0], row[5], width[4], height[2], Ipo_Filter_Ctrl[0].val,20, "Ipo datablock name" )
		if Ipo_Filter_Ctrl[0].val !='':
			Ipo_Filter_Ctrl[1] = Number("Use This Curve:",Ipo_Evt, col[0], row[7], width[4], height[3], Ipo_Filter_Ctrl[1].val, 0, 29, "Select curve to use" )
			Ipo_Filter_Ctrl[2] = Number("Curve Length:",  Ipo_Evt, col[0], row[8], width[4], height[1], Ipo_Filter_Ctrl[2].val, 0.0, 1000.0, "X: Length (number of frames) of the selected curve." )
			Ipo_Filter_Ctrl[3] = Number("Curve Height:",  Ipo_Evt, col[0], row[9], width[4], height[1], Ipo_Filter_Ctrl[3].val, 0.0, 1000.0, "Y: Height (offset) of the selected curve." )
		else:
			draw_Text( ( col[0]+5, row[6] ), 'Enter Ipo DataBlock name,' , black, 0 )
			draw_Text( ( col[0]+5, row[9] ), 'or else:' , black, 0 )
		PushButton( "New IpoDataBlock/Object", New_Ipo_Evt, col[0], row[10], width[4], height[1], "Creates new Ipo Object(Empty), and Ipo DataBlock(curves)' to use as height filter")

	if Filter_Mode.val ==3:
		draw_Text( ( col[0]+5, row[4] ), 'Custom math ( h, x,y, a,b )' , black, 0 )
		CustomFilt[0] = String(      "a = ", Btn_Evt, col[0], row[5], width[4], height[1] ,CustomFilt[0].val,96, "a" )
		CustomFilt[1] = String(      "b = ", Btn_Evt, col[0], row[6], width[4], height[1] ,CustomFilt[1].val,96, "b" )
		CustomFilt[2] = String( "result = ", Btn_Evt, col[0], row[7], width[4], height[1] ,CustomFilt[2].val,96, "result" )
	if Filter_Mode.val !=0:
		Filter_Order = Toggle("Change Filter Order",   Btn_Evt, col[0], row[3], width[4], height[1], Filter_Order.val, "Filter Order: OFF = Noise+Effect+Falloff+FILTER / ON = Noise+FILTER+Effect+Falloff.")

###-------------------------
## Option / Generate Image Buttons:
#
def OptionButtons( col, row, width, height ):
	global PreView, previewname

	PreView[0] = Toggle("Make Image", No_Evt, col[0], row[2], width[4], height[2], PreView[0].val, "Image: On/Off (Make a new Image in UV/ImageEditor Window, and give a name to it)")
	if PreView[0].val !=0:
		previewname = String( "IM:", No_Evt, col[0], row[3], width[4], height[1] ,previewname.val, 16, "IM:Name, Render terrain height to this image" )
		PreView[1] = Number("",      Im_Evt, col[0], row[4], width[1], height[1], PreView[1].val, 0.0, 10.0, "Image: Intensity Scale")
		PreView[2] = Number("",      Im_Evt, col[3], row[4], width[1], height[1], PreView[2].val,-10.0, 10.0, "Image: Offset")
		PushButton(   "Draw Image",  Im_Evt, col[0], row[5], width[4], height[1] , "Image: Update image ( KEY: V )")
		draw_Text( ( col[5], row[1] ), 'Create yourself a new image', black, 0 )
		draw_Text( ( col[5], row[2] ), 'in UV/Image Editor Window,', black, 0 )
		draw_Text( ( col[5], row[3] ), 'give it a name,', black, 0 )
		draw_Text( ( col[5], row[4] ), 'and save it manualy.', black, 0 )

####--------------------------------------------------------------------------
###--------------------------------------------------------------------------
## Draw G.U.I. -------------------------------------------------------------
#--------------------------------------------------------------------------
def drawgui():
	global guitabs, ledcolor, FullScreen, AutoUpd, RandMod, RSeed, filemessage
	global Effect_Ctrl, Filter_Mode, Falloff, PreView, rand_H, rand_S, rand_L, rand_I

	glClearColor(background[0],background[1],background[2],background[3])
	glClear(GL_COLOR_BUFFER_BIT)
	scissorbox=Buffer(GL_FLOAT,4)
	glGetFloatv(GL_SCISSOR_BOX,scissorbox)
	scissbleft=int(scissorbox[0])
	scissbbase=int(scissorbox[1])
	scissbwidth=int(scissorbox[2])
	scissbheight=int(scissorbox[3])
	xstart = 5
	ystart = 5
	xgap = 5
	ygap = 5
	if FullScreen.val==1:
		guiwidth  = scissbwidth-10
		guiheight = scissbheight-25
		if guiwidth < size_x/2:
			guiwidth = size_x/2
		if guiheight < size_y/2:
			guiheight = size_y/2
	else:
		guiwidth = size_x
		guiheight = size_y
	col,row = [],[]
	xpart = ( ( guiwidth-xstart ) / columns )
	ypart = ( ( guiheight-ystart ) / rows )
	width = []
	for c in xrange( columns ):
		col.append( xgap + xpart * c + xstart )
		width.append( xpart*(c+1)-xgap )
	height = [ (ypart-ygap)/2 , ypart-ygap, (ypart*3-ygap)/2, ypart*2-ygap, (ypart*5-ygap)/2  ]
	for r in xrange( rows ):
		row.append( ygap + ypart * r + ystart )
	row.reverse()

	###-------------------------
	## Draw G.U.I.:
	draw_BackPanel( 'Another Noise Tool 1.05', xstart, ystart, guiwidth, guiheight + ygap, lightgrey )

	FullScreen = Toggle("", Scrn_Evt, guiwidth-32, guiheight+ygap+3, 15, 15, FullScreen.val ,"FullScreen" )
	PushButton( "X",         End_Evt, guiwidth-16, guiheight+ygap+3, 15, 15, "Exit" )
	draw_Text(( guiwidth-(guiwidth/2)-width[0], guiheight+ygap+5 ), filemessage, white, 0 )

	# gui tabs
	guitabs[0] = Toggle("Main",    gt0_Evt, col[0], row[0], width[1], height[1], guitabs[0].val ,"Main / Mesh settings" )
	guitabs[1] = Toggle("Noise",   gt1_Evt, col[2], row[0], width[1], height[1], guitabs[1].val ,"Noise settings" )
	guitabs[2] = Toggle("Effect",  gt2_Evt, col[4], row[0], width[1], height[1], guitabs[2].val ,"Add Effect" )
	guitabs[3] = Toggle("Height",  gt3_Evt, col[6], row[0], width[1], height[1], guitabs[3].val ,"Height Filter" )
	guitabs[4] = Toggle("Options", gt4_Evt, col[8], row[0], width[1], height[1], guitabs[4].val ,"Options" )

	if   guitabs[0].val !=0: MeshButtons(   col, row, width, height )
	elif guitabs[1].val !=0: NoiseButtons(  col, row, width, height )
	elif guitabs[2].val !=0: EffectButtons( col, row, width, height )
	elif guitabs[3].val !=0: FilterButtons( col, row, width, height )
	elif guitabs[4].val !=0: OptionButtons( col, row, width, height )
	else: # some info
		draw_Panel(  col[0], row[0]-5, col[9]+width[0], row[10]-10, black )
		draw_Text( ( col[0]+5, row[1] ), 'Another Noise Tool v.1.05',                     ledcolors[0], 2 )
		draw_Text( ( col[0]+5, row[2] ), 'by: Jimmy Hazevoet, 01/2005-03/2007',           ledcolors[1], 2 )
		draw_Text( ( col[0]+5, row[3] ), 'v.1.05: build/tested in: Blender 2.43 (Wndws)', ledcolors[2], 1 )
		draw_Text( ( col[0]+5, row[4] ), 'HotKeys: ----------------------------',         ledcolors[3], 2 )
		draw_Text( ( col[0]+5, row[5] ), 'Space = Update mesh',                           ledcolors[4], 2 )
		draw_Text( ( col[0]+5, row[6] ), 'V = Update Image',                              ledcolors[5], 2 )
		draw_Text( ( col[0]+5, row[7] ), 'R = Randomise',                                 ledcolors[6], 2 )
		draw_Text( ( col[0]+5, row[8] ), 'L = Load from file',                            ledcolors[7], 2 )
		draw_Text( ( col[0]+5, row[9] ), 'S = Save to file',                              ledcolors[8], 2 )
		draw_Text( ( col[0]+5, row[10] ),'Q = Quit',                                      ledcolors[9], 2 )

	# auto/generate/randomise buttons
	rand_on_off = 0
	if rand_H.val !=0:   rand_on_off = 1
	elif rand_I.val !=0: rand_on_off = 1
	elif rand_S.val !=0: rand_on_off = 1
	elif rand_L.val !=0: rand_on_off = 1
	else:                rand_on_off = 0
	if rand_on_off != 0:
		if RandMod.val in [1,2]:
			PushButton(     "Randomise", Rndm_Evt, col[2], row[12], width[2], height[2] , "Randomise Noise ( KEY: R )")
		else: RSeed	= Number("Seed: ", Rndm_Evt, col[2], row[12], width[2], height[2], RSeed.val,  0, 255 , "Random Seed: If seed = 0, the current time will be used as seed." )
		AutoUpd = Toggle("Auto", No_Evt, col[0], row[12], width[1], height[2], AutoUpd.val ,"Automatic update" )
		PushButton("Update", Upd_Evt, col[5], row[12], width[4], height[2] , "Generate / Update. ( KEY: SPACE )")
	else:
		AutoUpd = Toggle("Auto", No_Evt, col[0], row[12], width[1], height[2], AutoUpd.val ,"Automatic update" )
		PushButton("Update", Upd_Evt, col[2], row[12], width[7], height[2] , "Generate / Update. ( KEY: SPACE )")
		####---------------------------------------------------------------------------

###---------------------------------------------------------------------------
##---------------------------------------------------------------------------
# Key Events:

def events(evt, val):
	global PreView, txtFile, AutoUpd, PreView

	## hotkey: Q = Quit
	if (evt == QKEY and not val):
		name = "Quit ?%t|No %x0|Yes %x1"
		result = Blender.Draw.PupMenu(name)
		if result==1:
			Exit()

	## hotkey: Space = Generate/Update terrain
	if (evt == SPACEKEY and not val):
		do_it()

	## hotkey: R = Randomise noise
	if (evt in [ RKEY ] and not val):
		if AutoUpd.val != 0:
			do_it_random()
		else:
			randomiseNoise()
			Draw()

	## hotkey: V = Update image
	if PreView[0].val != 0:
		if (evt in [ VKEY, RKEY ] and not val):
			do_it_preview()

	## hotkey: L = Load from file
	if (evt == LKEY and not val):
		loadmenu = "Load file ?%t|" + txtFile.val
		loadresult = Blender.Draw.PupMenu(loadmenu)
		if loadresult==1:
			LoadPreset(txtFile.val)
			if AutoUpd.val != 0:
				do_it()
			else: Draw()

	## hotkey: S = Save to file
	if (evt == SKEY and not val):
		savemenu = "Save file ?%t|" + txtFile.val
		saveresult = Blender.Draw.PupMenu(savemenu)
		if saveresult==1:
			SavePreset(txtFile.val)
			Draw()

###---------------------------------------------------------------------------
##---------------------------------------------------------------------------
# Button events:

def bevents(evt):
	global txtFile, effect_image, PreView, fileinfo, filemessage
	global Filter_Mode, Ipo_Filter_Ctrl, iponame, thiscurve, selectedcurve
	global antfilename, terrainname
	global actob, actme

	# quit/reset event
	if (evt == End_Evt ):
		name = "OK ?%t|Reset %x1|Quit %x2"
		result = Blender.Draw.PupMenu(name)
		if result==1:
			Set_ReSet_Values()
			Draw()
		elif result==2:
			Exit()

	## file info string  event
	if (evt == UseMe_Evt ):
		result = Blender.Draw.PupStrInput("Info:  ", fileinfo, 96)
		if result:
			fileinfo = result
			Draw()
		else: return

	## none event
	if (evt in [No_Evt, Scrn_Evt] ):
		Draw()

	## image event
	if (evt == Im_Evt ):
		do_it_preview()

	## generate/update event
	if (evt == Upd_Evt ):
		if PreView[0].val != 0:
			do_it_preview()
		do_it()

	## mesh button event
	if (evt == Msh_Evt):
		if AutoUpd.val != 0:
			do_it()
		else: Draw()

	## button event
	if (evt == Btn_Evt ):
		if AutoUpd.val != 0:
			if PreView[0].val != 0:
				do_it_preview()
			do_it()
		else: Draw()

	## randomise event
	if (evt == Rndm_Evt ):
		if AutoUpd.val != 0:
			do_it_random()
		else:
			randomiseNoise()
			if PreView[0].val != 0:
				do_it_preview()
			Draw()

	###---------------------------------------------------------
	## Effect Image Load/Select:
	if (evt == Load_Evt ):
		Blender.Window.FileSelector ( load_image, 'LOAD IMAGE')
	if (evt == Sel_Evt ):
		try: effect_image = Image_Menu()
		except: pass
		if AutoUpd.val != 0:
			do_it()
		else: Draw()

	###---------------------------------------------------------
	## Make New IPOCurve to use as Filter:
	if (evt == New_Ipo_Evt ):
		objname = Create("ANT_Ipo_Empty")
		iponame = Create("ANT_IPO")
		block = []
		block.append("Enter new names")
		block.append("and hit OK button")
		block.append(("OB: ", objname, 0, 30, "New Ipo Object Name. (Object type = 'Empty')"))
		block.append(("IP: ", iponame, 0, 30, "New Ipo DataBlock Name"))
		block.append("Open IpoCurveEditor")
		block.append("select Ipo DataBlock" )
		block.append("'Pin' the view" )
		block.append("and edit the curves." )
		retval = PupBlock("Make A.N.T. IpoCurve Object", block)
		if retval !=0:
			ANTAutoIpo(  objname.val, iponame.val )
			Ipo_Filter_Ctrl[0].val = iponame.val

	###---------------------------------------------------------
	## get IPOCurve to use as Filter:
	if (evt in [Ipo_Evt, New_Ipo_Evt] ):
		if Filter_Mode.val == 2:
			if AutoUpd.val != 0:				
				try:
					ipoblockname  = Ipo.Get( Ipo_Filter_Ctrl[0].val )
					thiscurve     = ipoblockname.getCurves()
					selectedcurve = thiscurve[ Ipo_Filter_Ctrl[1].val ]
					if PreView[0].val != 0:
						do_it_preview()
					#if AutoUpd.val != 0:
					do_it()
				except: pass
			else:
					try:
						ipoblockname  = Ipo.Get( Ipo_Filter_Ctrl[0].val )
						thiscurve     = ipoblockname.getCurves()
						selectedcurve = thiscurve[ Ipo_Filter_Ctrl[1].val ]
						if PreView[0].val != 0:
							do_it_preview()
						else:
							Draw()
					except: pass

	###---------------------------------------------------------
	## gui tabs
	if (evt == gt0_Evt ):
		if guitabs[0].val == 1:
			guitabs[1].val = ( 0 )
			guitabs[2].val = ( 0 )
			guitabs[3].val = ( 0 )
			guitabs[4].val = ( 0 )
		Draw()
	if (evt == gt1_Evt ):
		if guitabs[1].val == 1:
			guitabs[0].val = ( 0 )
			guitabs[2].val = ( 0 )
			guitabs[3].val = ( 0 )
			guitabs[4].val = ( 0 )
		Draw()
	if (evt == gt2_Evt ):
		if guitabs[2].val == 1:
			guitabs[0].val = ( 0 )
			guitabs[1].val = ( 0 )
			guitabs[3].val = ( 0 )
			guitabs[4].val = ( 0 )
		Draw()
	if (evt == gt3_Evt ):
		if guitabs[3].val == 1:
			guitabs[0].val = ( 0 )
			guitabs[1].val = ( 0 )
			guitabs[2].val = ( 0 )
			guitabs[4].val = ( 0 )
		Draw()
	if (evt == gt4_Evt ):
		if guitabs[4].val == 1:
			guitabs[0].val = ( 0 )
			guitabs[1].val = ( 0 )
			guitabs[2].val = ( 0 )
			guitabs[3].val = ( 0 )
		Draw()

	###---------------------------------------------------------
	## load and save all settings:
	if (evt == SelFile_Evt ):
		Blender.Window.FileSelector ( callback, "Select .ant File")
	if (evt == LoadFile_Evt ):
		loadmenu = "Load file ?%t|" + txtFile.val
		loadresult = Blender.Draw.PupMenu(loadmenu)
		if loadresult==1:
			LoadPreset(txtFile.val)
			Draw()
			if AutoUpd.val != 0:
				do_it()
	if (evt == SaveFile_Evt ):
		savemenu = "Save file ?%t|" + txtFile.val
		saveresult = Blender.Draw.PupMenu(savemenu)
		if saveresult==1:
			SavePreset(txtFile.val)
			Draw()

	###---------------------------------------------------------
	# New Grid
	###-------------------------
	if (evt == New_Evt):
		scn = Blender.Scene.GetCurrent()
		gridname = Create("Terrain")
		gridres = Create(256)
		curspos = Create(0)
		block = []
		block.append(("OB: ", gridname, 0, 30, "New Object Name."))
		block.append(("Resol: ", gridres, 4, 1024, "New grid resolution"))
		block.append(("At Cursor", curspos, "New grid at cursor position"))			
		retval = PupBlock("New Grid Mesh", block)
		if retval !=0:
			MakeGridMesh( gridres.val, gridname.val, curspos.val, scn )
			obj = scn.objects.active
			if obj.type == 'Mesh':
				actob=[]
				actme=[]		
				actob.append( obj )
				actme.append( actob[0].getData(mesh=1) )
				Blender.Redraw()

	###---------------------------------------------------------
	# Assign Grid
	###-------------------------
	if (evt == App_Evt):
		scn = Blender.Scene.GetCurrent()
		obj = scn.objects.active
		if obj:
			if obj.type == 'Mesh':
				actob=[]
				actme=[]		
				actob.append( obj )
				actme.append( actob[0].getData(mesh=1) )
				Draw()

	###-------------------------
	if (evt not in [LoadFile_Evt,SaveFile_Evt] ):
		filemessage = ''
		#Draw()

	### end events. -------------------------

####-------------------------------------------------------------------------------------
###-------------------------------------------------------------------------------------
##-------------------------------------------------------------------------------------
#-------------------------------------------------------------------------------------

##----------------------------------
# A.N.T. Auto Ipo generator:
def ANTAutoIpo( objname, iponame ):
	scn=Scene.GetCurrent()
	# Deselect all objects:
	scn.objects.selected=[]
	# Create new 'ANT_IPO_OBJECT':
	obj = scn.objects.new('Empty', objname )
	obj.setDrawMode(8)
	obj.select(1)
	obj.layers = Window.ViewLayers()
	# Set current frame at 1:
	frame = Get('curframe')
	if frame !=1:
		Set('curframe',1)
		frame = Get('curframe')
	# Insert IpoKeys:
	obj.setLocation(0.0, 0.0, 0.0)
	obj.insertIpoKey(0)
	Set('curframe',101)
	obj.setLocation(100.0, 100.0, 100.0)
	obj.insertIpoKey(0)
	Set('curframe',1)
	# Set Ipo name:
	ip = obj.getIpo()
	ip.name = iponame
	#-------------------------
	print "New ANT_IPO: " + objname +" (Object) and " + iponame + " (Ipo DataBlock) Created!"
	#-------------------------

##-------------------------------------------------------------------------------------

##-------------------------
# Generate random numbers:
def randnum(low,high):
	global RandMod, RSeed
	if RandMod.val == 0:
		# Noise.random setRandomSeed
		s = Noise.setRandomSeed( RSeed.val )
		num = Noise.random()
		num = num*(high-low)
		num = num+low
	elif RandMod.val == 1:
		# Mathutils.Rand
		num = Mathutils.Rand(low,high)
	else:
		# BPyMathutils  Mersenne Twister genrand
		num = genrand()
		num = num*(high-low)
		num = num+low
	return num

##-------------------------
# Randomise noise: height, size and location:
def randomiseNoise():
	global rand_I, rand_H, rand_S, rand_L, NSize, iScale, Offset, Invert, Lx, Ly, Sx, Sy

	if rand_I.val !=0:
		iScale[0] = Create( randnum( 0.2  , 3.0 ) )
		Offset[0] = Create( randnum(-1.0 , 1.0 ) )
	if rand_H.val !=0:
		iScale[2] = Create( randnum( 0.10  , 1.0 ) )
		Offset[2] = Create( randnum(-0.25 , 0.25 ) )
	if rand_S.val !=0:
		NSize[0]  = Create( randnum( 0.25 , 2.5 ) )
		#Sx[0]     = Create( randnum( 0.5 , 1.5 ) )
		#Sy[0]     = Create( randnum( 0.5 , 1.5 ) )
	if rand_L.val !=0:
		Lx[0]     = Create( randnum( -10000 , 10000 ) )
		Ly[0]     = Create( randnum( -10000 , 10000 ) )

##-------------------------------------------------------------------------------------

###--------------------------
# Load Image:
def load_image( ImageFileName ):
	Image.Load( ImageFileName )

###--------------------------
# Select Image Menu:
def Image_Menu():
	try:
		names=[]
		imagelist = Image.Get()
		imagelist.reverse()
		for numbers, obnames in enumerate( imagelist ):
			n = obnames.getName()
			names.append( n )
		imlistText = string.join( [ '|' + str(names[key]) + '%x' + str(key)  for key in xrange(numbers+1) ], '' )
		image_menu = Blender.Draw.PupMenu( "Images: %t" + imlistText )
		if image_menu == -1:
			return ''
		return imagelist[ image_menu ].getName()
	except:
		return 'No image found!'

###--------------------------
# Get Image Pixels:
def Image_Func( x,y ):
	try:
		pic  = Image.Get( effect_image )
	except:
		return 0.0
	w, h = pic.getSize()
	x, y = x,-y
	x = int(w * ((x + 1.0) % 2.0) / 2.0)
	y = int((h-1) - h * ((y + 1.0) % 2.0) / 2.0)	
	c = pic.getPixelF( x,y )
	return ( c[0] + c[1] + c[2] ) / 3.0

##-------------------------------------------------------------------------------------

# Transpose noise coords:
def Trans((x,y,z), size, loc  ):
	x = ( x / size[1] / size[0] + loc[0] )
	y = ( y / size[2] / size[0] + loc[1] )
	z = 0.0 #( z / size[3] / size[0] + loc[2] )
	return x,y,z

# Transpose effect coords:
def Trans_Effect((x,y,z), size, loc  ):
	x = ( x * size[1] * size[0] + loc[0] )
	y = ( y * size[2] * size[0] + loc[1] )
	z = 0.0
	return x,y,z

# Height scale:
def HeightScale( input, iscale, offset, invert ):
	if invert !=0:
		return (1.0-input) * iscale + offset
	else:
		return input * iscale + offset

# dist.
def Dist(x,y):
	return sqrt( (x*x)+(y*y) )

##-----------------------------------
# bias types:
def no_bias(a):
	return a
def sin_bias(a):
	return 0.5 + 0.5 * sin(a)
def cos_bias(a):
	return 0.5 + 0.5 * cos(a)
def tri_bias(a):
	b = 2 * phi
	a = 1 - 2 * abs(floor((a * (1/b))+0.5) - (a*(1/b)))
	return a
def saw_bias(a):
	b = 2 * phi
	n = int(a/b)
	a -= n * b
	if a < 0: a += b
	return a / b
# sharpen types:
def soft(a):
	return a
def sharp(a):
	return a**0.5
def sharper(a):
	return sharp(sharp(a))
Bias_Types  = [ sin_bias, cos_bias, tri_bias, saw_bias, no_bias ]
Sharp_Types = [ soft, sharp, sharper ]

##-----------------------------------
# clamp height
def clamp( height, min, max ):
	if ( height < min ): height = min
	if ( height > max ): height = max
	return height

##-----------------------------------
# Mix modes
def maximum( a, b ):
	if ( a > b ): b = a
	return b
def minimum( a, b ):
	if ( a < b ): b = a
	return b

def Mix_Modes( (i,j),(x,y,z) , a,b, mixfactor, mode ):
	a = a * ( 1.0 - mixfactor )
	b = b * ( 1.0 + mixfactor )
	if   mode == 0:  return  ( b )                     #0  effect only
	elif mode == 1:  return  ( a*(1.0-0.5) + (b*0.5) ) #1  mix
	elif mode == 2:  return  ( a + b )                 #2  add
	elif mode == 3:  return  ( a - b )                 #3  sub.
	elif mode == 4:  return  ( a * b )                 #4  mult.
	elif mode == 5:  return  (abs( a - b ))            #5  abs diff.
	elif mode == 6:  return  1.0-((1.0-a)*(1.0-b)/1.0) #6  screen
	elif mode == 7:  return  ( a + b ) % 1.0           #7  addmodulo
	elif mode == 8:  return  min( a, b )           #8  min.
	elif mode == 9:  return  max( a, b )           #9  max.
	elif mode == 10:                                   #10 warp: effect
		noise =  mixfactor * Noise_Function(x,y,z)
		return    Effects( (i,j),(x+noise,y+noise,z) )
	elif mode == 11:                                   #11 warp: noise
		effect = mixfactor * Effects( (i,j),(x,y,z) )
		return   Noise_Function( x+effect, y+effect, z )
	else: return a

###----------------------------------------------------------------------
# Effect functions:

# Effect_Basis_Function:
def Effect_Basis_Function((x,y), type, bias ):

	iscale = 1.0
	offset = 0.0
	## gradient:
	if type == 0:
		effect = offset + iscale * ( Bias_Types[ bias ]( x + y ) )
	## waves / bumps:
	if type == 1:
		effect = offset + iscale * 0.5 * ( Bias_Types[ bias ]( x*phi ) + Bias_Types[ bias ]( y*phi ) )
	## zigzag:
	if type == 2:
		effect = offset + iscale * Bias_Types[ bias ]( offset + iscale * sin( x*phi + sin( y*phi ) ) )
	## wavy:	
	if type == 3:
		effect = offset + iscale * ( Bias_Types[ bias ]( cos( x ) + sin( y ) + cos( x*2+y*2 ) - sin( -x*4+y*4) ) )
	## sine bump:	
	if type == 4:
		effect =   offset + iscale * 1-Bias_Types[ bias ](( sin( x*phi ) + sin( y*phi ) ))
	## dots:
	if type == 5:
		effect = offset + iscale * ( Bias_Types[ bias ](x*phi*2) * Bias_Types[ bias ](y*phi*2) )-0.5
	## rings / dome:
	if type == 6:
		effect = offset + iscale * ( Bias_Types[ bias ]( 1.0-(x*x+y*y) ) )
	## spiral:
	if type == 7:
		effect = offset + iscale * Bias_Types[ bias ](( x*sin( x*x+y*y ) + y*cos( x*x+y*y ) ))*0.5
	## square / piramide:
	if type == 8:
		effect = offset + iscale * Bias_Types[ bias ](1.0-sqrt( (x*x)**10 + (y*y)**10 )**0.1)
	## blocks:	
	if type == 9:
		effect = ( 0.5-max( Bias_Types[ bias ](x*phi) , Bias_Types[ bias ](y*phi) ))
		if effect > 0.0: effect = 1.0
		effect = offset + iscale * effect
	## grid:	
	if type == 10:
		effect = ( 0.025-min( Bias_Types[ bias ](x*phi) , Bias_Types[ bias ](y*phi) ))
		if effect > 0.0: effect = 1.0
		effect = offset + iscale * effect
	## tech:
	if type == 11:
		a = ( max( Bias_Types[ bias ](x*pi) , Bias_Types[ bias ](y*pi) ))
		b = ( max( Bias_Types[ bias ](x*pi*2+2) , Bias_Types[ bias ](y*pi*2+2) ))
		effect = ( min( Bias_Types[ bias ](a) , Bias_Types[ bias ](b) ))*3.0-2.0
		if effect > 0.5: effect = 1.0
		effect = offset + iscale * effect

	## crackle:	
	if type == 12:
		t = turbulence(( x, y, 0 ), 6, 0, 0 ) * 0.25
		effect = vlNoise(( x, y, t ), 0.25, 0, 8 )
		if effect > 0.5: effect = 0.5
		effect = offset + iscale * ( effect )
	## sparse cracks noise:
	if type == 13:
		effect = 2.5 * abs( noise((x*0.5,y*0.5, 0 ), 1 ) )-0.1
		if effect > 0.25: effect = 0.25
		effect = offset + iscale * ( effect * 2.5 )
	## shattered rock noise:
	if type == 14:
		effect = 0.5 + noise((x,y,0), 7 )
		if effect > 0.75: effect = 0.75
		effect = offset + iscale * effect
	## lunar noise:
	if type == 15:
		effect = 0.25 + 1.5 * voronoi(( x+2, y+2, 0 ), 1 )[0][0]
		if effect > 0.5: effect = 0.5
		effect = offset + iscale * ( effect * 2.0 )
	## cosine noise:
	if type == 16:
		effect = cos( 5*noise(( x, y, 0 ), 0 ) )
		effect = offset + iscale * ( effect*0.5 )
	## spikey noise:
	if type == 17:
		n = 0.5 + 0.5 * turbulence(( x*5, y*5, 0 ), 8, 0, 0 )
		effect = ( ( n*n )**5 )
		effect = offset + iscale * effect
	## stone noise:
	if type == 18:
		effect = offset + iscale *( noise((x*2,y*2, 0 ), 0 ) * 1.5 - 0.75)
	## Flat Turb:
	if type == 19:
		t = turbulence(( x, y, 0 ), 6, 0, 0 )
		effect = t*2.0
		if effect > 0.25: effect = 0.25
		effect = offset + iscale * ( effect )
	## Flat Voroni:
	if type == 20:
		t = 1-noise(( x, y, 0 ), 3 )
		effect = t*2-1.75
		if effect > 0.25: effect = 0.25
		effect = offset + iscale * ( effect )

	if effect < 0.0: effect = 0.0
	return effect

# fractalize Effect_Basis_Function: ------------------------------ 
def Effect_Function((x,y), type,bias, turb, depth,frequency,amplitude ):

	## effect distortion:
	if turb != 0.0:
		t =  vTurbulence(( x, y, 0 ), 6, 0, 0 )
		x = x + ( 0.5 + 0.5 * t[0] ) * turb
		y = y + ( 0.5 + 0.5 * t[1] ) * turb

	result = Effect_Basis_Function((x,y), type, bias )
	## fractalize effect:
	if depth != 0:
		i=0
		while i < depth:
			i+=1
			x *= frequency
			y *= frequency
			amplitude = amplitude / i
			result += Effect_Basis_Function( (x,y), type, bias ) * amplitude
	return result

###--------------------------------------------------
# Custom effect:
def CustomEffect( x,y,z,h ):
	global CustomFX
	try:
		a = eval( CustomFX[0].val )
		b = eval( CustomFX[1].val )
		result = eval( CustomFX[2].val )
		return result
	except:
		return 0.0

###--------------------------------------------------
## Effect Selector:

def Effects( (i,j),(x,y,z), h=0.0 ):
	global Effect_Type, Effect_Ctrl, iScale, Offset, Invert
	global NSize, Lx, Ly, Lz, Sx, Sy, Sz, marbleTwo, turbTwo, vlnoiTwo, Basis, musgrTwo

	x,y,z = Trans_Effect((x,y,z),( NSize[1].val, Sx[1].val, Sy[1].val, 0 ),( Lx[1].val, Ly[1].val, 0 )  )
	basis = Basis[1].val
	if basis == 9: basis = 14
	vbasis = vlnoiTwo[1].val
	if vbasis == 9: vbasis = 14
	if Effect_Ctrl[0].val == 1:
		try: effect  = Image_Func( x,y )
		except: effect =	0.0
	elif Effect_Ctrl[0].val == 2: effect = 0.5+0.5*turbulence(( x,y,z ),turbTwo[0].val, turbTwo[1].val, basis, turbTwo[2].val, turbTwo[3].val )
	elif Effect_Ctrl[0].val == 3: effect = 0.5+0.5*vlNoise(( x,y,z ),vlnoiTwo[0].val, vbasis, basis )
	elif Effect_Ctrl[0].val == 4: effect = 0.5*marbleNoise((x,y,z), marbleTwo[0].val, basis, marbleTwo[2].val, marbleTwo[3].val, marbleTwo[4].val, marbleTwo[5].val )
	elif Effect_Ctrl[0].val == 5:	effect = 0.5*multiFractal((   x,y,z ),musgrTwo[0].val, musgrTwo[1].val, musgrTwo[2].val, basis )
	elif Effect_Ctrl[0].val == 6:	effect = 0.5*ridgedMFractal(( x,y,z ),musgrTwo[0].val, musgrTwo[1].val, musgrTwo[2].val, musgrTwo[3].val, musgrTwo[4].val, basis )
	elif Effect_Ctrl[0].val == 7:	effect = 0.5*hybridMFractal(( x,y,z ),musgrTwo[0].val, musgrTwo[1].val, musgrTwo[2].val, musgrTwo[3].val, musgrTwo[4].val, basis )
	elif Effect_Ctrl[0].val == 8:	effect = 0.5*heteroTerrain((  x,y,z ),musgrTwo[0].val, musgrTwo[1].val, musgrTwo[2].val, musgrTwo[3].val, basis )*0.5
	elif Effect_Ctrl[0].val == 9:	effect = 0.5*fBm((            x,y,z ),musgrTwo[0].val, musgrTwo[1].val, musgrTwo[2].val, basis )+0.5
	elif Effect_Ctrl[0].val > 9 and Effect_Ctrl[0].val < 31:
		effect = Effect_Function((x,y), Effect_Ctrl[0].val-10,  Effect_Ctrl[3].val,  Effect_Ctrl[4].val,  Effect_Ctrl[5].val,  Effect_Ctrl[6].val, Effect_Ctrl[7].val )
	elif Effect_Ctrl[0].val == 31: effect = Effect_Ctrl[8].val * random()
	elif Effect_Ctrl[0].val == 32: effect = Effect_Ctrl[8].val
	elif Effect_Ctrl[0].val == 33: effect = CustomEffect( x,y,z, h )
	effect = HeightScale( effect, iScale[1].val , Offset[1].val, Invert[1].val )
	return  effect*2.0

###----------------------------------------------------------------------
# Noise:
##-----------------------------------

## voronoi_turbulence:
def voroTurbMode((x,y,z), voro, mode ):
	if mode == 0: # soft
		return voronoi(( x,y,z ),voro[0], voro[1] )[0][0]
	if mode == 1: # hard
		return ( abs( 0.5-voronoi(( x,y,z ),voro[0], voro[1] )[0][0] ) )+0.5
def voronoi_turbulence((x,y,z), voro, tur ):
	result = voroTurbMode((x,y,z), voro, tur[1] )
	depth  = tur[0]
	amp    = tur[2]
	freq   = tur[3]
	i=0
	for i in xrange( depth ):
		i+=1
		result += voroTurbMode( ( x*(freq*i), y*(freq*i), z ), voro, tur[1] )* ( amp*0.5/i )
	return (result*4.0-2.0)

## DistortedNoise / vlNoise_turbulence:
def vlnTurbMode((x,y,z), vlno, basis, mode ):
	if mode == 0: # soft
		return vlNoise(( x,y,z ),vlno[0], vlno[1], basis )
	if mode == 1: # hard
		return ( abs( -vlNoise(( x,y,z ),vlno[0], vlno[1], basis ) ) )
def vlNoise_turbulence((x,y,z), vlno, tur, basis ):
	result = vlnTurbMode((x,y,z), vlno, basis, tur[1] )
	depth  = tur[0]
	amp    = tur[2]
	freq   = tur[3]
	i=0
	for i in xrange( depth ):
		i+=1
		result += vlnTurbMode( ( x*(freq*i), y*(freq*i), z ), vlno, basis, tur[1] ) * ( amp*0.5/i )
	return result*2.0+0.5

## marbleNoise:
def marbleNoise( (x,y,z), depth, basis, turb, bias, sharpnes, rescale ):
	m = ( x * rescale + y * rescale + z ) * 5
	height = m + turb * turbulence( ( x ,y ,z ), depth, 0, basis, 0.5, 2.0 )
	height = Bias_Types[ bias ]( height )
	if bias != 4:
		height = Sharp_Types[ sharpnes ]( height )
	return height*2.0

## lava_multiFractal:
def lava_multiFractal( ( x,y,z ),Ha, La, Oc, distort, Basis ):
	m  = multiFractal( ( x,y,z ), Ha, La, Oc, Basis)
	d = m * distort
	m2 = 0.5 * multiFractal( ( x+d,y+d,d*0.5 ), Ha, La, Oc, Basis)
	return (m * m2)**0.5

## slopey_noise:
def slopey_noise((x,y,z), H, lacunarity, octaves, distort, basis ):
	x=x*2
	y=y*2
	turb = fBm((x,y,z), H, lacunarity, octaves, 2 ) * 0.5
	map = 0.5 + noise( ( x+turb, y+turb, z ), basis )
	result = map + turb * distort
	return result

## duo_multiFractal:
def double_multiFractal((x,y,z), H, lacunarity, octaves, offset, gain, basis ):
	n1 = multiFractal( (x*1.5+1,y*1.5+1,z), 1.0, 1.0, 1.0, basis[0] ) * offset
	n2 = multiFractal( (x-1,y-1,z), H, lacunarity, octaves, basis[1] ) * gain
	result = ( n1*n1 + n2*n2 )*0.5
	return result

## distorted_heteroTerrain:
def distorted_heteroTerrain((x,y,z), H, lacunarity, octaves, offset, distort, basis ):
	h1 = ( heteroTerrain((x,y,z), 1.0, 2.0, 1.0, 1.0, basis[0] ) * 0.5 )
	h2 = ( heteroTerrain(( x, y, h1*distort ), H, lacunarity, octaves, offset, basis[1] ) * 0.25 )
	result = ( h1*h1 + h2*h2 )
	return  result

## SlickRock:
def SlickRock((x,y,z), H, lacunarity, octaves, offset, gain, basis ):
	n = multiFractal( (x,y,z), 1.0, 2.0, 1.0, basis[0] )
	r = ridgedMFractal((x,y,n*0.5), H, lacunarity, octaves, offset, gain, basis[1] )*0.5
	return n+(n*r)

## terra_turbulence:
def terra_turbulence((x,y,z), depth, hard, basis, amp, freq ):
	t2 = turbulence( ( x, y, z ), depth,  hard , basis, amp, freq )
	return (t2*t2*t2)+0.5

## rocky_fBm:
def rocky_fBm((x,y,z), H, lacunarity, octaves, basis ):
	turb = fBm((x,y,z), H, lacunarity, octaves, 2 ) * 0.25
	coords = ( x+turb, y+turb, z )
	map = noise( coords, 7 )
	result = map + fBm( coords, H, lacunarity, octaves, basis ) + 1.0
	return result

## Shattered_hTerrain:
def Shattered_hTerrain((x,y,z), H, lacunarity, octaves, offset, distort, basis ):
	d = ( turbulence( ( x, y, z ), 6, 0, 0, 0.5, 2.0 ) * 0.5 + 0.5 )*distort*0.25
	t0 = ( turbulence( ( x+d, y+d, z ), 0, 0, 7, 0.5, 2.0 ) + 0.5 )
	t2 = ( heteroTerrain(( x*2, y*2, t0*0.5 ), H, lacunarity, octaves, offset, basis ) )
	return (( t0*t2 )+t2*0.5)*0.75

## vlhTerrain
def vlhTerrain((x,y,z), H, lacunarity, octaves, offset, basis, vlbasis, distort ):
	ht = heteroTerrain(( x, y, z ), H, lacunarity, octaves, offset, basis )*0.5
	vl = ht * vlNoise((x,y,z), distort, basis, vlbasis )*0.5+0.5
	return vl * ht

####---------------------------------------.
### StatsByAlt, double terrain  basis mode:
def TerrainBasisMode((x,y,z), basis, mode ):
	if mode == 0: # noise
		return noise((x,y,z),basis)
	if mode == 1: # noise ridged
		return ( 1.0-abs( noise((x,y,z),basis) ) )-0.5
	if mode == 2: # vlNoise
		return vlNoise((x,y,z), 1.0, 0, basis )
	else:         # vlNoise ridged
		return ( 1.0-abs( vlNoise((x,y,z), 1.0, 0, basis ) ) )-0.5

#### StatsByAlt terrain:
def StatsByAltTerrain((x,y,z), exp, lacu, octs, offset, amp, basis, mode ):
	result = 0.5 * (offset + TerrainBasisMode((x,y,z), basis, mode ) )
	octs = int( octs )
	i = 0
	for i in xrange( 1, octs ):
		i += 1
		result += result * amp * 0.5 * (offset + TerrainBasisMode((x,y,z), basis, mode ) )
		x *= lacu
		y *= lacu
		amp /= ( exp * 0.5 ) * i		
	return result

##### double terrain:
def doubleTerrain((x,y,z), exp, lacu, octs, offset, threshold, basis, mode ):
	result = amp = freq = 1.0
	#octs = int( octs )
	offset*=0.5
	i = 1
	signal = result = 0.5 * (offset + TerrainBasisMode((x,y,z), basis, mode ) )
	for i in xrange( 1, octs ):
		i += 1
		x = x * lacu
		y = y * lacu
		freq *= lacu
		amp = pow( freq, -exp )
		amp *= i
		weight = signal / threshold
		if weight > 1.0: weight = 1.0
		if weight < 0.0: weigth = 0.0
		signal = weight * 0.5 * ( offset + TerrainBasisMode((x,y,z), basis, mode ) )
		result += amp * signal
	return result * 2.0

##------------------------------------------------------------
# Noise Functions:
def Noise_Function(x,y,z):
	global Basis, NType, musgr, vlnoi, voron, turbOne, marbleOne, tBasismod
	global vlBasis, Distort, VFunc, VExp, VDep
	global iScale, Offset, Invert, NSize, Lx, Ly, Sx, Sy

	x,y,z = Trans((x,y,z),( NSize[0].val, Sx[0].val, Sy[0].val, 0 ),( Lx[0].val, Ly[0].val, 0 )  )
	basis = Basis[0].val
	if basis == 9: basis = 14
	vbasis = vlnoi[1].val
	if vbasis == 9: vbasis = 14
	if   NType.val == 0:	z = multiFractal((   x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, basis )
	elif NType.val == 1:	z = ridgedMFractal(( x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[4].val, basis )
	elif NType.val == 2:	z = hybridMFractal(( x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[4].val, basis )
	elif NType.val == 3:	z = heteroTerrain((  x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, basis )*0.5
	elif NType.val == 4:	z = fBm((            x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, basis )+0.5
	elif NType.val == 5:	z = turbulence((     x,y,z ),turbOne[0].val, turbOne[1].val, basis, turbOne[2].val, turbOne[3].val )+0.5
	elif NType.val == 6:	z = voronoi_turbulence((x,y,z),(voron[0].val,voron[1].val),(turbOne[0].val,turbOne[1].val,turbOne[2].val,turbOne[3].val) )*0.5+0.5
	elif NType.val == 7:	z = vlNoise_turbulence((x,y,z),(vlnoi[0].val,vbasis), (turbOne[0].val,turbOne[1].val,turbOne[2].val,turbOne[3].val), basis )*0.5+0.5
	elif NType.val == 8:	z = noise((          x,y,z ),basis )+0.5
	elif NType.val == 9:	z = cellNoise((      x,y,z ))+0.5
	elif NType.val == 10: z = marbleNoise((         x,y,z), marbleOne[0].val, basis, marbleOne[2].val, marbleOne[3].val, marbleOne[4].val, marbleOne[5].val )
	elif NType.val == 11: z = lava_multiFractal((  x,y,z ), musgr[0].val, musgr[1].val, musgr[2].val, vlnoi[0].val, basis )
	elif NType.val == 12: z = slopey_noise((         x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, vlnoi[0].val, basis )+0.5
	elif NType.val == 13: z = double_multiFractal(( x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[4].val, (vbasis,basis) )
	elif NType.val == 14: z = distorted_heteroTerrain((x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, vlnoi[0].val, (vbasis,basis) )
	elif NType.val == 15: z = SlickRock((           x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[4].val, (vbasis,basis)  )
	elif NType.val == 16: z = terra_turbulence((  x,y,z), turbOne[0].val, turbOne[1].val, basis, turbOne[2].val, turbOne[3].val )
	elif NType.val == 17: z = rocky_fBm((           x,y,z ),musgr[0].val, musgr[1].val, musgr[2].val, basis )
	elif NType.val == 18: z = StatsByAltTerrain(   (x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[4].val*0.5, basis, tBasismod.val )
	elif NType.val == 19: z = doubleTerrain(       (x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, musgr[5].val, basis, tBasismod.val )
	elif NType.val == 20: z = Shattered_hTerrain((x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, vlnoi[0].val, basis )
	elif NType.val == 21: z = vlhTerrain((x,y,z), musgr[0].val, musgr[1].val, musgr[2].val, musgr[3].val, basis, vbasis, vlnoi[0].val )
	else:	z = 0.0
	return HeightScale( z, iScale[0].val , Offset[0].val, Invert[0].val )

###----------------------------------------------------------------------
##-----------------------------------
# Filter functions:

##-----------------------------------
# Filter: Clamp height
def Clamp_Max( height, max ):
	if ( height > max ): height = max
	return height
def Clamp_Min( height, min ):
	if ( height < min ): height = min
	return height

##-----------------------------------
# Filters: terrace / posterise / peaked / bias:
def Def_Filter((x,y,z), input, numb, type ):
	if   type == 0:
		s = ( sin( input*numb*phi ) * ( 0.1/numb*phi ) )
		return ( input * (1.0-0.5) + s*0.5 ) * 2.0
	elif type == 1:
		s = -abs( sin( input*(numb*0.5)*phi ) * ( 0.1/(numb*0.5)*phi ) )
		return ( input * (1.0-0.5) + s*0.5 ) * 2.0
	elif type == 2:
		s = abs( sin( input*(numb*0.5)*phi ) * ( 0.1/(numb*0.5)*phi ) )
		return ( input * (1.0-0.5) + s*0.5 ) * 2.0
	elif type == 3:
		numb = numb*0.5
		s = ( int( input*numb ) * 1.0/numb )
		return ( input * (1.0-0.5) + s*0.5 ) * 2.0
	elif type == 4:
		numb = numb*0.5
		s = ( int( input*numb ) * 1.0/numb )
		return ( s ) * 2.0
	elif type == 5:
		s = ( sin( input*(2*numb)*phi ) * ( 0.1/(2*numb)*phi ) )
		l = ( input * (1.0-0.5) + s*0.5 ) * 2.0
		p = ( ( l*numb*0.25 ) * ( l*numb*0.25 ) )**2
		return ( l * (1.0-0.5) + p*0.5 ) * 2.0
	elif type == 6:
		return ( input*numb*0.25 )**4
	elif type == 7:
		return 2.0-exp( 1.0-(input*numb/3.0) )
	elif type == 8:
		return sin_bias( input*numb )*2.0
	elif type == 9:
		return cos_bias( input*numb )*2.0
	elif type == 10:
		return tri_bias( input*numb )*2.0
	elif type == 11:
		return saw_bias( input*numb )*2.0
	elif type == 12:
		return Clamp_Max( input, numb )
	else:
		return input

##-----------------------------------
# Filter: Edge falloff
def EdgeFalloff( (x,y,z), height, type ):
	global Falloff, iScale, Offset

	x = x / Falloff[1].val
	y = y / Falloff[2].val

	if Falloff[3].val != 0:
		sealevel = (Min.val-Offset[2].val)*2.0/iScale[2].val
	else:
		sealevel = 0.0

	falltypes = ( 0, sqrt(x*x+y*y), sqrt((x*x)**2+(y*y)**2), sqrt((x*x)**10+(y*y)**10), sqrt(y*y), sqrt(x*x), abs(x-y), abs(x+y), ((x*x)**10+(y*y)**10)**0.1, ((x*x)+(y*y)) )

	dist = falltypes[ type ]
	if Falloff[4].val != 0:
		dist = 1.0 - dist
	radius = 1.0
	height = height - sealevel
	if( dist < radius ):
		dist = dist / radius
		dist = ( (dist) * (dist) * ( 3-2*(dist) ) )
		height = ( height - height * dist ) + sealevel
	else:
		height = sealevel

	if Falloff[3].val != 0:
		height = Clamp_Min( height, sealevel )
	else:
		height = Clamp_Min( height, Min.val )

	return height

##-----------------------------------
# Filter: Custom height filter:
def CustomFilter( x,y,z, h ):
	global CustomFilt
	try:
		a = eval(CustomFilt[0].val)
		b = eval(CustomFilt[1].val)
		result = eval(CustomFilt[2].val)
		return result
	except:
		return 0.0

#####-------------------------------------------------------------------------------------#####
####-------------------------------------------------------------------------------------####
### Combine Functions: (get noise, Add effect, filter height and return result)         ###
##-------------------------------------------------------------------------------------##

def Combine_Functions( (i,j),(x,y,z) ):
	global Effect_Ctrl, Blend_Effect, Filter_Mode, Def_Filter_Ctrl, Ipo_Filter_Ctrl, Filter_Order
	global iScale, Offset, Invert, Min, Max, Falloff

	# get noise height:
	height = Noise_Function(x,y,0.0)

	### Filter On
	if Filter_Mode.val !=0:
		### 0= Default Filter Order: Noise>Effect>Filter ---------------------
		if Filter_Order.val ==0:
			# mix noise with effect:
			if Effect_Ctrl[0].val !=0:
				height = Mix_Modes( (i,j),(x,y,z), height , Effects( (i,j),(x,y,z),height ), Effect_Ctrl[2].val, Effect_Ctrl[1].val )
			# edge fallof:
			if Falloff[0].val !=0:
				height = EdgeFalloff( (x,y,z), height, Falloff[0].val )
		#else: pass

		if Filter_Mode.val !=0:
			# height Def_Filter (Terrace/peaked/bias):
			if Filter_Mode.val ==1:
				height = Def_Filter((x,y,z), height, Def_Filter_Ctrl[ 1 ].val, Def_Filter_Ctrl[ 0 ].val )

			## 'IPOCurve' height filter:
			elif Filter_Mode.val ==2:
				try:
					height = selectedcurve.evaluate( 1 + ( height*Ipo_Filter_Ctrl[2].val/2 ) )*2.0/Ipo_Filter_Ctrl[3].val
				except:
					height = height

			## Custom filter:
			elif Filter_Mode.val ==3:
					height = CustomFilter( x,y,z, height )

		### 1= Changed Filter Order: Noise>Filter>Effect ---------------------
		if Filter_Order.val !=0:
			# mix noise with effect:
			if Effect_Ctrl[0].val !=0:
				height = Mix_Modes( (i,j),(x,y,z), height , Effects( (i,j),(x,y,z),height ), Effect_Ctrl[2].val, Effect_Ctrl[1].val )
			# edge fallof:
			if Falloff[0].val !=0:
				height = EdgeFalloff( (x,y,z), height, Falloff[0].val )
		#else: pass

	### Filter Off ---------------------
	else:
		# mix noise with effect:
		if Effect_Ctrl[0].val !=0:
			height = Mix_Modes( (i,j),(x,y,z), height , Effects( (i,j),(x,y,z),height ), Effect_Ctrl[2].val, Effect_Ctrl[1].val )
		# edge fallof:
		if Falloff[0].val !=0:
			height = EdgeFalloff( (x,y,z), height, Falloff[0].val )

	# height scale:	
	height = HeightScale( height, 0.5*iScale[2].val , Offset[2].val, Invert[2].val )

	# clamp height min. max.:
	if Falloff[0].val !=1:
		height = Clamp_Min( height, Min.val )
	height = Clamp_Max( height, Max.val )

	# return height:
	return height


#------------------------------------------------------------
##------------------------------------------------------------
###  Render Noise to a Image('NAME') (you must create one first)
##------------------------------------------------------------
#------------------------------------------------------------

def HeightFieldImage():
	global PreView, previewname

	iname = previewname.val
	try: 
		pic = Image.Get( iname )
	except:
		#print iname, ' No Image with this name'
		PupMenu( 'No Image with this name' )
		return
	res = pic.getMaxXY()
	for i in xrange( res[0] ):
		x = i - (res[0]) / 2.0
		x = (x*2.0) / (res[0])
		for j in xrange( res[1] ):
			y = j - (res[1]) / 2.0
			y = (y*2.0) / (res[1])
			height = PreView[2].val + PreView[1].val * Combine_Functions( (i,j),(x,y,0) )
			if height > 1.0: height = 1.0
			if height < 0.0: height = 0.0
			pic.setPixelF( i, j, ( height,height,height, 1.0 ) )


#------------------------------------------------------------
##------------------------------------------------------------
###  Mesh
##------------------------------------------------------------
#------------------------------------------------------------

#------------------------------------------------------------
## Mesh: make new grid
###------------------------------------------------------------

def MakeGridMesh( RESOL=32, NAME='GridMesh', CURSORPOS=0, SCENE=None ):
	# scene, object, mesh ---------------------------------------
	if not SCENE:
		SCENE = Blender.Scene.GetCurrent()
	SCENE.objects.selected=[]
	newme = Blender.Mesh.New( NAME )
	newob = SCENE.objects.new( newme, NAME )
	n = RESOL
	# verts ---------------------------------------
	v=[]
	for i in xrange( n ):
		x = i-(n-1)/2.0
		x = x*2.0/(n-1)
		for j in xrange( n ):
			y = j-(n-1)/2.0
			y = y*2.0/(n-1)
			v.append( [ x, y, 0 ] )
	newme.verts.extend(v)
	# faces ---------------------------------------
	f=[]
	for i in xrange( n-1 ):
		for j in xrange( n-1 ):
			f.append( [	i*n+j,\
						(i+1)*n+j,\
						(i+1)*n+j+1,\
						i*n+j+1 ] )
	
	newme.faces.extend(f, smooth=True)
	#---------------------------------------
	newme.calcNormals()
	#---------------------------------------
	if CURSORPOS !=0:
		newob.loc = Window.GetCursorPos()
	newob.select(1)

#------------------------------------------------------------
## Mesh: Grid vert displace / update terrain
###------------------------------------------------------------

def displace( OB, ME, WORLD=0  ):
	if WORLD == 1:
		wx,wy,wz = OB.getLocation( 'worldspace' )
	elif WORLD ==2:
		l = OB.getLocation( 'worldspace' )
		w = Window.GetCursorPos()
		wx,wy,wz = l[0]-w[0], l[1]-w[1], l[2]-w[2]
	else:
		wx,wy,wz = 0,0,0

	for v in ME.verts:
		co = v.co
		co[2] = Combine_Functions( (co[0]+wx,co[1]+wy),(co[0]+wx, co[1]+wy, 0.0+wz) )
	ME.update()
	ME.calcNormals()	
	#OB.makeDisplayList()


#----------------------------------------------------------------------------------------------------
##----------------------------------------------------------------------------------------------------
###----------------------------------------------------------------------------------------------------
####----------------------------------------------------------------------------------------------------
###----------------------------------------------------------------------------------------------------
## Do_it:
#--------------------------------------

#--------------------------------------
def do_it():
	global PreView, actme, actob, WorldSpaceCo
	if actme !=[]:	
		if print_time !=0:
			t= sys.time()
		Window.WaitCursor(1)
		in_editmode = Window.EditMode()
		if in_editmode: Window.EditMode(0)
		if PreView[0].val != 0:
			do_it_preview()
			displace( actob[0], actme[0], WorldSpaceCo.val  )
			Window.RedrawAll()
		else:
			displace( actob[0], actme[0], WorldSpaceCo.val  )
			Window.RedrawAll()
		if in_editmode: Window.EditMode(1)
		Window.WaitCursor(0)			
		if print_time !=0:
			print 'Generate Mesh: done in %.6f' % (sys.time()-t)

#--------------------------------------
def do_it_random():
	global PreView, actme, actob
	if actme !=[]:	
		if print_time !=0:
			t= sys.time()
		Window.WaitCursor(1)
		in_editmode = Window.EditMode()
		if in_editmode: Window.EditMode(0)
		randomiseNoise()
		if PreView[0].val != 0:
			do_it_preview()
			displace( actob[0], actme[0], WorldSpaceCo.val  )
			Window.RedrawAll()
		else:
			displace( actob[0], actme[0], WorldSpaceCo.val  )
			Window.RedrawAll()
		if in_editmode: Window.EditMode(1)
		Window.WaitCursor(0)
		if print_time !=0:
			print 'Generate Mesh: done in %.6f' % (sys.time()-t)

#--------------------------------------
def do_it_preview():
	if print_time !=0:
		t= sys.time()
	HeightFieldImage()
	Window.RedrawAll()
	if print_time !=0:
		print 'Generate Image: done in %.6f' % (sys.time()-t)

###---------------------------------------------------------
###---------------------------------------------------------
## load and save:
#-------------------------

def callback( filename ):
	txtFile.val = filename
	Register(drawgui, events, bevents)
def writeln(f,x):
  f.write(str(x))
  f.write("\n")
def readint(f):
  return int(f.readline())
def readfloat(f):
  return float(f.readline())
def readstr(f):
  s = (f.readline())
  return strip(s)

#--------------------------------------------------
# Save settings to .ant file
def SavePreset(FName):
	global iScale, Offset, Invert, NSize, Sx, Sy, Lx, Ly
	global NType, Basis, musgr, tBasismod, vlnoi, vlnoiTwo, voron, turbOne, turbTwo, marbleOne, marbleTwo, musgrTwo
	global CustomFX, effect_image, Effect_Ctrl, Min, Max, Falloff, CustomFilt, Filter_Mode, Def_Filter_Ctrl, Ipo_Filter_Ctrl, Filter_Order
	global RandMod, RSeed, rand_H, rand_S, rand_L, rand_I, filemessage, fileinfo

	try:
		f = open(FName,'w')
		writeln(f,CurVersion)
	except:
		filemessage = "Unable to save file."
		return

	writeln(f,fileinfo)
	writeln(f,iScale[0].val)
	writeln(f,iScale[1].val)
	writeln(f,iScale[2].val)
	writeln(f,Offset[0].val)
	writeln(f,Offset[1].val)
	writeln(f,Offset[2].val)
	writeln(f,Invert[0].val)
	writeln(f,Invert[1].val)
	writeln(f,Invert[2].val)
	writeln(f,NSize[0].val)
	writeln(f,NSize[1].val)
	writeln(f,Sx[0].val)
	writeln(f,Sx[1].val)
	writeln(f,Sy[0].val)
	writeln(f,Sy[1].val)
	writeln(f,Lx[0].val)
	writeln(f,Lx[1].val)
	writeln(f,Ly[0].val)
	writeln(f,Ly[1].val)
	writeln(f,NType.val)
	writeln(f,Basis[0].val)
	writeln(f,Basis[1].val)
	writeln(f,musgr[0].val)
	writeln(f,musgr[1].val)
	writeln(f,musgr[2].val)
	writeln(f,musgr[3].val)
	writeln(f,musgr[4].val)
	writeln(f,musgr[5].val)
	writeln(f,tBasismod.val)
	writeln(f,vlnoi[0].val)
	writeln(f,vlnoi[1].val)
	writeln(f,vlnoiTwo[0].val)
	writeln(f,vlnoiTwo[1].val)
	writeln(f,voron[0].val)
	writeln(f,voron[1].val)
	writeln(f,turbOne[0].val)
	writeln(f,turbOne[1].val)
	writeln(f,turbOne[2].val)	
	writeln(f,turbOne[3].val)
	writeln(f,turbTwo[0].val)
	writeln(f,turbTwo[1].val)	
	writeln(f,turbTwo[2].val)	
	writeln(f,turbTwo[3].val)	
	writeln(f,marbleOne[0].val)
	writeln(f,marbleOne[1].val)
	writeln(f,marbleOne[2].val)
	writeln(f,marbleOne[3].val)
	writeln(f,marbleOne[4].val)
	writeln(f,marbleOne[5].val)
	writeln(f,marbleTwo[0].val)
	writeln(f,marbleTwo[1].val)		
	writeln(f,marbleTwo[2].val)
	writeln(f,marbleTwo[3].val)
	writeln(f,marbleTwo[4].val)
	writeln(f,marbleTwo[5].val)
	writeln(f,musgrTwo[0].val)
	writeln(f,musgrTwo[1].val)
	writeln(f,musgrTwo[2].val)
	writeln(f,musgrTwo[3].val)
	writeln(f,musgrTwo[4].val)
	writeln(f,effect_image)
	writeln(f,Effect_Ctrl[0].val)
	writeln(f,Effect_Ctrl[1].val)
	writeln(f,Effect_Ctrl[2].val)
	writeln(f,Effect_Ctrl[3].val)
	writeln(f,Effect_Ctrl[4].val)
	writeln(f,Effect_Ctrl[5].val)
	writeln(f,Effect_Ctrl[6].val)
	writeln(f,Effect_Ctrl[7].val)
	writeln(f,Effect_Ctrl[8].val)
	writeln(f,CustomFX[0].val)
	writeln(f,CustomFX[1].val)
	writeln(f,CustomFX[2].val)
	writeln(f,Min.val)
	writeln(f,Max.val)
	writeln(f,Falloff[0].val)
	writeln(f,Falloff[1].val)
	writeln(f,Falloff[2].val)
	writeln(f,Falloff[3].val)
	writeln(f,Falloff[4].val)
	writeln(f,Filter_Mode.val)
	writeln(f,Filter_Order.val)
	writeln(f,CustomFilt[0].val)
	writeln(f,CustomFilt[1].val)
	writeln(f,CustomFilt[2].val)
	writeln(f,Def_Filter_Ctrl[0].val)
	writeln(f,Def_Filter_Ctrl[1].val)
	writeln(f,Ipo_Filter_Ctrl[0].val)
	writeln(f,Ipo_Filter_Ctrl[1].val)
	writeln(f,Ipo_Filter_Ctrl[2].val)
	writeln(f,Ipo_Filter_Ctrl[3].val)
	writeln(f,RandMod.val)
	writeln(f,RSeed.val)
	writeln(f,rand_H.val)
	writeln(f,rand_I.val)
	writeln(f,rand_S.val)
	writeln(f,rand_L.val)
	filemessage = 'Settings saved to file.'
	f.close()

#--------------------------------------------------
# load settings from .ant file
def LoadPreset(FName):
	global iScale, Offset, Invert, NSize, Sx, Sy, Lx, Ly
	global NType, Basis, musgr, tBasismod, vlnoi, vlnoiTwo, voron, turbOne, turbTwo, marbleOne, marbleTwo, musgrTwo
	global CustomFX, effect_image, Effect_Ctrl, Min, Max, Falloff, CustomFilt, Filter_Mode, Def_Filter_Ctrl, Ipo_Filter_Ctrl, Filter_Order
	global RandMod, RSeed, rand_H, rand_S, rand_L, rand_I, filemessage, fileinfo

	try:
		f = open(FName,'r')
		FVersion = readstr(f)
	except:
		filemessage = "Unable to open file."
		return

	fileinfo = readstr(f)
	iScale[0].val = readfloat(f)
	iScale[1].val = readfloat(f)
	iScale[2].val = readfloat(f)
	Offset[0].val = readfloat(f)
	Offset[1].val = readfloat(f)
	Offset[2].val = readfloat(f)
	Invert[0].val = readint(f)
	Invert[1].val = readint(f)
	Invert[2].val = readint(f)
	NSize[0].val = readfloat(f)
	NSize[1].val = readfloat(f)
	Sx[0].val = readfloat(f)
	Sx[1].val = readfloat(f)
	Sy[0].val = readfloat(f)
	Sy[1].val = readfloat(f)
	Lx[0].val = readfloat(f)
	Lx[1].val = readfloat(f)
	Ly[0].val = readfloat(f)
	Ly[1].val = readfloat(f)
	NType.val = readint(f)
	Basis[0].val = readint(f)
	Basis[1].val = readint(f)
	musgr[0].val = readfloat(f)
	musgr[1].val = readfloat(f)
	musgr[2].val = readint(f)
	musgr[3].val = readfloat(f)
	musgr[4].val = readfloat(f)
	musgr[5].val = readfloat(f)
	tBasismod.val  = readint(f)
	vlnoi[0].val = readfloat(f)
	vlnoi[1].val = readint(f)
	vlnoiTwo[0].val = readfloat(f)
	vlnoiTwo[1].val = readint(f)
	voron[0].val = readint(f)
	voron[1].val = readfloat(f)
	turbOne[0].val = readint(f)
	turbOne[1].val = readint(f)
	turbOne[2].val = readfloat(f)
	turbOne[3].val = readfloat(f)
	turbTwo[0].val = readint(f)
	turbTwo[1].val = readint(f)
	turbTwo[2].val = readfloat(f)
	turbTwo[3].val = readfloat(f)
	marbleOne[0].val = readint(f)
	marbleOne[1].val = readint(f)
	marbleOne[2].val = readfloat(f)
	marbleOne[3].val = readint(f)
	marbleOne[4].val = readint(f)
	marbleOne[5].val = readfloat(f)
	marbleTwo[0].val = readint(f)
	marbleTwo[1].val = readint(f)
	marbleTwo[2].val = readfloat(f)
	marbleTwo[3].val = readint(f)
	marbleTwo[4].val = readint(f)
	marbleTwo[5].val = readfloat(f)
	musgrTwo[0].val = readfloat(f)
	musgrTwo[1].val = readfloat(f)
	musgrTwo[2].val = readint(f)
	musgrTwo[3].val = readfloat(f)
	musgrTwo[4].val = readfloat(f)
	effect_image = readstr(f)
	Effect_Ctrl[0].val = readint(f)
	Effect_Ctrl[1].val = readint(f)
	Effect_Ctrl[2].val = readfloat(f)
	Effect_Ctrl[3].val = readint(f)
	Effect_Ctrl[4].val = readfloat(f)
	Effect_Ctrl[5].val = readint(f)
	Effect_Ctrl[6].val = readfloat(f)
	Effect_Ctrl[7].val = readfloat(f)
	Effect_Ctrl[8].val = readfloat(f)
	CustomFX[0].val = readstr(f)
	CustomFX[1].val = readstr(f)
	CustomFX[2].val = readstr(f)
	Min.val = readfloat(f)
	Max.val = readfloat(f)
	Falloff[0].val = readint(f)
	Falloff[1].val = readfloat(f)
	Falloff[2].val = readfloat(f)
	Falloff[3].val = readint(f)
	Falloff[4].val = readint(f)
	Filter_Mode.val = readint(f)
	Filter_Order.val = readint(f)
	CustomFilt[0].val = readstr(f)
	CustomFilt[1].val = readstr(f)
	CustomFilt[2].val = readstr(f)
	Def_Filter_Ctrl[0].val = readint(f)
	Def_Filter_Ctrl[1].val = readfloat(f)
	Ipo_Filter_Ctrl[0].val = readstr(f)
	Ipo_Filter_Ctrl[1].val = readint(f)
	Ipo_Filter_Ctrl[2].val = readfloat(f)
	Ipo_Filter_Ctrl[3].val = readfloat(f)
	RandMod.val = readint(f)
	RSeed.val = readint(f)
	rand_H.val = readint(f)
	rand_I.val = readint(f)	
	rand_S.val = readint(f)
	rand_L.val = readint(f)
	filemessage = 'Settings loaded from file.'
	f.close()

##---------------------------------------------------------------------------
# Register:

Register( drawgui, events, bevents )
###--------------------------------------------------------------------------
		