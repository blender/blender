#!BPY

""" Registration info for Blender menus:
Name: 'HotKey and MouseAction Reference'
Blender: 232
Group: 'Help'
Tip: 'All the hotkeys/short keys'
""" 

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_hotkeyscript.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "10/2004"

__bpydoc__ = """\
This script is a reference about all hotkeys and mouse actions in Blender.

Usage:

Open the script from the Help menu and select group of keys to browse.

Notes:<br>
    Additional entries in the database (c) 2004 by Bart.
"""

# $Id$
#------------------------
#  Hotkeys script
#         jm soler (2003-->10/2004)
# -----------------------
# Page officielle :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_hotkeyscript.htm
# Communiquer les problemes et les erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#--------------------------------------------- 
# ce script est proposé sous licence GPL pour etre associe
# a la distribution de Blender 2.33 et suivant
# --------------------------------------------------------------------------
# this script is released under GPL licence
# for the Blender 2.33 scripts package
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) 2003, 2004: Jean-Michel Soler 
# Additionnal entries in the original data base (c) 2004 by Bart (bart@neeneenee.de)
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

import Blender
from Blender.Draw import *
from Blender.BGL import *

hotkeys={
'Specials 1 ':[
[',', 'Set Bounding Box rotation scaling pivot'],
['Ctrl-,', 'Set Median Point rotation scaling pivot'],
['.', 'Set 3D cursor as rotation scaling pivot'],
['Ctrl-.', 'Set Individual Object Centers as rotation scaling pivot'] ,
['~', 'Display all layers (German keys: ö)'],
['Shift-~', 'Display all/previous layers (German keys: Shift-ö)'],
['Space', 'Popup menu'],
['Space', '3D View: camera selected + fly mode, accept'],
['TAB', 'Enter/exit Edit Mode'],
['TAB', 'Edit Mode and Numerical Edit (see N key) : move to next input value'],
['TAB', 'Sequencer: Edit meta strip'],
['TAB', 'IPO: Edit selected'],
['Ctrl-TAB', 'Enter/exit Pose Mode'],
['Shift-TAB', 'Enter Object Mode'],
['Ctrl-Open menu/', ''],
['Ctrl-Load Image', 'Opens a thumbnail browser instead of file browser for images']
],
'Mouse ':[
['Actions:', ''],
['LMB', '3D View: Set 3D Cursor'],
['LMB', '3D View: camera selected + fly mode, move forward'],
['LMB drag', 'Border select circle: add to selection'],
['LMB hold down', 'Popup menu'],
['LMB hold down drag', 'Gesture'],
['Ctrl-LMB', 'IPO: Add key'],
['MMB', 'Rotate'],
['Ctrl-MMB', 'Zoom view'],
['Shift-MMB', 'Move view'],
['RMB', 'Select'],
['RMB drag', 'Border select circle: subtract from selection'],
['RMB hold down', 'Popup menu'],
['Alt+Ctrl-RMB', 'Edit Mode: Select edge'],
['Alt+Ctrl-RMB', 'UV Image Editor: Select face'],
['Shift-RMB', 'Add/subtract to/from selection'],
['Wheel', 'Zoom view'],
['Transformations:', ''],
['Drag+Ctrl', 'Step adjustment'],
['Drag+Ctrl+Shift', 'Small step adjustment'],
['Drag+Shift', 'Fine adjustment'],
['LMB', 'Confirm transformation'],
['MMB', 'Toggle optional transform feature'],
['RMB', 'Abort transformation']
],
'F-Keys ':[
['F1', 'Open File'],
['F2', 'Save File'],
['F3', 'Save image'],
['F4', 'Logic Window (may change)'],
['F5', 'Material Window'],
['F6', 'Texture Window'],
['F7', 'Object Window'],
['F8', 'World Window'],
['F9', 'Edit Mode Window'],
['F10', 'Render Window'],
['F11', 'Recall the last rendered image'],
['F12', 'Render current Scene'],
['Ctrl-Shift-F12', 'NLA Editor'],
['Shift-F1', 'Library Data Select'],
['Shift-F2', 'Export DXF'],
['Shift-F4', 'Object manager Data Select '],
['Shift-F5', '3D Window'],
['Shift-F6', 'IPO Window'],
['Shift-F7', 'Buttons Window'],
['Shift-F8', 'Video Sequencer Window'],
['Shift-F9', 'OOP Window'],
['Shift-F10', 'UV Image Editor'],
['Shift-F11', 'Text Editor'],
['Shift-F12', 'Action Editor']
],

'Numbers ':[
['1..2..0-=', 'Show layer 1..2..12'],
['Alt-1..2..0', 'Show layer 11..12..20'],
['Shift-1..2..0-=', 'Toggle layer 1..2..12'],
['Shift-ALT-...', 'Toggle layer 11..12..20']
],

'Numpad ':[
['Numpad DEL', 'Zoom on object'],
['Numpad /', 'Local view on object (hide others)'],
['Numpad *', 'Rotate view to objects local axes'],
['Numpad +', 'Zoom in (works everywhere)'],
['Numpad +', 'Proportional vertex Edit Mode: Increase range of influence'],
['Ctrl-Numpad +', 'Edit Mode: Select More vertices'],
['Numpad -', 'Zoom out (works everywhere)'],
['Numpad -', 'Proportional vertex Edit Mode: Decrease range of influence'],
['Ctrl-Numpad +', 'Edit Mode: Select Less vertices'],
['Numpad INS', 'Set Camera view'],
['Ctrl-Numpad INS', 'Set active object as camera'],
['Alt-Numbad INS', 'Restore old camera'],
['Numpad 1', 'Front view'],
['Ctrl-Numpad 1', 'Back view'],
['Numpad 3', 'Right view'],
['Ctrl-Numpad 3', 'Left view'],
['Numpad 7', 'Top view'],
['Ctrl-Numpad 7', 'Bottom view '],
['Numpad 5', 'Toggle orthogonal/perspective view'],
['Numpad 9', 'Redraw view'],
['Numpad 4', 'Rotate view left'],
['Numpad 6', 'Rotate view right'],
['Numpad 8', 'Rotate view up'],
['Numpad 2', 'Rotate view down']
],

'Arrows ':[
['Home/Pos1', 'View all'],
['PgUp', 'IPO: Select next keyframe'],
['Ctrl-PgUp', 'IPO: Select and jump to next keyframe'],
['PgDn', 'IPO: Select previous keyframe'],
['Ctrl-PgDn', 'IPO: Select and jump to previous keyframe'],
['Left', 'One frame backwards'],
['Right', 'One frame forwards'],
['Down', '10 frames backwards'],
['Up', '10 frames forwards'],
['Alt-Down', 'Blender in Window mode'],
['Alt-Up', 'Blender in Fullscreen mode'],
['Ctrl-Left', 'Previous screen'],
['Ctrl-Right', 'Next screen'],
['Ctrl-Down', 'Maximize window toggle'],
['Ctrl-Up', 'Maximize window toggle'],
['Shift-Arrow', 'Toggle first frame/ last frame']
],

'Letters ':[ {"A":[ 
['A', 'Select all/Deselect all'],
['Alt-A', 'Play animation in current window'],
['Ctrl-A', 'Apply objects size/rotation to object data'],
['Ctrl-A', 'Text Editor: Select all'],
['Shift-A', 'Sequencer: Add menu'],
['Shift-A', '3D-View: Add menu'],
['Shift-ALT-A', 'Play animation in all windows'],
['Shift-CTRL-A', 'Apply lattice / Make dupliverts real']
],

"B":[ 
['B', 'Border select'],
['BB', 'Circle select'],
['Alt+B', 'Edit Mode: Select Vertex Loop'],
['Shift-B', 'Set render border (in active camera view)']
],

"C":[ 
['C', 'Center view on cursor'],
['C', 'UV Image Editor: Active Face Select toggle'],
['C', 'Sequencer: Change images'],
['C', 'IPO: Snap current frame to selected key'],
['Alt-C', 'Object Mode: Convert menu'],
['Alt-C', 'Text Editor: Copy selection to clipboard'],
['Ctrl-C', 'Copy menu (Copy properties of active to selected objects)'],
['Ctrl-C', 'UV Image Editor: Stick UVs to mesh vertex'],
['Shift-C', 'Center and zoom view on selected objects'],
['Shift-C', 'UV Image Editor: Stick local UVs to mesh vertex']
],

"D":[  
['D', 'Set 3d draw mode'],
['Alt-D', 'Object Mode: Create new instance of object'],
['Ctrl-D', 'Display alpha of image texture as wire'],
['Shift-D', 'Create full copy of object']
],

"E":[ 
['E', 'Edit Mode: Extrude'],
['E', 'UV Image Editor: LSCM Unwrap'],
['ER', 'Edit Mode: Extrude Rotate'],
['ES', 'Edit Mode: Extrude Scale'],
['ESX', 'Edit Mode: Extrude Scale X axis'],
['ESY', 'Edit Mode: Extrude Scale Y axis'],
['ESZ', 'Edit Mode: Extrude Scale Z axis'],
['EX', 'Edit Mode: Extrude along X axis'],
['EY', 'Edit Mode: Extrude along Y axis'],
['EZ', 'Edit Mode: Extrude along Z axis'],
['Alt-E', 'Edit Mode: exit Edit Mode'],
['Ctrl-E', 'Edit Mode: Edge Specials menu'],
['Shift-E', 'Edit Mode: SubSurf Edge Sharpness']
],

"F":[ 
['F', 'Edit mode: Make edge/face'],
['F', 'Sequencer: Set Filter Y'],
['F', 'Object Mode: UV/Face Select mode'],
['Alt-F', 'Edit Mode: Beautify fill'],
['Ctrl-F', 'Object Mode: Sort faces in Z direction'],
['Ctrl-F', 'Edit Mode: Flip triangle edges'],
['Shift-F', 'Edit Mode: Fill with triangles'],
['Shift-F', 'Object Mode: active camera in fly mode (use LMB, RMB, Alt, Ctrl and Space too)']
],

"G":[ 
['G', 'Grab (move)'],
['Alt-G', 'Clear location'],
['Shift-ALT-G', 'Remove selected objects from group'],
['Ctrl-G', 'Add selected objects to group'],
['Shift-G', 'Selected Group menu']
],

"H":[ 
['H', 'Hide selected vertices/faces'],
['H', 'Curves: Set handle type'],
['Alt-H', 'Show Hidden vertices/faces'],
['Ctrl-H', 'Curves: Automatic handle calculation'],
['Shift-H', 'Hide deselected  vertices/faces'],
['Shift-H', 'Curves: Set handle type']
],

"I":[ 
['I', 'Keyframe menu']
],

"J":[ 
['J', 'IPO: Join menu'],
['J', 'Mesh: Join all adjacent triangles to quads'],
['J', 'Render Window: Swap render buffer'],
['Ctrl-J', 'Join selected objects'],
['Ctrl-J', 'Nurbs: Add segment'],
['Ctrl-J', 'IPO: Join keyframes menu'],
['Alt-J', 'Edit Mode: convert quads to triangles']
],

"K":[  
['K', '3d Window: Show keyframe positions'],
['K', 'Edit Mode: Loop/Cut menu'],
['K', 'IPO: Show keyframe positions'],
['K', 'Nurbs: Print knots'],
['Ctrl-K', 'Make skeleton from armature'],
['Shift-K', 'Show and select all keyframes for object'],
['Shift-K', 'Edit Mode: Knife Mode select'],
['Shift-K', 'UV Face Select: Clear vertex colours'],
['Shift-K', 'Vertex Paint: Fill with vertex colours']
],

"L":[ 
['L', 'Make local menu'],
['L', 'Edit mode: Select linked vertices (near mouse pointer)'],
['L', 'OOPS window: Select linked objects'],
['L', 'UV Face Select: Select linked faces'],
['Ctrl-L', 'Make links menu'],
['Shift-L', 'Select links menu']
],

"M":[ 
['M', 'Move object to different layer'],
['M', 'Sequencer: Make meta strip (group) from selected strips'],
['M', 'Edit Mode: Mirros Axis menu'],
['Alt-M', 'Edit Mode: Merge vertices menu'],
['Ctrl-M', 'Object Mode: Mirros Axis menu']
],

"N":[ 
['N', 'Transform Properties panel'] ,
['N', 'OOPS window: Rename object/linked objects'] ,
['Ctrl-N', 'Armature: Recalculate bone roll angles'] ,
['Ctrl-N', 'Edit Mode: Recalculate normals to outside'] ,
['Ctrl-ALT-N', 'Edit Mode: Recalculate normals to inside'] ],

"O":[ 
['O', 'Edit Mode/UV Image Editor: Toggle proportional vertex editing'],
['Alt-O', 'Clear object origin'],
['Ctrl-O', 'Revert current file to last saved'],
['Shift-O', 'Proportional vertex Edit Mode: Toggle smooth/steep falloff']
],

"P":[ 
['P', 'Object Mode: Start realtime engine'],
['P', 'Edit mode: Seperate vertices to new object'],
['P', 'UV Image Editor: Pin UVs'],
['Alt-P', 'Clear parent relationship'],
['Alt-P', 'UV Image Editor: Unpin UVs'],
['Ctrl-P', 'Make active object parent of selected object'],
['Ctrl-SHIFT-P', 'Make active object parent of selected object without inverse'],
['Ctrl-P', 'Edit mode: Make active vertex parent of selected object']
],

"Q":[['Q', 'Quit'] ],

"R":[ 
['R', 'Rotate'],
['R', 'IPO: Record mouse movement as IPO curve'],
['R', 'UV Face Select: Rotate menu uv coords or vertex colour'],
['RX', 'Rotate around X axis'],
['RXX', "Rotate around object's local X axis"],
['RY', 'Rotate around Y axis'],
['RYY', "Rotate around object's local Y axis"],
['RZ', 'Rotate around Z axis'],
['RZZ', "Rotate around object's local Z axis"],
['Alt-R', 'Clear object rotation'],
['Ctrl-R', 'Edit Mode: Knife, cut selected edges, accept left mouse/ cancel right mouse'],
['Shift-R', 'Edit Mode: select Face Loop'],
['Shift-R', 'Nurbs: Select row'] ],

"S":[ 
['S', 'Scale'] ,
['SX', 'Flip around X axis'] ,
['SY', 'Flip around Y axis'] ,
['SZ', 'Flip around Z axis'] ,
['SXX', 'Flip around X axis and show axis'] ,
['SYY', 'Flip around Y axis and show axis'] ,
['SZZ', 'Flip around Z axis and show axis'] ,
['Alt-S', 'Edit mode: Shrink/fatten (Scale along vertex normals)'] ,
['Alt-S', 'Clear object size'] ,
['Ctrl-S', 'Edit mode: Shear'] ,
['Shift-S', 'Cursor/Grid snap menu'] ],

"T":[ 
['T', 'Sequencer: Touch and print selected movies'] ,
['T', 'Adjust texture space'] ,
['T', 'Edit mode: Flip 3d curve'] ,
['T', 'IPO: Change IPO type'] ,
['Alt-T', 'Clear tracking of object'] ,
['Ctrl-T', 'Make selected object track active object'] ,
['Ctrl-T', 'Edit Mode: Convert to triangles'] ,
['Ctrl-ALT-T', 'Benchmark'] ],

"U":[ 
['U', 'Make single user menu'] ,
['U', '3D View: Global undo'] ,
['U', 'Edit Mode: Reload object data from before entering Edit Mode'] ,
['U', 'UV Face Select: Automatic UV calculation menu'] ,
['U', 'Vertex-/Weightpaint mode: Undo'] ,
['Ctrl-U', 'Save current state as user default'],
['Shift-U', 'Edit Mode: Redo Menu'],
['Alt-U', 'Edit Mode: Undo Menu'] ],

"V":[ 
['V', 'Curves/Nurbs: Vector handle'],
['V', 'Vertexpaint mode'],
['V', 'UV Image Editor: Stitch UVs'],
['Alt-V', "Scale object to match image texture's aspect ratio"],
['Shift-V', 'Edit mode: Align view to selected vertices'],
['Shift-V', 'UV Image Editor: Limited Stitch UVs popup'],
],

"W":[ 
['W', 'Object Mode: Boolean operations menu'],
['W', 'Edit mode: Specials menu'],
['W', 'UV Image Editor: Weld/Align'],
['WX', 'UV Image Editor: Weld/Align X axis'],
['WY', 'UV Image Editor: Weld/Align Y axis'],
['Ctrl-W', 'Save current file'] ,
['Ctrl-W', 'Nurbs: Switch direction'] ,
['Shift-W', 'Warp/bend selected vertices around cursor'] ],

"X":[ 
['X', 'Delete menu'] ,
['Ctrl-X', 'Restore default state (Erase all)'] ],

"Y":[ 
['Y', 'Mesh: Split selected vertices/faces from the rest'] ],

"Z":[ 
['Z', 'Render Window: 200% zoom from mouse position'],
['Z', 'Switch 3d draw type : solide/ wireframe (see also D)'],
['Alt-Z', 'Switch 3d draw type : solid / textured (see also D)'],
['Ctrl-Z', 'Switch 3d draw type : shaded (see also D)'],
['Shift-Z', 'Switch 3d draw type : shaded / wireframe (see also D)'],

]}]}

