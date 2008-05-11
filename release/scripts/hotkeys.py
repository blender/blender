#!BPY
# coding: utf-8
""" Registration info for Blender menus:
Name: 'HotKey and MouseAction Reference'
Blender: 242
Group: 'Help'
Tip: 'All the hotkeys/short keys'
""" 

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "blenderartist",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_hotkeyscript.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "21/01/2007"

__bpydoc__ = """\
This script is a reference about all hotkeys and mouse actions in Blender.

Usage:

Open the script from the Help menu and select group of keys to browse.

Notes:<br>
    Additional entries in the database (c) 2004 by Bart.
    Additional entries in the database for blender 2.37 --> 2.43 (c) 2003-2007/01 by jms.
    
"""

#------------------------
#  Hotkeys script
#        (c) jm soler (2003-->01/2007)
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

# INTERNATIONAL={0:'English','1':'French'}
# LANGUAGE=0

hotkeys={
'Search ':[['', '']],
'Specials 1 ':[
[',', 'Set Bounding Box rotation scaling pivot'],
['Ctrl-,', 'Set Median Point rotation scaling pivot'],
['.', 'Set 3D cursor as rotation scaling pivot'],
['.', 'Outliner : to get the current active data in center of view'],
['Ctrl-.', 'Set Individual Object Centers as rotation scaling pivot'],
['~', 'Display all layers (German keys: ö,french keyboard: ù)'],
['Shift-~', 'Display all/previous layers (German keys: Shift-ö, french keyboard: shift-ù)'],
['ENTER', 'Outliner : to open a subtree, works on entire item line. '],	
['HOME', 'Outliner :  to show the entire Object hierarchy. '],
['SHIFT+BACKSPACE',' Text edit mode: Clear text '],
['SPACE', 'Popup menu'],
['SPACE', '3D View: camera selected'],
['Ctrl-SPACE', 'Manipulator (transform widget) Menu'],
['TAB', 'Enter/exit Edit Mode'],
['TAB', 'Edit Mode and Numerical Edit (see N key) : move to next input value'],
['TAB', 'Sequencer: Edit meta strip'],
['TAB', 'IPO: Edit selected'],
['TAB', 'Text Editor : indent'],
['TAB', 'NODE window : Edit group'], #243	
['Shift-TAB', 'Text Editor : unindent'],
['Shift-TAB', 'Edit Mode: Toggle snaping'],
['Ctrl-TAB', 'ARMATURE : Enter/exit Pose Mode'],
['Ctrl-TAB','MESH : all views, enter exit weight paint mode.'],
['Shift-TAB', 'Edit Mode : Enter Object Mode'],
['Ctrl-Open menu /', ''],
['Ctrl-Load Image', 'Opens a thumbnail browser instead of file browser for images'],
['.', '...']
],

'Mouse ':[
['Actions:', ''],
['LMB', '3D View: Set 3D Cursor'],
['LMB', '3D View: camera selected'],
['LMB drag', 'Border select circle: add to selection'],
['LMB hold down', 'Popup menu'],
['LMB hold down drag', 'Gesture'],
['Ctrl-LMB', 'IPO: Add key'],
['Ctrl-LMB', '3D View: OBJECT or EDIT mode, select with the Lasso tool'],
['Ctrl-LMB', '3D View: ARMATURE EDIT mode, add a new bone to the selected end '],
['Shift-LMB','MANIPULATOR (transform widget): select the axe to remove in the current'],
['Shift-LMB','MANIPULATOR      transformation ( if there is a problem with small step adjustment'], 
['Shift-LMB','MANIPULATOR      first select the axe or axes with LBM alone)'],
['Shift-LMB', 'Outliner : Hold Shift while clicking on a triangle arrow to open/close the subtree below'],
['MMB', 'Rotate'],
['Ctrl-MMB', 'Zoom view'],
['Ctrl-LMB', 'Outliner : Hold CTRL while clicking on a name allows you to edit a name.'],
['Ctrl-LMB', 'Outliner : 	 This works for all visualized data, including bones or vertex groups,'],
['Ctrl-LMB', 'Outliner : 	 but not for \'nameless\' items that draw the links to Hooks, Deform '],
['Ctrl-LMB', 'Outliner :   Groups or Constraints.'],
['Shift-MMB', 'Move view'],
['RMB', 'Select'],
['RMB drag', 'Border select circle: subtract from selection'],
['RMB hold down', 'Popup menu'],
['Alt-RMB', 'Object Mode :Select but in a displayed list of objects located under the mouse cursor'],
['Alt-RMB', 'Edit Mode: Select EDGES LOOP '],
['Alt+Ctrl-RMB', 'Edit Mode: Select FACES LOOP'],	
['Alt+Ctrl-RMB', 'UV Image Editor: Select face'],
['Shift-RMB', 'Add/subtract to/from selection'],
['Wheel', 'Zoom view'],
['Transformations:', ''],
['Drag+Ctrl', 'Step adjustment'],
['Drag+Ctrl+Shift', 'Small step adjustment (Transform Widget : first select the axe or axes with LBM alone)'],
['Drag+Shift', 'Fine adjustment (Transform Widget : first select the axe or axes with LBM alone)'],
['LMB', 'Confirm transformation'],
['MMB', 'Toggle optional transform feature'],
['RMB', 'Abort transformation'],
['.', '...']
],

'F-Keys ':[
['F1', 'Open File'],
['Shift-F1', 'Library Data Select'],
['F2', 'Save File'],
['Shift-F2', 'Export DXF'],
['Ctrl-F2', 'Save/export in vrml 1.0 format' ],
['F3', 'Save image'],
['Ctrl-F3', 'Save image : dump 3d view'],
['Ctrl-Shift-F3', 'Save image : dump screen'],
['F4', 'Logic Window (may change)'],
['Shift-F4', 'Object manager Data Select '],
['F5', 'Material Window'],
['Shift-F5', '3D Window'],
['F6', 'Texture Window'],
['Shift-F6', 'IPO Window'],
['F7', 'Object Window'],
['Shift-F7', 'Buttons Window'],
['F8', 'World Window'],
['Shift-F8', 'Video Sequencer Window'],
['F9', 'Edit Mode Window'],
['Shift-F9', 'OOP Window'],
['Alt-Shift-F9', 'OutLiner Window'],
['F10', 'Render Window'],
['Shift-F10', 'UV Image Editor'],
['F11', 'Recall the last rendered image'],
['Shift-F11', 'Text Editor'],
['ctrl-F11', 'replay the last rendered animation'],
['F12', 'Render current Scene'],
['Ctrl-F12', 'Render animation'],
['Ctrl-Shift-F12', 'NLA Editor'],
['Shift-F12', 'Action Editor'],
['Shift-F12', 'Action Editor'],
['.', '...']
],

'Numbers ':[
['1..2..0-=', 'Show layer 1..2..12'],
['1..2..0-=', 'Edit Mode with Size, Grab, rotate tools : enter value'],
['Alt-1..2..0', 'Show layer 11..12..20'],
['Shift-1..2..0', 'Toggle layer 1..2..12'],
['Ctrl-1..4', 'Object/Edit Mode : change subsurf level to the selected value'],
['Shift-ALT-...', 'Toggle layer 11..12..20'],
['Crtl-Shift-ALT-3', 'Edit Mode & Face Mode : Triangle faces'],
['Crtl-Shift-ALT-4', 'Edit Mode & Face Mode : Quad faces'],
['Crtl-Shift-ALT-5', 'Edit Mode & Face Mode : Non quad or triangle faces'],
['.', '...']
],

'Numpad ':[
['Numpad DEL', 'Zoom on object'],
['Numpad /', 'Local view on object (hide others)'],
['Numpad *', 'Rotate view to objects local axes'],
['Numpad +', 'Zoom in (works everywhere)'],
['Numpad -', 'OutLiner window, Collapse one level of the  hierarchy'],
['Alt-Numpad +', 'Proportional vertex Edit Mode: Increase range of influence'],
['Ctrl-Numpad +', 'Edit Mode: Select More vertices'],
['Numpad -', 'Zoom out (works everywhere)'],
['Numpad +', 'OutLiner window, Expand one level of the  hierarchy'],
['Alt-Numpad -', 'Proportional vertex Edit Mode: Decrease range of influence'],
['Ctrl-Numpad +', 'Edit Mode: Select Less vertices'],
['Numpad 0', 'Set Camera view'],
['Ctrl-Numpad 0', 'Set active object as camera'],
['Alt-Numbad 0', 'Restore old camera'],
['Ctrl-Alt-Numpad 0', 'Align active camera to view'],
['Numpad 1', 'Front view'],
['Ctrl-Numpad 1', 'Back view'],
['Numpad 3', 'Right view'],
['Ctrl-Numpad 3', 'Left view'],
['Numpad 7', 'Top view'],
['Ctrl-Numpad 7', 'Bottom view '],
['Numpad 5', 'Toggle orthogonal/perspective view'],
['Numpad 9', 'Redraw view'],
['Numpad 4', 'Rotate view left'],
['ctrl-Shift-Numpad 4', 'Previous Screen'],
['Numpad 6', 'Rotate view right'],
['ctrl-Shift-Numpad 6', 'Next Screen'],
['Numpad 8', 'Rotate view up'],
['Numpad 2', 'Rotate view down'],
['.', '...']
],

'Arrows ':[
['Home/Pos1', 'View all',''],
['Home', 'OutLiner Windows, Show hierarchy'],
['PgUp', 'Edit Mode and Proportionnal Editing Tools, increase influence'],
['PgUp', 'Strip Editor, Move Down'],
['PgUn', 'TimeLine: Jump to next marker'],
['PgUp', 'IPO: Select next keyframe'],
['Ctrl-PgUp', 'IPO: Select and jump to next keyframe'],
['Ctrl-PgUn', 'TimeLine: Jump to next key'],	
['PgDn', 'Edit Mode and Proportionnal Editing Tools, decrease influence'],
['PgDn', 'Strip Editor, Move Up'],
['PgDn', 'TimeLine: Jump to prev marker'],
['PgDn', 'IPO: Select previous keyframe'],
['Ctrl-PgDn', 'IPO: Select and jump to previous keyframe'],
['Ctrl-PgDn', 'TimeLine: Jump to prev key'],		
['Left', 'One frame backwards'],
['Right', 'One frame forwards'],
['Down', '10 frames backwards'],
['Up', '10 frames forwards'],
['Alt-Down', 'Blender in Window mode'],
['Alt-Up', 'Blender in Fullscreen mode'],
['Ctrl-Left', 'Previous screen'],
['Ctrl-Right', 'Next screen'],
['Ctrl-Alt-C', 'Object Mode : Add  Constraint'],
['Ctrl-Down', 'Maximize window toggle'],
['Ctrl-Up', 'Maximize window toggle'],
['Shift-Arrow', 'Toggle first frame/ last frame'],
['.', '...']
],

'Letters ':[ 
{
"A":[ 
['A', 'Select all/Deselect all'],
['A', 'Outliner : Select all/Deselect all'],
['A', 'Ipo Editor : Object mode, Select all/Deselect all displayed Curves'],  #243
['A', 'Ipo Editor : Edit mode, Select all/Deselect all vertices'], #243
['A', 'Render window (F12) : Display alpha plane'],
['Alt-A', 'Play animation in current window'],
['Ctrl-A', 'Apply objects size/rotation to object data'],
['Ctrl-A', 'Text Editor: Select all'],
['Shift-A', 'Sequencer: Add menu'],
['Shift-A', '3D-View: Add menu'],
['Shift-ALT-A', 'Play animation in all windows'],
['Shift-CTRL-A', 'Apply lattice / Make dupliverts real'],
['Shift-CTRL-A', 'Apply Deform '],
['.', '...']
],

"B":[ 
['B', 'Border select'],
['BB', 'Circle select'],
['Alt+B', 'Object Mode: Select visible view section in 3D space'],
['Shift-B', 'Set render border (in active camera view)'],
['Ctrl-Alt+B', 'Object Mode: in 3D view, Bake (on an image in the uv editor window) the selected Meshes'], #243
['Ctrl-Alt+B', 'Object Mode: in 3D view, Bake Full render of selected Meshes'],	 #243
['Ctrl-Alt+B', 'Object Mode: in 3D view, Bake Ambient Occlusion of selected Meshes'],  #243	
['Ctrl-Alt+B', 'Object Mode: in 3D view, Bake Normals of the selected Meshes'],	 #243
['Ctrl-Alt+B', 'Object Mode: in 3D view, Bake Texture Only of selected Meshes'],	#243
['.', '...']
],

"C":[ 
['C', 'Center view on cursor'],
['C', 'UV Image Editor: Active Face Select toggle'],
['C', 'Sequencer: Change content of the strip '], #243
['C', 'IPO: Snap current frame to selected key'],
['C', 'TimeLine: Center View'],	
['C', 'File Selector : Copy file'],
['C', 'NODE window : Show cyclic referencies'], #243				
['Alt-C', 'Object Mode: Convert menu'],
['Alt-C', 'Text Editor: Copy '],
['Ctrl-Shift-C', 'Text Editor: Copy selection to clipboard'],
['Ctrl-C', 'Copy menu (Copy properties of active to selected objects)'],
['Ctrl-C', 'UV Image Editor: Stick UVs to mesh vertex'],
['Ctrl-C','ARMATURE : posemode, Copy pose attributes'],
['Ctrl+Alt-C',' ARMATURE : posemode, add constraint to new empty object.'],
['Shift-C', 'Center and zoom view on selected objects'],
['Shift-C', 'UV Image Editor: Stick local UVs to mesh vertex'],
['.', '...']
],

"D":[  
['D', 'Set 3d draw mode'],
['Alt-D', 'Object Mode: Create new instance of object'],
['Ctrl-D', 'Display alpha of image texture as wire'],
['Ctrl-D', 'Text Editor : uncomment'],
['Shift-D', 'Create full copy of object'],
['Shift-D', 'NODE window : duplicate'], #243	
['CTRL-SHIFT-D', 'NLA editor : Duplicate markers'],
['CTRL-SHIFT-D', 'Action editor : Duplicate markers'],	
['CTRL-SHIFT-D', 'IPO editor : Duplicate markers'],		
['.', '...']
],

"E":[ 
['E', 'Edit Mode: Extrude'],
['E', 'UV Image Editor: LSCM Unwrap'],
['E', 'TimeLine: Set current frame as End '],	
['E', 'NODE window : Execute composite'], #243		
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
['Ctrl-E', 'Edit Mode: Edge Specials menu, Mark seams'],
['Ctrl-E', 'Edit Mode: Edge Specials menu, Clear seams'],
['Ctrl-E', 'Edit Mode: Edge Specials menu, Rotate Edge CW'],
['Ctrl-E', 'Edit Mode: Edge Specials menu, Rotate Edge CCW'],
['Ctrl-E', 'Edit Mode: Edge Specials menu, Loop Cut'],
['Ctrl-E', 'Edit Mode: Edge Specials menu, Edge Slide'],
['Shift-E', 'Edit Mode: SubSurf Edge Sharpness'],
['.', '...']
],

"F":[ 
['F', 'Edit mode: Make edge/face'],
['F', 'Sequencer: Set Filter Y'],
['F', 'Object Mode: UV/Face Select mode'],
['Alt-F', 'Edit Mode: Beautify fill'],
['Alt-F,','Text editor : find again '],
['Alt-Ctrl-F,','Text editor : find '],
['Ctrl-F', 'Object Mode: Sort faces in Z direction'],
['Ctrl-F', 'Edit Mode: Flip triangle edges'],
['Shift-F', 'Edit Mode: Fill with triangles'],
['Shift-F', 'Object Mode: fly mode (see header for fly mode keys)'],
['.', '...']
],

"G":[ 
['G', 'Grab (move)'],
['G', 'Timeline : Grab (move) Marker'],
['Alt-G', 'Clear location (this does only make sense in Object mode)'],
['Alt-G', 'NODE window : ungroup'], #243
['Shift-ALT-G', 'Object mode: Remove selected objects from group'],
['Ctrl-G', 'NODE window : group'], #243	
['Ctrl-G', 'Add selected objects to group'],
['Ctrl-G', 'IPO editor, Grab/move marker'],
['Ctrl-Alt-G', 'MANIPULATOR (transform widget): set in Grab Mode'],
['Shift-G', 'Object mode: Selected Group menu'],
['Shift-G', 'Object mode: Selected Group menu 1, Children'],
['Shift-G', 'Object mode: Selected Group menu 2, Immediate Children'],
['Shift-G', 'Object mode: Selected Group menu 3, Parent'],
['Shift-G', 'Object mode: Selected Group menu 4, Sibling'],
['Shift-G', 'Object mode: Selected Group menu 5, Object of same type'],
['Shift-G', 'Object mode: Selected Group menu 6, Object in same shared layers'],
['Shift-G', 'Object mode: Selected Group menu 7, Objects in same group'],
['.', '...']
],

"H":[ 
['H', 'Hide selected vertices/faces'],
['H', 'Curves: Set handle type'],
['H', 'Action editor: Handle type aligned'],
['H', 'Action editor: Handle type free'],		
['H', 'NODE window : hide/unhide'], #243		
['Alt-H', 'Edit Mode : Show Hidden vertices/faces'],
['Shift-H', 'Curves: Automatic handle calculation'],
['Shift-H', 'Action editor: Handle type auto'],	
['Shift-H', 'Edit Mode : Hide deselected  vertices/faces'],
['Ctrl-H', 'Edit Mode : Add a hook on selected points or show the hook menu .'],
['.', '...']
],

"I":[ 
['I', 'Keyframe menu'],
['Alt-I','ARMATURE : posemode, remove IK constraints.'],
['Ctrl-I','ARMATURE : add IK constraint'],
['.', '...']
],

"J":[ 
['J', 'IPO: Join menu'],
['J', 'Mesh: Join all adjacent triangles to quads'],  
['J', 'Render Window: Swap render buffer'],
['Alt-J,','Text editor : Jump '],
['Ctrl-J', 'Join selected objects'],
['Ctrl-J', 'Nurbs: Add segment'],
['Ctrl-J', 'IPO: Join keyframes menu'],
['.', '...']
],

"K":[  
['K', '3d Window: Show keyframe positions'],
['K', 'Edit Mode: Loop/Cut menu'],
['K', 'IPO: Show keyframe positions'],
['K', 'Nurbs: Print knots'],
['K', 'VIDEO editor : cut at current frame'], #243		
['Ctrl-K', 'Make skeleton from armature'],
['Shift-K', 'Show and select all keyframes for object'],
['Shift-K', 'Edit Mode: Knife Mode select'],
['Shift-K', 'UV Face Select: Clear vertex colours'],
['Shift-K', 'Vertex Paint: All vertex colours are erased; they are changed to the current drawing colour.'],
['.', '...']
],

"L":[ 
['L', 'Make local menu'],
['L', 'Edit Mode: Select linked vertices (near mouse pointer)'],
['L', 'NODE window: Select linked from '], #243	
['L', 'OOPS window: Select linked objects'],
['L', 'UV Face Select: Select linked faces'],
['Ctrl-L', 'Make links menu (for instance : to scene...)'],
['Shift-L', 'Select links menu'],
['Shift-L', 'NODE window: Select linked to '], #243
['.', '...']
],

"M":[ 
['M', 'Object mode : Move object to different layer'],
['M', 'Sequencer: Make meta strip (group) from selected strips'],
['M', 'Edit Mode: Mirros Axis menu'],
['M', 'File Selector: rename file'],
['M', 'Video Sequence Editor : Make Meta strip...'],		
['M', 'NLA editor: Add marker'],	
['M', 'Action editor: Add marker'],		
['M', 'IPO editor: Add marker'],	
['M', 'TimeLine: Add marker'],	
['Alt-M', 'Edit Mode: Merge vertices menu'],
['Alt-M', 'Video Sequence Editor : Separate Meta strip...'],	
['Ctrl-M', 'Object Mode: Mirros Axis menu'],
['Shift-M', 'TimeLine: Name marker'],
['Shift-M', 'IPO editor : Name marker'],
['Shift-M', 'NLA editor : Name marker'],
['Shift-M', 'Actions editor : Name marker'],	
['.', '...']
],

"N":[ 
['N', 'Transform Properties panel'] ,
['N', 'OOPS window: Rename object'],
['N', 'VIDEO SEQUENCE editor : display strip properties '], #243	
['Alt-N', 'Text Editor : New text '],
['Ctrl-N', 'Armature: Recalculate bone roll angles'] ,
['Ctrl-N', 'Edit Mode: Recalculate normals to outside'] ,
['Ctrl-Shift-N', 'Edit Mode: Recalculate normals to inside'],
['.', '...']
],

"O":[ 
['O', 'Edit Mode/UV Image Editor: Toggle proportional vertex editing'],
['O', 'IPO editor: Clean ipo curves (beware to the thresold needed value)'], #243
['Alt-O', 'Clear object origin'],
['Alt-O', 'Edit mode, 3dview with prop-edit-mode, enables/disables connected'],
['Alt-O', 'Text Editor : Open file '],
['Ctrl-O', 'Open a panel with the ten most recent projets files'], #243
['Shift-O', 'Proportional vertex Edit Mode: Toggle smooth/steep falloff'],
['Shift-O', 'Object Mode: Add a subsurf modifier to the selected mesh'],
['Shift-O', 'IPO editor: Smooth ipo curves'],	#243
['.', '...']
],

"P":[ 
['P', 'Object Mode: Start realtime engine'],
['P', 'Edit mode: Seperate vertices to new object'],
['shift-P', 'Edit mode: Push-Pull'],
['shift-P', 'Object mode: Add a preview window in the D window'],
['P', 'UV Image Editor:  Pin selected vertices. Pinned vertices will stay in place on the UV editor when executing an LSCM unwrap.'],
['Alt-P', 'Clear parent relationship'],
['Alt-P', 'UV Image Editor: Unpin UVs'],
['Alt-P', 'Text Editor : Run current script '],
['Ctrl-P', 'Make active object parent of selected object'],
['Ctrl-Shift-P', 'Make active object parent of selected object without inverse'],
['Ctrl-P', 'Edit mode: Make active vertex parent of selected object'],
['Ctrl-P', 'ARMATURE : editmode, make bone parent.'],
['.', '...']
],

"Q":[['Ctrl-Q', 'Quit'],
     ['.', '...']
     ],

"R":[ 
['R', 'FileSelector : remove file'],	
['R', 'Rotate'],
['R', 'IPO: Record mouse movement as IPO curve'],
['R', 'UV Face Select: Rotate menu uv coords or vertex colour'],
['R', 'NODE window : read saved render result'], #243	
['R', 'SEQUENCER window : re-assign entries to another strip '], #243			
['RX', 'Rotate around X axis'],
['RXX', "Rotate around object's local X axis"],
['RY', 'Rotate around Y axis'],
['RYY', "Rotate around object's local Y axis"],
['RZ', 'Rotate around Z axis'],
['RZZ', "Rotate around object's local Z axis"],
['Alt-R', 'Clear object rotation'],
['Alt-R', 'Text editor : reopen text.'],
['Ctrl-R', 'Edit Mode: Knife, cut selected edges, accept left mouse/ cancel right mouse'],
['Ctrl-Alt-R', 'MANIPULATOR (transform widget): set in Rotate Mode'],
['Shift-R', 'Edit Mode: select Face Loop'],
['Shift-R', 'Nurbs: Select row'],
['.', '...']
],

"S":[ 
['S', 'Scale'] ,
['S', 'TimeLine: Set Start'],
['SX', 'Flip around X axis'] ,
['SY', 'Flip around Y axis'] ,
['SZ', 'Flip around Z axis'] ,
['SXX', 'Flip around X axis and show axis'] ,
['SYY', 'Flip around Y axis and show axis'] ,
['SZZ', 'Flip around Z axis and show axis'] ,
['Alt-S', 'Edit mode: Shrink/fatten (Scale along vertex normals)'] ,
['Alt-S', 'Text Editor : Save the current text to file '],
['Alt-S',' ARMATURE : posemode editmode: Scale envalope.'],
['Ctrl-Shift-S', 'Edit mode: To Sphere'] ,
['Ctrl-Alt-Shift-S', 'Edit mode: Shear'] ,
['Alt-S', 'Clear object size'] ,
['Ctrl-S', 'Edit mode: Shear'] ,
['Alt-Shift-S,','Text editor : Select the line '],
['Ctrl-Alt-G', 'MANIPULATOR (transform widget): set in Size Mode'],
['Shift-S', 'Cursor/Grid snap menu'],
['Shift-S+1', 'VIDEO SEQUENCE editor : jump to the current frame '],
['.', '...']
],

"T":[ 
['T', 'Adjust texture space'],
['T', 'Edit mode: Flip 3d curve'],
['T', 'IPO: Menu Change IPO type, 1 Constant'],
['T', 'IPO: Menu Change IPO type, 2 Linear'],
['T', 'IPO: Menu Change IPO type, 3 Bezier'],	
['T', 'TimeLine: Show second'],	
['T', 'VIDEO SEQUENCE editor : toggle between show second andd show frame'], #243	
['Alt-T', 'Clear tracking of object'],
['Ctrl-T', 'Make selected object track active object'],
['Ctrl-T', 'Edit Mode: Convert to triangles'],
['Ctrl-ALT-T', 'Benchmark'],
['.', '...']
],

"U":[ 
['U', 'Make single user menu (for import completly linked object to another scene  for instance) '] ,
['U', '3D View: Make Single user Menu'] ,
['U', 'UV Face Select: Automatic UV calculation menu'] ,
['U', 'Vertex-/Weightpaint mode: Undo'] ,
['Ctrl-U', 'Save current state as user default'],
['Shift-U', 'Edit Mode: Redo Menu'],
['Alt-U', 'Edit Mode & Object Mode: Undo Menu'],
['.', '...']
],

"V":[ 
['V', 'Curves/Nurbs: Vector handle'],
['V', 'Edit Mode : Rip selected vertices'],
['V', 'Vertexpaint mode'],
['V', 'UV Image Editor: Stitch UVs'],
['Ctrl-V',' UV Image Editor:  maximize stretch.'],
['V', 'Action editor: Vector'],
['Alt-V', "Scale object to match image texture's aspect ratio"],
['Alt-V', 'Text Editor : Paste '],
['Alt-Shift-V', 'Text Editor : View menu'],
['Alt-Shift-V', 'Text Editor : View menu 1, Top of the file '],
['Alt-Shift-V', 'Text Editor : View menu 2, Bottom of the file '],
['Alt-Shift-V', 'Text Editor : View menu 3, PageUp'],
['Alt-Shift-V', 'Text Editor : View menu 4, PageDown'],
['Ctrl-Shift-V', 'Text Editor: Paste from clipboard'],
['Shift-V', 'Edit mode: Align view to selected vertices'],
['Shift-V', 'UV Image Editor: Limited Stitch UVs popup'],
['.', '...']  
],

"W":[ 
['W', 'Edit Mode: Specials menu'],
['W', 'Edit Mode: Specials menu, ARMATURE 1 Subdivide'],
['W', 'Edit Mode: Specials menu, ARMATURE 2 Flip Left-Right Name'],
['W', 'Edit Mode: Specials menu, CURVE 1 Subdivide'],
['W', 'Edit Mode: Specials menu, CURVE 2 Swich Direction'],
['W', 'Edit Mode: Specials menu, MESH 1 Subdivide'],
['W', 'Edit Mode: Specials menu, MESH 2 Subdivide Multi'],
['W', 'Edit Mode: Specials menu, MESH 3 Subdivide Multi Fractal'],
['W', 'Edit Mode: Specials menu, MESH 4 Subdivide Smooth'],
['W', 'Edit Mode: Specials menu, MESH 5 Merge'],
['W', 'Edit Mode: Specials menu, MESH 6 Remove Double'],
['W', 'Edit Mode: Specials menu, MESH 7 Hide'],
['W', 'Edit Mode: Specials menu, MESH 8 Reveal'],
['W', 'Edit Mode: Specials menu, MESH 9 Select Swap'],
['W', 'Edit Mode: Specials menu, MESH 10 Flip Normal'],
['W', 'Edit Mode: Specials menu, MESH 11 Smooth'],
['W', 'Edit Mode: Specials menu, MESH 12 Bevel'],
['W', 'Edit Mode: Specials menu, MESH 13 Set Smooth'],
['W', 'Edit Mode : Specials menu, MESH 14 Set Solid'],
['W', 'Object Mode : on MESH objects, Boolean Tools menu'],
['W', 'Object Mode : on MESH objects, Boolean Tools 1 Intersect'],
['W', 'Object Mode : on MESH objects, Boolean Tools 2 union'],
['W', 'Object Mode : on MESH objects, Boolean Tools 3 difference'],
['W', 'Object Mode : on MESH objects, Boolean Tools 4 Add an intersect Modifier'],
['W', 'Object Mode : on MESH objects, Boolean Tools 5 Add an union Modifier'],
['W', 'Object Mode : on MESH objects, Boolean Tools 6 Add a difference Modifier'],
['W', 'Object mode : on TEXT object, Split characters, a new TEXT object by character in the selected string '],
['W', 'UV Image Editor: Weld/Align'],
['WX', 'UV Image Editor: Weld/Align X axis'],
['WY', 'UV Image Editor: Weld/Align Y axis'],
['Ctrl-W', 'Save current file'] ,
['Shift-W', 'Warp/bend selected vertices around cursor'],
['alt-W', 'Export in videoscape format'],
['.', '...']
 ],

"X":[ 
['X', 'Delete menu'] ,
['X', 'TimeLine : Remove marker'],
['X', 'NLA : Remove marker'],
['X', 'IPO : Remove marker'],	
['X', 'NODE window : delete'], #243		
['Alt-X', 'Text Editor : Cut '],
['Ctrl-X', 'Restore default state (Erase all)'],
['.', '...']
 ],

"Y":[ 
['Y', 'Edit Mode & Mesh : Split selected vertices/faces from the rest'],
['Ctrl-Y', 'Object Mode : Redo'],
['.', '...']
],

"Z":[ 
['Z', 'Render Window: 200% zoom from mouse position'],
['Z', 'Switch 3d draw type : solide/ wireframe (see also D)'],
['Alt-Z', 'Switch 3d draw type : solid / textured (see also D)'],
['Alt-Z,','Text editor : undo '],
['Ctrl-Z', 'Object Mode : Undo'],
['Ctrl-Z,','Text editor : undo '],
['Ctrl-Shift-Z,','Text editor : Redo '],
['Shift-Z', 'Switch 3d draw type : shaded / wireframe (see also D)'],
['.', '...']
]}]}

