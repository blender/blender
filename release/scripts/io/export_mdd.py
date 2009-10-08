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
import Mathutils
import math
import os

#import Blender
#from Blender import *
#import BPyMessages
try:
	from struct import pack
except:
	pack = None

def zero_file(filepath):
	'''
	If a file fails, this replaces it with 1 char, better not remove it?
	'''
	file = open(filepath, 'w')
	file.write('\n') # apparently macosx needs some data in a blank file?
	file.close()

def check_vertcount(mesh,vertcount):
	'''
	check and make sure the vertcount is consistent throughout the frame range
	'''
	if len(mesh.verts) != vertcount:
		raise Exception('Error, number of verts has changed during animation, cannot export')
		f.close()
		zero_file(filepath)
		return
	
	
def write(filename, sce, ob, PREF_STARTFRAME, PREF_ENDFRAME, PREF_FPS):
	if not pack:
		raise Exception('Error, this script requires the "pack" module')
	
	if ob.type != 'MESH':
		raise Exception('Error, active object is not a mesh')
	"""
	Window.EditMode(0)
	Blender.Window.WaitCursor(1)

	mesh_orig = Mesh.New()
	mesh_orig.getFromObject(ob.name)
	"""
	orig_frame = sce.current_frame
	sce.set_frame(PREF_STARTFRAME)
	me = ob.create_mesh(True, 'PREVIEW')

	#Flip y and z
	mat_flip= Mathutils.Matrix(\
	[1.0, 0.0, 0.0, 0.0],\
	[0.0, 0.0, 1.0, 0.0],\
	[0.0, 1.0, 0.0, 0.0],\
	[0.0, 0.0, 0.0, 1.0],\
	)

	numverts = len(me.verts)

	numframes = PREF_ENDFRAME-PREF_STARTFRAME+1
	PREF_FPS= float(PREF_FPS)
	f = open(filename, 'wb') #no Errors yet:Safe to create file
	
	# Write the header
	f.write(pack(">2i", numframes, numverts))
	
	# Write the frame times (should we use the time IPO??)
	f.write( pack(">%df" % (numframes), *[frame/PREF_FPS for frame in range(numframes)]) ) # seconds
	
	#rest frame needed to keep frames in sync
	"""
	Blender.Set('curframe', PREF_STARTFRAME)
	me_tmp.getFromObject(ob.name)
	"""

	check_vertcount(me,numverts)
	me.transform(mat_flip * ob.matrix)
	f.write(pack(">%df" % (numverts*3), *[axis for v in me.verts for axis in v.co]))
		
	for frame in range(PREF_STARTFRAME,PREF_ENDFRAME+1):#in order to start at desired frame
		"""
		Blender.Set('curframe', frame)
		me_tmp.getFromObject(ob.name)
		"""

		sce.set_frame(frame)
		me = ob.create_mesh(True, 'PREVIEW')
		check_vertcount(me,numverts)
		me.transform(mat_flip * ob.matrix)
		
		# Write the vertex data
		f.write(pack(">%df" % (numverts*3), *[axis for v in me.verts for axis in v.co]))
	
	"""
	me_tmp.verts= None
	"""
	f.close()
	
	print ('MDD Exported: %s frames:%d\n'% (filename, numframes-1))
	"""
	Blender.Window.WaitCursor(0)
	Blender.Set('curframe', orig_frame)
	"""
	sce.set_frame(orig_frame)

class EXPORT_OT_mdd(bpy.types.Operator):
	'''Animated mesh to MDD vertex keyframe file.'''
	__idname__ = "export.mdd"
	__label__ = "Export MDD"

	# get first scene to get min and max properties for frames, fps

	sce = bpy.data.scenes[bpy.data.scenes.keys()[0]]
	minframe = sce.rna_type.properties["current_frame"].soft_min
	maxframe = sce.rna_type.properties["current_frame"].soft_max
	minfps = sce.render_data.rna_type.properties["fps"].soft_min
	maxfps = sce.render_data.rna_type.properties["fps"].soft_max

	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	__props__ = [
		bpy.props.StringProperty(attr="path", name="File Path", description="File path used for exporting the MDD file", maxlen= 1024, default= "tmp.mdd"),
		bpy.props.IntProperty(attr="fps", name="Frames Per Second", description="Number of frames/second", min=minfps, max=maxfps, default= 25),
		bpy.props.IntProperty(attr="start_frame", name="Start Frame", description="Start frame for baking", min=minframe,max=maxframe,default=1),
		bpy.props.IntProperty(attr="end_frame", name="End Frame", description="End frame for baking", min=minframe, max=maxframe, default= 250),
	]

	def poll(self, context):
		return context.active_object != None

	def execute(self, context):
		if not self.path:
			raise Exception("filename not set")
		write(self.path, context.scene, context.active_object,
			self.start_frame, self.end_frame, self.fps )
		return ('FINISHED',)
	
	def invoke(self, context, event):	
		wm = context.manager
		wm.add_fileselect(self.__operator__)
		return ('RUNNING_MODAL',)

bpy.ops.add(EXPORT_OT_mdd)

if __name__=='__main__':
	#if not pack:
#		Draw.PupMenu('Error%t|This script requires a full python install')
	#Blender.Window.FileSelector(mdd_export_ui, 'EXPORT MDD', sys.makename(ext='.mdd'))
	bpy.ops.EXPORT_OT_mdd(path="/tmp/test.mdd")

