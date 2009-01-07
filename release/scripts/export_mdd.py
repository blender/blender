#!BPY

"""
 Name: 'Vertex Keyframe Animation (.mdd)...'
 Blender: 242
 Group: 'Export'
 Tooltip: 'Animated mesh to MDD vertex keyframe file.'
"""

__author__ = "Bill L.Nieuwendorp"
__bpydoc__ = """\
This script Exports Lightwaves MotionDesigner format.

The .mdd format has become quite a popular Pipeline format<br>
for moving animations from package to package.

Be sure not to use modifiers that change the number or order of verts in the mesh
"""
#Please send any fixes,updates,bugs to Slow67_at_Gmail.com or cbarton_at_metavr.com
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

import bpy
import Blender
from Blender import *
import BPyMessages
try:
	from struct import pack
except:
	pack = None

	
def zero_file(filepath):
	'''
	If a file fails, this replaces it with 1 char, better not remove it?
	'''
	file = open(filepath, 'w')
	file.write('\n') # aparently macosx needs some data in a blank file?
	file.close()


def check_vertcount(mesh,vertcount):
	'''
	check and make sure the vertcount is consistent throghout the frame range
	'''
	if len(mesh.verts) != vertcount:
		Blender.Draw.PupMenu('Error%t|Number of verts has changed during animation|cannot export')
		f.close()
		zero_file(filepath)
		return
	
	
def mdd_export(filepath, ob, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS):
	
	Window.EditMode(0)
	Blender.Window.WaitCursor(1)
	mesh_orig = Mesh.New()
	mesh_orig.getFromObject(ob.name)
	
	#Flip y and z
	'''
	mat = Mathutils.Matrix()
	mat[2][2] = -1
	rotmat = Mathutils.RotationMatrix(90, 4, 'x')
	mat_flip = mat*rotmat
	'''
	# Above results in this matrix
	mat_flip= Mathutils.Matrix(\
	[1.0, 0.0, 0.0, 0.0],\
	[0.0, 0.0, 1.0, 0.0],\
	[0.0, 1.0, 0.0, 0.0],\
	[0.0, 0.0, 0.0, 1.0],\
	)
	
	me_tmp = Mesh.New() # container mesh

	numverts = len(mesh_orig.verts)
	numframes = PREF_ENDFRAME-PREF_STARTFRAME+1
	PREF_FPS= float(PREF_FPS)
	f = open(filepath, 'wb') #no Errors yet:Safe to create file
	
	# Write the header
	f.write(pack(">2i", numframes, numverts))
	
	# Write the frame times (should we use the time IPO??)
	f.write( pack(">%df" % (numframes), *[frame/PREF_FPS for frame in xrange(numframes)]) ) # seconds
	
	#rest frame needed to keep frames in sync
	Blender.Set('curframe', PREF_STARTFRAME)
	me_tmp.getFromObject(ob.name)
	check_vertcount(me_tmp,numverts)
	me_tmp.transform(ob.matrixWorld * mat_flip)
	f.write(pack(">%df" % (numverts*3), *[axis for v in me_tmp.verts for axis in v.co]))
	me_tmp.verts= None
		
	for frame in xrange(PREF_STARTFRAME,PREF_ENDFRAME+1):#in order to start at desired frame
		Blender.Set('curframe', frame)
		
		me_tmp.getFromObject(ob.name)
		
		check_vertcount(me_tmp,numverts)
		
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
	
	scn= bpy.data.scenes.active
	ob_act= scn.objects.active
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
	
	ctx = scn.getRenderingContext()
	orig_frame = Blender.Get('curframe')
	PREF_STARTFRAME= Blender.Draw.Create(int(ctx.startFrame()))
	PREF_ENDFRAME= Blender.Draw.Create(int(ctx.endFrame()))
	PREF_FPS= Blender.Draw.Create(ctx.fps)

	block = [\
	("Start Frame: ", PREF_STARTFRAME, 1, 30000, "Start Bake from what frame?: Default 1"),\
	("End Frame: ", PREF_ENDFRAME, 1, 30000, "End Bake on what Frame?"),\
	("FPS: ", PREF_FPS, 1, 100, "Frames per second")\
	]
	
	if not Blender.Draw.PupBlock("Export MDD", block):
		return
	
	PREF_STARTFRAME, PREF_ENDFRAME=\
		min(PREF_STARTFRAME.val, PREF_ENDFRAME.val),\
		max(PREF_STARTFRAME.val, PREF_ENDFRAME.val)
	
	print (filepath, ob_act, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS.val)
	mdd_export(filepath, ob_act, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS.val)
	Blender.Set('curframe', orig_frame)
	
if __name__=='__main__':
	if not pack:
		Draw.PupMenu('Error%t|This script requires a full python install')
	
	Blender.Window.FileSelector(mdd_export_ui, 'EXPORT MDD', sys.makename(ext='.mdd'))