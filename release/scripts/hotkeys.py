#!BPY

""" Registration info for Blender menus:
Name: 'Hotkey Reference'
Blender: 232
Group: 'Help'
Tip: 'All the hotkeys'
""" 
# $Id$
#------------------------
#  Hotkeys script
#         jm soler (2003)
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
# Copyright (C) 2003, 2004: Jean-Michel Soler
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
['.', 'Set 3D cursor as rotation scaling pivot'] ,
['~', 'Display all layers'] ,
['Shift-~', 'Display all/previous layers'] ,
['TAB', 'Enter/exit edit mode'] ,
['TAB', 'Edit Mode and Numerical Edit (see N key) : move to next input value'],
['TAB', 'Sequencer: Edit meta strip'] ,
['CTRL-TAB', 'Enter/exit pose mode']
],

'Specials 2 ':[
['F1', 'Open File'],
['F2', 'Save File'],
['F3', 'Save image'],
['F4', 'Logic window (may change)'],
['F5', 'Material window'],
['F6', 'Texture window'],
['F7', 'Object window'],
['F8', 'World window'],
['F9', 'Edit Mode window'],
['F10', 'Render Window'],
['F11', 'Recall the last rendered image'],
['F12', 'Render current Scene'],
['Shift-F1', 'Library Data Select'],
['Shift-F4', 'Data Select '],
['Shift-F5', '3D window'],
['Shift-F6', 'IPO window'],
['Shift-F8', 'Video Sequencer window'],
['Shift-F9', 'OOP window'],
['Shift-F10', 'Image Window']
] ,

'Numbers  ':[
['1..2..0-=', 'Show layer 1..2..12'] ,
['ALT-1..2..0', 'Show layer 11..12..20'] ,
['SHIFT-1..2..0-=', 'Toggle layer 1..2..12'] ,
['SHIFT-ALT-...', 'Toggle layer 11..12..20'] ],

'Numpad  ':[
['Numpad DEL', 'Zoom on object'] ,
['Numpad /', 'Local view on object (hide others)'] ,
['Numpad *', "Rotate view to object's local axes"] ,
['Numpad +', 'Zoom in (works everywhere)'] ,
['Numpad +', 'Proportional vertex edit mode: Increase range of influence'] ,
['Numpad -', 'Zoom out (works everywhere)'] ,
['Numpad -', 'Proportional vertex edit mode: Decrease range of influence'] ,
['Numpad INS', 'Set Camera view'] ,
['CTRL-Numpad INS', 'Set active object as camera'] ,
['ALT-Numbad INS', 'Restore old camera'] ,
['Numpad 1', 'Front view'] ,
['CTRL-Numpad 1', 'Back view'] ,
['Numpad 3', 'Right-Side view'] ,
['CTRL-Numpad 3', 'Left-Side view'] ,
['Numpad 7', 'Top view'] ,
['CTRL-Numpad 7', 'Bottom view '] ,
['Numpad 5', 'Toggle orthogonal // perspective view'] ,
['Numpad 9', 'Redraw view'] ,
['Numpad 2', 'Rotate view left'] ,
['Numpad 6', 'Rotate view right'] ,
['Numpad 8', 'Rotate view up'] ,
['Numpad 2', 'Rotate view down'] ],

'Arrows ':[
['PgUp', 'IPO: Select next keyframe'] ,
['CTRL-PgUp', 'IPO: Select and jump to next keyframe'] ,
['PgDn', 'IPO: Select previous keyframe'] ,
['CTRL-PgDn', 'IPO: Select and jump to previous keyframe'] ,
['LEFT', 'One frame backwards'] ,
['RIGHT', 'One frame forwards'] ,
['DOWN', '10 frames backwards'] ,
['UP', '10 frames forwards'] ],

