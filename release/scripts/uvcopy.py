#!BPY
""" Registration info for Blender menus: <- these words are ignored
Name: 'UVcopy'
Blender: 242
Group: 'Object'
Tip: 'Copy UV coords from a mesh to another that has same vertex indices'
"""

__author__ = "Toni Alatalo, Martin Poirier et. al."
__url__ = ("blender", "elysiun",
"Script's homepage, http://www.elysiun.com/forum/viewtopic.php?t=14897", 
"Communicate problems and errors, http://www.elysiun.com/forum/viewtopic.php?t=14897")
__version__ = "0.2 01/2006"

__bpydoc__ = """\
This script copies UV coords from a mesh to another (version of the same mesh).
"""

import Blender
 
scene = Blender.Scene.GetCurrent()

unwrapped = scene.getActiveObject()
targets = Blender.Object.GetSelected()
 
if unwrapped: 
    source = unwrapped.data
else: 
    raise RuntimeError, "No active object to copy UVs from." 
 
if targets:
    try:
        targets.remove(unwrapped)
    except ValueError:
        print "ob for sourcedata was not in targets, so did not need to remove", unwrapped, targets
    #try:
    #    target = targets[0].data
    #except IndexError:
    if not targets:
        raise RuntimeError, "no selected object other than the source, hence no target defined."
else: 
    raise RuntimeError, "No selected object(s) to copy UVs to."

if source and targets:
    for target in targets:
        target = target.data
        for i in range(len(target.faces)): 
            target.faces[i].uv = source.faces[i].uv
            #print "copied to target:", target.name, target.data.faces[i].uv, ", source being:", source.faces[i].uv
        target.update() 
        #print "Copied UV from object " + unwrapped.name + " to object(s) " + target.name + "."
