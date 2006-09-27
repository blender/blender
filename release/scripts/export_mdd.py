#!BPY

"""
 Name: 'Save Mesh RVKs as MDD'
 Blender: 242
 Group: 'Animation'
 Tooltip: 'baked vertex animation fromo selected model.'
"""

__author__ = "Bill L.Nieuwendorp"
__bpydoc__ = """\
This script Exports Lightwaves MotionDesigner format.

The .mdd format has become quite a popular Pipeline format<br>
for moving animations from package to package.
"""
# mdd export  
#
# 
#
# Warning if the vertex order or vertex count differs from frame to frame
# The script will fail because the resulting file would be an invalid mdd file.
# 
# mdd files should only be applied to the the origonating model with the origonal vert order
#
#Please send any fixes,updates,bugs to Slow67_at_Gmail.com
#Bill Niewuendorp

import Blender
from Blender import *
import BPyMessages
try:
	from struct import pack
except:
	pack = None

def mdd_export(filepath, ob, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS):
	
	Window.EditMode(0)
	Blender.Window.WaitCursor(1)
	mesh_orig = ob.getData(mesh=1)

	#Flip y and z matrix
	mat_flip= Mathutils.Matrix(\
	[1,0,0,0],\
	[0,0,1,0],\
	[0,-1,0,0],\
	[0,0,0,1],\
	)
	
	me_tmp = Mesh.New() # container mesh

	numverts = len(mesh_orig.verts)
	numframes = PREF_ENDFRAME-PREF_STARTFRAME+1
	PREF_FPS= float(PREF_FPS)
	f = open(filepath, 'wb') #no Errors yet:Safe to create file
	
	# Write the header
	f.write(pack(">2i", numframes-1, numverts))
	
	# Write the frame times (should we use the time IPO??)
	f.write( pack(">%df" % (numframes-1), *[frame/PREF_FPS for frame in xrange(numframes-1)]) ) # seconds
	
	Blender.Set('curframe', PREF_STARTFRAME)
	for frame in xrange(numframes+1):
		Blender.Set('curframe', frame)
		# Blender.Window.RedrawAll() # not needed
		me_tmp.getFromObject(ob.name)
		
		if len(me_tmp.verts) != numverts:
			Blender.Draw.PupMenu('Error%t|Number of verts has changed during animation|cannot export')
			Blender.Window.WaitCursor(0)
			f.close() # should we zero?
			return
		
		me_tmp.transform(ob.matrixWorld * mat_flip)
		
		# Write the vertex data
		f.write(pack(">%df" % (numverts*3), *[axis for v in me_tmp.verts for axis in v.co]))
	
	me_tmp.verts= None
	f.close()
	
	print'MDD Exported: %s frames:%d\n'% (filepath, numframes-1)  
	Blender.Window.WaitCursor(0)


def mdd_export_ui(filepath):
	# Dont overwrite
	if not BPyMessages.Warning_SaveOver(filepath):
		return
	
	scn= Scene.GetCurrent()
	ob_act= scn.objects.active
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
	
	ctx = scn.getRenderingContext()
	orig_frame = Blender.Get('curframe')
	PREF_STARTFRAME= Blender.Draw.Create(ctx.startFrame())
	PREF_ENDFRAME= Blender.Draw.Create(ctx.endFrame())
	PREF_FPS= Blender.Draw.Create(ctx.fps)

	block = [\
	("Start Frame: ", PREF_STARTFRAME, 1, 30000, "Start Bake from what frame?: Default 1"),\
	("End Frame: ", PREF_ENDFRAME, 1, 30000, "End Bake on what Frame?"),\
	("FPS: ", PREF_FPS, 1, 100, "Frames per second")\
	]
	
	PREF_STARTFRAME, PREF_ENDFRAME=\
		min(PREF_STARTFRAME.val, PREF_ENDFRAME.val),\
		max(PREF_STARTFRAME.val, PREF_ENDFRAME.val)
	
	if not Blender.Draw.PupBlock("Export MDD", block):
		return
	
	print (filepath, ob_act, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS.val)
	mdd_export(filepath, ob_act, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS.val)
	Blender.Set('curframe', orig_frame)
	
if __name__=='__main__':
	if not pack:
		Draw.PupMenu('Error%t|This script requires a full python install')
	
	Blender.Window.FileSelector(mdd_export_ui, 'EXPORT MDD', sys.makename(ext='.mdd'))