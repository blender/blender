#!BPY

 #"""
 #Name: 'Load MDD to Mesh RVKs'
 #Blender: 242
 #Group: 'Animation'
 #Tooltip: 'baked vertex animation to active mesh object.'
 #"""
__author__ = "Bill L.Nieuwendorp"
__bpydoc__ = """\
This script Imports Lightwaves MotionDesigner format.

The .mdd format has become quite a popular Pipeline format<br>
for moving animations from package to package.
"""
# mdd importer  
#
# Warning if the vertex order or vertex count differs from the
# origonal model the mdd was Baked out from their will be Strange
# behavior
# 
#
#vertex animation to ShapeKeys with ipo  and gives the frame a value of 1.0 
#A modifier to read mdd files would be Ideal but thats for another day :)
#
#Please send any fixes,updates,bugs to Slow67_at_Gmail.com
#Bill Niewuendorp

try:
	import struct
except:
	struct= None

import Blender
from Blender import Mesh, Object, Scene
import BPyMessages

def mdd_import(filepath, ob, PREF_IPONAME, PREF_START_FRAME, PREF_JUMP):
	
	print '\n\nimporting mdd "%s"' % filepath
	
	Blender.Window.DrawProgressBar (0.0, "Importing mdd ...")
	Blender.Window.EditMode(0)
	Blender.Window.WaitCursor(1)
	
	file = open(filepath, 'rb')
	toUnpack = file.read(8)
	frames, points = struct.unpack(">2i",toUnpack)
	floatBytes = frames * 4
	furtherUnpack = file.read(floatBytes)
	floatBytes2 = points * 12
	floatBytes3 = 12 * points * frames
	restsize = points * 3
	pfsize = points * frames * 3
	restPoseUnpack = file.read(floatBytes2)
	PerFrameUnpack = file.read(floatBytes3)
	pattern = ">%df" % frames
	pattern2 = ">%df" % restsize
	pattern3 = ">%df" % pfsize
	time = struct.unpack(pattern, furtherUnpack)
	rest_pose = struct.unpack(pattern2, restPoseUnpack)
	PerFramexyz = struct.unpack(pattern3, PerFrameUnpack)
	
	print '\tpoints:%d frames:%d' % (points,frames)

	scn = Scene.GetCurrent()
	ctx = scn.getRenderingContext()
	#ctx.startFrame(PREF_START_FRAME)
	#ctx.endFrame(PREF_START_FRAME+frames)
	Blender.Set("curframe", PREF_START_FRAME)
	me = ob.getData(mesh=1)
	xyzs = PerFramexyz
	Point_list = []
	for i in xrange(len(xyzs)/3):
		xpos, zpos = i*3, (i*3)+3
		Point_list.append(xyzs[xpos:zpos])
		Frm_points = []
	Blender.Window.DrawProgressBar (0.2, "3 Importing mdd ...")
	for i in xrange(len(Point_list)):
		first, last = i*points, (i*points)+points
		Frm_points.append(Point_list[first:last])			
	
	
	def UpdateMesh(me,fr):
		for vidx, v in enumerate(me.verts):
			v.co[:] = Frm_points[fr][vidx][0], Frm_points[fr][vidx][2], Frm_points[fr][vidx][1]
		me.update()

	Blender.Window.DrawProgressBar (0.4, "4 Importing mdd ...")
	
	
	curfr = ctx.currentFrame()
	print'\twriting mdd data...'
	for i in xrange(frames):
		Blender.Set("curframe", i+PREF_START_FRAME)
		if len(me.verts) > 1 and (curfr >= PREF_START_FRAME) and (curfr <= PREF_START_FRAME+frames):
			UpdateMesh(me, i)
			ob.insertShapeKey()
			ob.makeDisplayList()
			Blender.Window.RedrawAll() 
	
	Blender.Window.DrawProgressBar (0.5, "5 Importing mdd ...")
	
	key= me.key
	
	# Add the key of its not there
	if not key:
		me.insertKey(1, 'relative')
		key= me.key
	
	key.ipo = Blender.Ipo.New("Key", PREF_IPONAME)
	ipo = key.ipo
	block = key.getBlocks()
	all_keys = ipo.curveConsts

	for i in xrange(PREF_JUMP, len(all_keys), PREF_JUMP):
			curve = ipo.getCurve(i)
			if curve == None:
				ipo.addCurve(all_keys[i])

	Blender.Window.DrawProgressBar (0.8, "appending to ipos")
	for i in xrange(PREF_JUMP, len(all_keys), PREF_JUMP):# Key Reduction 
		mkpoints = ipo.getCurve(i)
		mkpoints.append((1+PREF_START_FRAME+i-1,1))
		mkpoints.append((1+PREF_START_FRAME+i- PREF_JUMP -1,0))
		mkpoints.append((1+PREF_START_FRAME+i+ PREF_JUMP-1,0))
		mkpoints.setInterpolation('Linear')
		mkpoints.recalc()
	
	print 'done'
	Blender.Window.WaitCursor(0)
	Blender.Window.DrawProgressBar (1.0, '')


def mdd_import_ui(filepath):
	
	if BPyMessages.Error_NoFile(filepath):
		return
		
	scn= Scene.GetCurrent()
	ob_act= scn.objects.active
	
	if ob_act == None or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return
	
	PREF_IPONAME = Blender.Draw.Create(filepath.split('/')[-1].split('\\')[-1].split('.')[0])
	PREF_START_FRAME = Blender.Draw.Create(1)
	PREF_JUMP = Blender.Draw.Create(1)
	
	block = [\
	("Ipo Name: ", PREF_IPONAME, 0, 30, "Ipo name for the new shape key"),\
	("Start Frame: ", PREF_START_FRAME, 1, 3000, "Start frame for the animation"),\
	("Key Skip: ", PREF_JUMP, 1, 100, "KeyReduction, Skip every Nth Frame")\
	]

	if not Blender.Draw.PupBlock("Import MDD", block):
		return
	orig_frame = Blender.Get('curframe')
	mdd_import(filepath, ob_act, PREF_IPONAME.val, PREF_START_FRAME.val, PREF_JUMP.val)
	Blender.Set('curframe', orig_frame)

if __name__ == '__main__':
	if not struct:
		Draw.PupMenu('Error%t|This script requires a full python install')
	
	Blender.Window.FileSelector(mdd_import_ui, 'IMPORT MDD', '*.mdd')