up=128
down=129
UP=0
SEARCH=131
OLDSEARCHLINE=''
SEARCHLINE=Create('')
LINE=130
FINDED=[]
LEN=0

for k in hotkeys.keys():
   hotkeys[k].append(Create(0))

for k in hotkeys['Letters '][0]:
   hotkeys['Letters '][0][k].append(Create(0))

hotL=hotkeys['Letters '][0].keys()
hotL.sort()

hot=hotkeys.keys()
hot.sort()

def searchfor(SEARCHLINE):
	global hotkeys, hot
	FINDLIST=[]
	for k in hot:
		if k not in ['Letters ', 'Search '] :
			for l in hotkeys[k][:-1]:
				#print 'k, l : ', k,  l, l[1] 
				if  l[1].upper().find(SEARCHLINE.upper())!=-1:
					FINDLIST.append(l)
					
		elif k == 'Letters ':
			for l in hotL :
				for l0 in hotkeys['Letters '][0][l][:-1]:
					#print 'k, l : ',l,  k,  l0
					if l0[1].upper().find(SEARCHLINE.upper())!=-1:
						FINDLIST.append(l0)
	#print 'FINDLIST',FINDLIST					
	FINDLIST.append(['Find list','Entry'])
	return FINDLIST			
			
	
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
    global r,c,c1,hotkeys, hot, hotL, up, down, UP, SEARCH, SEARCHLINE,LINE
    global OLDSEARCHLINE, FINDED, SCROLL, LEN
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

    BeginAlign()
    for i, k in enumerate(hot):
       hotkeys[k][-1]=Toggle(k, i+10, 78*i, size[3]-(47), 78, 24, hotkeys[k][-1].val )
       l+=len(k)
       if hotkeys[k][-1].val==1.0:
           listed= i
    EndAlign()
    l=0
    size[3]=size[3]-4
    
    if hot[listed]!='Letters ' and hot[listed]!='Search ' :
       size[3]=size[3]-8
       SCROLL=size[3]/21
       END=-1
       if SCROLL < len(hotkeys[hot[listed]][:-1]):
          BeginAlign()
          Button('/\\',up,4,size[3]+8,20,14,'Scroll up') 
          Button('\\/',down,4,size[3]-8,20,14,'Scroll down')            
          EndAlign()
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
          
    elif hot[listed]=='Search ' :
       r=[0,size[3]-70,
          size[2], size[3]-44]
       trace_rectangle4(r,c2)
       SEARCHLINE=String(' ', LINE, 42, size[3]-68,200,18,SEARCHLINE.val, 256,'')
       if len(FINDED)>0:
        LEN=len(FINDED)	   
        size[3]=size[3]-8
        SCROLL=size[3]/21
        END=-1
        
        if SCROLL < len(FINDED):
            BeginAlign()
            Button('/\\',up,4,size[3]+8,20,14,'Scroll up') 
            Button('\\/',down,4,size[3]-8,20,14,'Scroll down')            
            EndAlign()
            if (SCROLL+UP)<len(FINDED):
               END=(UP+SCROLL-1)
            else:
               END=-1
               #UP=len(FINDED)-SCROLL
        else:
            UP=0         
        for n in FINDED[UP:END]:
             if l%2==0:
                 r=[0,size[3]-(21*l+66+24),
                     size[2], size[3]-(21*l+43+24)]
                 trace_rectangle4(r,c2)
             glColor3f(0,0,0)
             glRasterPos2f(4+8, size[3]-(58+24+21*l))
             Text(n[0])
             glRasterPos2f(4+8*15, size[3]-(58+24+21*l))
             Text('  : '+n[1]) 
             l+=1
    else:
       BeginAlign()
       for pos, k in enumerate(hotL):
            hotkeys['Letters '][0][k][-1]=Toggle(k,pos+20,pos*21, size[3]-(52+18), 21, 18, hotkeys['Letters '][0][k][-1].val )
            if hotkeys['Letters '][0][k][-1].val==1.0:
               Llisted=pos
       EndAlign()
       size[3]=size[3]-8
       SCROLL=(size[3]-88)/21
       END=-1
       if SCROLL < len(hotkeys['Letters '][0][hotL[Llisted]]):
          LEN=len(hotkeys['Letters '][0][hotL[Llisted]])
          BeginAlign()
          Button('/\\',up,4,size[3]+8,20,14,'Scroll up, you can use arrow or page keys too ') 
          Button('\\/',down,4,size[3]-8,20,14,'Scroll down,  you can use arrow or page keys too ')            
          EndAlign()
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
    global hotkeys, UP,  SCROLL  , LEN   
    if (evt== QKEY or evt== ESCKEY): 
        Exit()
    elif val:
      if (evt== PAGEUPKEY):
         if (UP+SCROLL)<LEN-5: 
             UP+=5 
      elif (evt== PAGEDOWNKEY):
          if UP>4: 
             UP-=5 
      elif (evt== UPARROWKEY):
          if (UP+SCROLL)<LEN-1: 
             UP+=1
      elif (evt== DOWNARROWKEY):
          if UP>0: 
              UP-=1
      Redraw()

def bevent(evt):
    global hotkeysmhot, hotL, up,down,UP, FINDED
    global SEARCH, SEARCHLINE,LINE, OLDSEARCHLINE

    if   (evt== 1):
        Exit()

    elif 9 < evt < 20:
        for i, k in enumerate(hot):
           if i+10!=evt:
                 hotkeys[k][-1].val=0
                 UP=0 
        Blender.Window.Redraw()

    elif 19 < evt < 46:
        for i, k in enumerate(hotL):
           if i+20!=evt:
                 hotkeys['Letters '][0][k][-1].val=0
                 UP=0 
        Blender.Window.Redraw()

    elif (evt==up):
       UP+=1
       Blender.Window.Redraw()

    elif (evt==down):
       if UP>0: UP-=1
       Blender.Window.Redraw()

    elif (evt==LINE):
       if SEARCHLINE.val!='' and SEARCHLINE.val!=OLDSEARCHLINE:
          OLDSEARCHLINE=SEARCHLINE.val	
          FINDED=searchfor(OLDSEARCHLINE)
          Blender.Window.Redraw()

if __name__ == '__main__':
	Register(draw, event, bevent)