up=128
down=129
UP=0

for k in hotkeys.keys():
   hotkeys[k].append(Create(0))

for k in hotkeys['Letters '][0]:
   hotkeys['Letters '][0][k].append(Create(0))

hotL=hotkeys['Letters '][0].keys()
hotL.sort()

hot=hotkeys.keys()
hot.sort()

glCr=glRasterPos2d
glCl3=glColor3f
glCl4=glColor4f
glRct=glRectf

cf=[0.95,0.95,0.9,0.0]
c1=[0.95,0.95,0.9,0.0]
c=cf
r=[0,0,0,0]

def trace_rectangle4(r,c):
    glCl4(c[0],c[1],c[2],c[3])
    glRct(r[0],r[1],r[2],r[3])

def trace_rectangle3(r,c,c1):
    glCl3(c[0],c[1],c[2])
    glRct(r[0],r[1],r[2],r[3])
    glCl3(c1[0],c1[1],c1[2])

def draw():
    global r,c,c1,hotkeys, hot, hotL, up, down, UP

    size=Buffer(GL_FLOAT, 4)
    glGetFloatv(GL_SCISSOR_BOX, size)
    size= size.list

    for s in [0,1,2,3]: size[s]=int(size[s])

    c=[0.75,0.75,0.75,0]
    c1=[0.6,0.6,0.6,0]

    r=[0,size[3],size[2],0]
    trace_rectangle4(r,c)

    c=[0.64,0.64,0.64,0]
    c1=[0.95,0.95,0.9,0.0]
    
    r=[0,size[3],size[2],size[3]-40]
    trace_rectangle4(r,c)

    c1=[0.7,0.7,0.9,0.0]
    c=[0.2,0.2,0.4,0.0]
    c2=[0.71,0.71,0.71,0.0]     

    glColor3f(1, 1, 1)
    glRasterPos2f(42, size[3]-25)

    Text("HotKey and MouseAction Reference")

    l=0
    listed=0
    Llisted=0
    size[3]=size[3]-18

    for k in hot:             
       #hotkeys[k][-1]=Toggle(k, hot.index(k)+10, 4+(20*26)/6*hot.index(k), size[3]-(42), len(k)*8, 20, hotkeys[k][-1].val )
       hotkeys[k][-1]=Toggle(k, hot.index(k)+10, 78*hot.index(k), size[3]-(47), 78, 24, hotkeys[k][-1].val )
       
       l+=len(k)

       if hotkeys[k][-1].val==1.0:
           listed=hot.index(k)
    l=0
    size[3]=size[3]-4
    if hot[listed]!='Letters ':
       size[3]=size[3]-8
       SCROLL=size[3]/21
       END=-1
       if SCROLL < len(hotkeys[hot[listed]][:-1]):
          Button('/\\',up,4,size[3]+8,20,14,'Scroll up') 
          Button('\\/',down,4,size[3]-8,20,14,'Scroll down')            
          if (SCROLL+UP)<len(hotkeys[hot[listed]][:-1]):
             END=(UP+SCROLL)
          else:
             END=-1
             UP=len(hotkeys[hot[listed]][:-1])-SCROLL         
       else :
         UP=0

       for n in  hotkeys[hot[listed]][:-1][UP:END]:
          
          if l%2==0:
             r=[0,size[3]-(21*l+66),
                     size[2], size[3]-(21*l+43)]
             trace_rectangle4(r,c2)
          glColor3f(0,0,0)
          glRasterPos2f(4+8, size[3]-(58+21*l))
          Text(n[0])
          glRasterPos2f(4+8*15, size[3]-(58+21*l))
          Text('  : '+n[1]) 
          l+=1
    else:
       for k in hotL:
            pos=hotL.index(k)
            hotkeys['Letters '][0][k][-1]=Toggle(k,pos+20,hotL.index(k)*21, size[3]-(52+18), 21, 18, hotkeys['Letters '][0][k][-1].val )
            if hotkeys['Letters '][0][k][-1].val==1.0:
               Llisted=pos

       size[3]=size[3]-8

       SCROLL=(size[3]-88)/21
       END=-1
       if SCROLL < len(hotkeys['Letters '][0][hotL[Llisted]]):
          Button('/\\',up,4,size[3]+8,20,14,'Scroll up') 
          Button('\\/',down,4,size[3]-8,20,14,'Scroll down')            
          if (UP+SCROLL)<len(hotkeys['Letters '][0][hotL[Llisted]]):
             END=(UP+SCROLL)
          else:
             END=-1
             UP=len(hotkeys['Letters '][0][hotL[Llisted]])-SCROLL         
       else :
         UP=0

       for n in hotkeys['Letters '][0][hotL[Llisted]][UP:END]:
          if l%2==0:
             r=[4,size[3]-(21*l+92),
                     size[2], size[3]-(69+21*l+1)]
             trace_rectangle4(r,c2)

          glColor3f(0.1, 0.1, 0.15)  
          glRasterPos2f(4+8, (size[3]-(88+21*l))+3)
          Text(n[0])
          glRasterPos2f(4+8*15, (size[3]-(88+21*l))+3)
          Text('  : '+n[1]) 
          l+=1

def event(evt, val):
    global hotkeys, UP     
    if ((evt== QKEY or evt== ESCKEY) and not val): 
        Exit()


def bevent(evt):
    global hotkeysmhot, hotL, up,down,UP

    if   (evt== 1):
        Exit()

    elif (evt in range(10,20,1)):
        for k in hot:
           if hot.index(k)+10!=evt:
                 hotkeys[k][-1].val=0
                 UP=0 
        Blender.Window.Redraw()

    elif (evt in range(20,46,1)):
        for k in hotL:
           if hotL.index(k)+20!=evt:
                 hotkeys['Letters '][0][k][-1].val=0
                 UP=0 
        Blender.Window.Redraw()

    elif (evt==up):
       UP+=1
       Blender.Window.Redraw()

    elif (evt==down):
       if UP>0: UP-=1
       Blender.Window.Redraw()

Register(draw, event, bevent)
