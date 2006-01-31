#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'UVcopy'
Blender: 242
Group: 'UV'
Tip: 'Copy UV coords from a mesh to another that has same vertex indices'
"""

__author__ = "Martin Poirier, Toni Alatalo et. al."
__url__ = ("blender", "elysiun",
"Script's homepage, http://www.elysiun.com/forum/viewtopic.php?t=14897", 
"Communicate problems and errors, http://www.elysiun.com/forum/viewtopic.php?t=14897")
__version__ = "0.1 01/2006"

__bpydoc__ = """\
This script copies UV coords from a mesh to another (version of the same mesh).
"""

import Blender
 
Name_From = "Unwrapped" #XXX active and 1st selected object, or what? two first selected?
Name_To   = "Original" 
 
me1 = Blender.Object.Get(Name_To) 
me2 = Blender.Object.Get(Name_From) 
 
if me1: 
    me1 = me1.getData() 
else: 
    print "No object named "+Name_To+"." 
 
if me2: 
    me2 = me2.getData() 
else: 
    print "No object named "+Name_From+"." 
 
if me1 and me2: 
    for i in range(len(me1.faces)): 
        me1.faces[i].uv = me2.faces[i].uv 
    me1.update() 
    print "Copied UV from object "+Name_From+" to object "+Name_To+"."
