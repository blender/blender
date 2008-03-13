#!BPY

"""
 Name: 'Load MDD to Mesh RVKs'
 Blender: 242
 Group: 'Import'
 Tooltip: 'baked vertex animation to active mesh object.'
"""
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

# ***** BEGIN GPL LICENSE BLOCK *****
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




try:
	from struct import unpack
except:
	unpack = None

import Blender
from Blender import Mesh, Object, Scene
import BPyMessages

def mdd_import(filepath, ob, PREF_IPONAME, PREF_START_FRAME, PREF_JUMP):
	
	print '\n\nimporting mdd "%s"' % filepath
	
	Blender.Window.DrawProgressBar (0.0, "Importing mdd ...")
	Blender.Window.EditMode(0)
	Blender.Window.WaitCursor(1)
	
	file = open(filepath, 'rb')
	frames, points = unpack(">2i", file.read(8))
	time = unpack((">%df" % frames), file.read(frames * 4))
	
	print '\tpoints:%d frames:%d' % (points,frames)

	scn = Scene.GetCurrent()
	ctx = scn.getRenderingContext()
	Blender.Set("curframe", PREF_START_FRAME)
	me = ob.getData(mesh=1)
	
	def UpdateMesh(me,fr):
		for v in me.verts:
			# 12 is the size of 3 floats
			x,y,z= unpack('>3f', file.read(12))
			v.co[:] = x,z,y
		me.update()
	
	Blender.Window.DrawProgressBar (0.4, "4 Importing mdd ...")
	
	
	curfr = ctx.currentFrame()
	print'\twriting mdd data...'
	for i in xrange(frames):
		Blender.Set("curframe", i+PREF_START_FRAME)
		if len(me.verts) > 1 and (curfr >= PREF_START_FRAME) and (curfr <= PREF_START_FRAME+frames):
			UpdateMesh(me, i)
			ob.insertShapeKey()
	
	Blender.Window.DrawProgressBar (0.5, "5 Importing mdd ...")
	
	key= me.key
	
	# Add the key of its not there
	if not key:
		me.insertKey(1, 'relative')
		key= me.key
	
	key.ipo = Blender.Ipo.New('Key', PREF_IPONAME)
	ipo = key.ipo
	# block = key.getBlocks() # not used.
	all_keys = ipo.curveConsts

	for i in xrange(PREF_JUMP+1, len(all_keys), PREF_JUMP):
		curve = ipo.getCurve(i)
		if curve == None:
			curve = ipo.addCurve(all_keys[i])
		
		curve.append((PREF_START_FRAME+i-1,1))
		curve.append((PREF_START_FRAME+i- PREF_JUMP -1,0))
		curve.append((PREF_START_FRAME+i+ PREF_JUMP-1,0))
		curve.setInterpolation('Linear')
		curve.recalc()
	
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
	if not unpack:
		Draw.PupMenu('Error%t|This script requires a full python install')
	
	Blender.Window.FileSelector(mdd_import_ui, 'IMPORT MDD', '*.mdd')