'Letters ':[ {"A":[ 
['A', 'Select all / Deselect all'] ,
['ALT-A', 'Animate current window'] ,
['CTRL-A', "Apply object's size/rotation to object data"] ,
['SHIFT-A', 'Sequencer: ADD menu'] ,
['SHIFT-ALT-A', 'Animate all windows'] ,
['SHIFT-CTRL-A', 'Apply lattice / Make dupliverts real']] ,

"B":[ 
['B', 'Border select'] ,
['BB', 'Circle select'] ,
['SHIFT-B', 'Set render border'] ],

"C":[ 
['C', 'Center view on cursor'] ,
['C', 'Sequencer: Change images'] ,
['C', 'IPO: Snap current frame to selected key'] ,
['ALT-C', 'Convert menu'] ,
['CTRL-C', 'Copy menu (Copy properties of active to selected objects)'] ,
['SHIFT-C', 'Center and zoom view on selected objects']] ,

"D":[  
['D', 'Set 3d draw mode'] ,
['ALT-D', 'Create new instance of object'] ,
['CTRL-D', 'Display alpha of image texture as wire'] ,
['SHIFT-D', 'Create full copy of object'] ],

"E":[ 
['E', 'Extrude'],
['EX', 'Extrude along X axis'],
['EY', 'Extrude along Y axis'],
['EZ', 'Extrude along Z axis'],
['ALT-E', 'Edit mode: exit edit mode'],] ,

"F":[ 
['F', 'Edit mode: Make edge/face'] ,
['F', 'Sequencer: Set Filter Y'] ,
['F', 'Faceselect mode'] ,
['ALT-F', 'Beautify fill'] ,
['CTRL-F', 'Sort faces in Z direction'] ,
['CTRL-F', 'Edit mode: Flip triangle edges'] ,
['SHIFT-F', 'Edit mode: Fill with triangles']] ,

"G":[ 
['G', 'Grab (move)'] ,
['ALT-G', 'Clear location'] ,
['SHIFT-ALT-G', 'Remove selected objects from group'] ,
['CTRL-G', 'Add selected objects to group'] ,
['SHIFT-G', 'Group menu'] ],

"H":[ 
['H', 'Hide selected vertices/faces'] ,
['H', 'Curves: Set handle type'] ,
['ALT-H', 'Reveal vertices'] ,
['CTRL-H', 'Curves: Automatic handle calculation'] ,
['SHIFT-H', 'Hide deselected vertices'] ,
['SHIFT-H', 'Curves: Set handle type']] ,

"I":[ 
['I', 'Keyframe menu'] ],

"J":[ 
['J', 'Mesh: Join all adjacent triangles to quads'] ,
['J', 'Swap render page of render window'] ,
['CTRL-J', 'Join selected objects'] ,
['CTRL-J', 'Nurbs: Add segment'] ,
['CTRL-J', 'IPO: Join keyframes menu'],
['ALT-J', 'Edit Mode: convert quads to triangles']
],

"K":[  
['K', '3d window: Show keyframe positions'] ,
['K', 'IPO: Show keyframe positions'] ,
['K', 'Nurbs: Print knots'] ,
['CTRL-K', 'Make skeleton from armature'] ,
['SHIFT-K', 'Show and select all keyframes for object'] ,
['SHIFT-K', 'Edit: Knife Mode select'],
['SHIFT-K', 'Faceselect: Clear vertexcolours'],
] ,

"L":[ 
['L', 'Make local menu'] ,
['L', 'Edit mode: Select linked vertices (near mouse pointer)'] ,
['L', 'OOPS window: Select linked objects'] ,
['CTRL-L', 'Make links menu'] ,
['SHIFT-L', 'Select links menu'] ],

"M":[ 
['M', 'Move object to different layer'] ,
['M', 'Sequencer: Make meta strip (group) from selected strips'],
['ALT-M', 'Edit Mode: Merge vertices'] ],

"N":[ 
['N', 'Numeric input menu (Size/Rot/Loc)'] ,
['N', 'OOPS window: Rename object/linked objects'] ,
['CTRL-N', 'Armature: Recalculate bone roll angles'] ,
['CTRL-N', 'Recalculate normals to outside'] ,
['CTRL-ALT-N', 'Recalculate normals to inside'] ],

"O":[ 
['O', 'Edit mode: Toggle proportional vertex editing'] ,
['ALT-O', 'Clear object origin'] ,
['CTRL-O', 'Revert current file to last saved'] ,
['SHIFT-O', 'Proportional vertex edit mode: Toggle smooth/steep falloff'] ],

"P":[ 
['P', 'Start realtime engine'] ,
['P', 'Edit mode: Seperate vertices to new object'] ,
['ALT-P', 'Clear parent relationship'] ,
['CTRL-P', 'Make active object parent of selected object'] ,
['CTRL-SHIFT-P', 'Make active object parent of selected object without inverse'] ,
['CTRL-P', 'Edit mode: Make active vertex parent of selected object'] ],

"Q":[['Q', 'Quit'] ],

"R":[ 
['R', 'Rotate'] ,
['R', 'IPO: Record mouse movement as IPO curve'] ,
['RX', 'Rotate around X axis'] ,
['RXX', "Rotate around object's local X axis"] ,
['RY', 'Rotate around Y axis'] ,
['RYY', "Rotate around object's local Y axis"] ,
['RZ', 'Rotate around Z axis'] ,
['RZZ', "Rotate around object's local Z axis"] ,
['ALT-R', 'Clear object rotation'] ,
['SHIFT-R', 'Nurbs: Select row'], 
['CTRL-R', 'Edit Mode: Knife, cut selected edges, accept left mouse/ cancel right mouse'],
['SHIT-R', 'Edit Mode: loop Selection']],

"S":[ 
['S', 'Scale'] ,
['SX', 'Flip around X axis'] ,
['SY', 'Flip around Y axis'] ,
['SZ', 'Flip around Z axis'] ,
['SXX', 'Flip around X axis and show axis'] ,
['SYY', 'Flip around Y axis and show axis'] ,
['SZZ', 'Flip around Z axis and show axis'] ,
['ALT-S', 'Edit mode: Shrink/fatten (Scale along vertex normals)'] ,
['ALT-S', 'Clear object size'] ,
['CTRL-S', 'Edit mode: Shear'] ,
['SHIFT-S', 'Cursor/Grid snap menu'] ],

"T":[ 
['T', 'Sequencer: Touch and print selected movies'] ,
['T', 'Adjust texture space'] ,
['T', 'Edit mode: Flip 3d curve'] ,
['T', 'IPO: Change IPO type'] ,
['ALT-T', 'Clear tracking of object'] ,
['CTRL-T', 'Make selected object track active object'] ,
['CTRL-T', 'Mesh: Convert to triangles'] ,
['CTRL-ALT-T', 'Blenchmark'] ],

"U":[ 
['U', 'Make single user menu'] ,
['U', 'Edit mode: Reload object data from before entering edit mode'] ,
['U', 'Faceselect mode: Automatic UV calculation menu'] ,
['U', 'Vertex-/Weightpaint mode: Undo'] ,
['CTRL-U', 'Save current state as user default'],
['SHIFT-U', 'EditMode : Redo Menu'],
['ALT-U', 'Edit Mode: Undo Menu']
 ],

"V":[ 
['V', 'Curves/Nurbs: Vector handle'] ,
['V', 'Vertexpaint mode'] ,
['ALT-V', "Scale object to match image texture's aspect ratio"] ,
['SHIFT-V', 'Edit mode: Align view to selected vertices'] ],

"W":[ 
['W', 'Boolean operations menu'] ,
['W', 'Edit mode: Specials menu'] ,
['CTRL-W', 'Save current file'] ,
['CTRL-W', 'Nurbs: Switch direction'] ,
['SHIFT-W', 'Warp/bend selected vertices around cursor'] ] ,

"X":[ 
['X', 'Delete menu'] ,
['CTRL-X', 'Restore default state (Erase all)'] ],

"Y":[ 
['Y', 'Mesh: Split selected vertices/faces from the rest'] ],

"Z":[ 
['Z', 'Switch 3d draw type : solide/ wireframe (see also D)'],
['Alt-Z', 'Switch 3d draw type : solid / textured (see also D)'],
['Shift-Z', 'Switch 3d draw type : shaded / wireframe (see also D)'],

]}]}


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
    global r,c,c1,hotkeys, hot, hotL

    size=Buffer(GL_FLOAT, 4)
    glGetFloatv(GL_SCISSOR_BOX, size)
    size= size.list

    for s in [0,1,2,3]: size[s]=int(size[s])

    c=[0.9,0.95,0.95,0.0]
    c1=[0.95,0.95,0.9,0.0]

    r=[0,size[3],size[2],0]
    trace_rectangle4(r,c)

    c=[0.7,0.7,0.9,0.0]
    c1=[0.95,0.95,0.9,0.0]
    
    r=[0,size[3],size[2],size[3]-20]
    trace_rectangle4(r,c)

    c1=[0.7,0.7,0.9,0.0]
    c=[0.2,0.2,0.4,0.0]
    c2=[0.87,0.87,0.95,0.0]     

    r=[0,size[3]-20,size[2],size[3]-44]
    trace_rectangle4(r,c)

    glColor3f(0.1, 0.1, 0.15)
    glRasterPos2f(10, size[3]-16)

    Text("HotKey")

    l=0
    listed=0
    Llisted=0
    for k in hot:             
       hotkeys[k][-1]=Toggle(k, hot.index(k)+10, 4+(20*26)/6*hot.index(k), size[3]-(40), len(k)*8, 20, hotkeys[k][-1].val )
       l+=len(k)
       if hotkeys[k][-1].val==1.0:
           listed=hot.index(k)
           #print listed
    l=0
    if hot[listed]!='Letters ':
       for n in  hotkeys[hot[listed]][:-1]:
          if l%2==0:
             r=[4,size[3]-(18*l+66),
                     8+(21*26), size[3]-(46+18*l)]
             trace_rectangle4(r,c2)
          glColor3f(0.1, 0.1, 0.15)
          glRasterPos2f(4+8, size[3]-(58+18*l))
          Text(n[0])
          glRasterPos2f(4+8*15, size[3]-(58+18*l))
          Text('  : '+n[1]) 
          l+=1
    else:
       for k in hotL:
            pos=hotL.index(k)
            hotkeys['Letters '][0][k][-1]=Toggle(k,pos+20,4+hotL.index(k)*21, size[3]-(52+18), 20, 20, hotkeys['Letters '][0][k][-1].val )
            if hotkeys['Letters '][0][k][-1].val==1.0:
               Llisted=pos
       for n in hotkeys['Letters '][0][hotL[Llisted]][:-1]:
          if l%2==0:
             r=[4,size[3]-(18*l+92),
                     8+(21*26), size[3]-(74+18*l)]
             trace_rectangle4(r,c2)
          glColor3f(0.1, 0.1, 0.15)  
          glRasterPos2f(4+8, size[3]-(88+18*l))
          Text(n[0])
          glRasterPos2f(4+8*15, size[3]-(88+18*l))
          Text('  : '+n[1]) 
          l+=1

def event(evt, val):
    global hotkeys     
    if ((evt== QKEY or evt== ESCKEY) and not val): Exit()

def bevent(evt):
    global hotkeysmhot, hotL
    if   (evt== 1):
        Exit()

    elif (evt in range(10,20,1)):
        for k in hot:
           if hot.index(k)+10!=evt:
                 hotkeys[k][-1].val=0
                 
        Blender.Window.Redraw()

    elif (evt in range(20,46,1)):
        for k in hotL:
           if hotL.index(k)+20!=evt:
                 hotkeys['Letters '][0][k][-1].val=0

        Blender.Window.Redraw()

Register(draw, event, bevent)
