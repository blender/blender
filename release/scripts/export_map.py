#!BPY

"""
Name: 'Quake 3 (.map)'
Blender: 249
Group: 'Export'
Tooltip: 'Export to Quake map format'
"""

__author__ = 'Campbell Barton'
__version__ = '0.1a'
__email__ = "ideasman42@gmail.com"
__bpydoc__ = """\
This script Exports a Quake 3 map format.

 Supports meshes, lights and nurbs patch surfaces
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C): Campbell Barton
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




from Blender import *
import BPyMesh

PREF_SCALE= Draw.Create(100)
PREF_FACE_THICK= Draw.Create(0.1)
PREF_GRID_SNAP= Draw.Create(0)
# Quake 1/2?
# PREF_DEF_TEX_OPTS= Draw.Create(' 0 0 0 1 1\n') # not user settable yet
# Quake 3+?
PREF_DEF_TEX_OPTS= Draw.Create(' 0 0 0 1 1 0 0 0\n') # not user settable yet

PREF_NULL_TEX= Draw.Create('NULL') # not user settable yet
PREF_INVIS_TEX= Draw.Create('common/caulk')

def write_cube2brush(file, faces):
	'''
	Takes 6 faces and writes a brush,
	these faces can be from 1 mesh, 1 cube within a mesh of larger cubes
	Faces could even come from different meshes or be contrived.
	'''
	# comment only
	# file.write('// brush "%s", "%s"\n' % (ob.name, ob.getData(name_only=1)))
	file.write('// brush from cube\n{\n')

	if PREF_GRID_SNAP.val:		format_vec= '( %d %d %d ) '
	else:						format_vec= '( %.8f %.8f %.8f ) '

	for f in faces:
		# from 4 verts this gets them in reversed order and only 3 of them
		# 0,1,2,3 -> 2,1,0
		for v in f.v[2::-1]:
			file.write(format_vec % tuple(v.co) )	
		
		try:	mode= f.mode
		except:	mode= 0
		
		if mode & Mesh.FaceModes.INVISIBLE:
			file.write(PREF_INVIS_TEX.val)
		else:
			try:	image= f.image
			except:	image= None
			
			if image:	file.write(sys.splitext(sys.basename(image.filename))[0])
			else:		file.write(PREF_NULL_TEX.val)
			
		# Texture stuff ignored for now
		file.write(PREF_DEF_TEX_OPTS.val)
	file.write('}\n')


def round_vec(v):
	if PREF_GRID_SNAP.val:
		return round(v.x), round(v.y), round(v.z)
	else:
		return tuple(v)

def write_face2brush(file, face):
	'''
	takes a face and writes it as a brush
	each face is a cube/brush
	'''
	
	if PREF_GRID_SNAP.val:		format_vec= '( %d %d %d ) '
	else:						format_vec= '( %.8f %.8f %.8f ) '
	
	
	image_text= PREF_NULL_TEX.val
	
	try:	mode= face.mode
	except:	mode= 0
	
	if mode & Mesh.FaceModes.INVISIBLE:
		image_text= PREF_INVIS_TEX.val
	else:
		try:	image= face.image
		except:	image= None
		if image:	image_text = sys.splitext(sys.basename(image.filename))[0]
	
	# original verts as tuples for writing
	orig_vco= [tuple(v.co) for v in face]
	
	# new verts that give the face a thickness
	dist= PREF_SCALE.val * PREF_FACE_THICK.val
	new_vco= [round_vec(v.co - (v.no * dist)) for v in face]
	#new_vco= [round_vec(v.co - (face.no * dist)) for v in face]
	
	file.write('// brush from face\n{\n')
	# front
	for co in orig_vco[2::-1]:
		file.write(format_vec % co )
	file.write(image_text)
	# Texture stuff ignored for now
	file.write(PREF_DEF_TEX_OPTS.val)
	
	
	for co in new_vco[:3]:
		file.write(format_vec % co )
	if mode & Mesh.FaceModes.TWOSIDE:
		file.write(image_text)
	else:
		file.write(PREF_INVIS_TEX.val)
	
	# Texture stuff ignored for now
	file.write(PREF_DEF_TEX_OPTS.val)
	
	# sides.
	if len(orig_vco)==3: # Tri, it seemms tri brushes are supported.
		index_pairs= ((0,1), (1,2), (2,0))
	else:
		index_pairs= ((0,1), (1,2), (2,3), (3,0))
	
	for i1, i2 in index_pairs:
		for co in orig_vco[i1], orig_vco[i2], new_vco[i2]:
			file.write( format_vec %  co )
		file.write(PREF_INVIS_TEX.val)
		file.write(PREF_DEF_TEX_OPTS.val)

	file.write('}\n')

def is_cube_facegroup(faces):
	'''
	Returens a bool, true if the faces make up a cube
	'''
	# cube must have 6 faces
	if len(faces) != 6:
		print '1'
		return False
	
	# Check for quads and that there are 6 unique verts
	verts= {}
	for f in faces:
		if len(f)!= 4:
			return False
		
		for v in f:
			verts[v.index]= 0
	
	if len(verts) != 8:
		return False
	
	# Now check that each vert has 3 face users
	for f in faces:
		for v in f:
			verts[v.index] += 1
	
	for v in verts.itervalues():
		if v != 3: # vert has 3 users?
			return False
	
	# Could we check for 12 unique edges??, probably not needed.
	return True
	
def is_tricyl_facegroup(faces):
	'''
	is the face group a tri cylinder
	Returens a bool, true if the faces make an extruded tri solid
	'''
	
	# cube must have 5 faces
	if len(faces) != 5:
		print '1'
		return False
	
	# Check for quads and that there are 6 unique verts
	verts= {}
	tottri= 0
	for f in faces:
		if len(f)== 3:
			tottri+=1
		
		for v in f:
			verts[v.index]= 0
	
	if len(verts) != 6 or tottri != 2:
		return False
	
	# Now check that each vert has 3 face users
	for f in faces:
		for v in f:
			verts[v.index] += 1
	
	for v in verts.itervalues():
		if v != 3: # vert has 3 users?
			return False
	
	# Could we check for 12 unique edges??, probably not needed.
	return True

def write_node_map(file, ob):
	'''
	Writes the properties of an object (empty in this case)
	as a MAP node as long as it has the property name - classname
	returns True/False based on weather a node was written
	'''
	props= [(p.name, p.data) for p in ob.game_properties]
	
	IS_MAP_NODE= False
	for name, value in props:
		if name=='classname':
			IS_MAP_NODE= True
			break
		
	if not IS_MAP_NODE:
		return False
	
	# Write a node
	file.write('{\n')
	for name_value in props:
		file.write('"%s" "%s"\n' % name_value)
	if PREF_GRID_SNAP.val:
		file.write('"origin" "%d %d %d"\n' % tuple([round(axis*PREF_SCALE.val) for axis in ob.getLocation('worldspace')]) )
	else:
		file.write('"origin" "%.6f %.6f %.6f"\n' % tuple([axis*PREF_SCALE.val for axis in ob.getLocation('worldspace')]) )
	file.write('}\n')
	return True


def export_map(filepath):
	
	pup_block = [\
	('Scale:', PREF_SCALE, 1, 1000, 'Scale the blender scene by this value.'),\
	('Face Width:', PREF_FACE_THICK, 0.01, 10, 'Thickness of faces exported as brushes.'),\
	('Grid Snap', PREF_GRID_SNAP, 'snaps floating point values to whole numbers.'),\
	'Null Texture',\
	('', PREF_NULL_TEX, 1, 128, 'Export textureless faces with this texture'),\
	'Unseen Texture',\
	('', PREF_INVIS_TEX, 1, 128, 'Export invisible faces with this texture'),\
	]
	
	if not Draw.PupBlock('map export', pup_block):
		return
	
	Window.WaitCursor(1)
	time= sys.time()
	print 'Map Exporter 0.0'
	file= open(filepath, 'w')
	
	
	obs_mesh= []
	obs_lamp= []
	obs_surf= []
	obs_empty= []
	
	SCALE_MAT= Mathutils.Matrix()
	SCALE_MAT[0][0]= SCALE_MAT[1][1]= SCALE_MAT[2][2]= PREF_SCALE.val
	
	dummy_mesh= Mesh.New()
	
	TOTBRUSH= TOTLAMP= TOTNODE= 0
	
	for ob in Object.GetSelected():
		type= ob.type
		if type == 'Mesh':		obs_mesh.append(ob)
		elif type == 'Surf':	obs_surf.append(ob)
		elif type == 'Lamp':	obs_lamp.append(ob)
		elif type == 'Empty':	obs_empty.append(ob)
	
	if obs_mesh or obs_surf:
		# brushes and surf's must be under worldspan
		file.write('\n// entity 0\n')
		file.write('{\n')
		file.write('"classname" "worldspawn"\n')
	
	
	print '\twriting cubes from meshes'
	for ob in obs_mesh:
		dummy_mesh.getFromObject(ob.name)
		
		#print len(mesh_split2connected(dummy_mesh))
		
		# Is the object 1 cube? - object-is-a-brush
		dummy_mesh.transform(ob.matrixWorld*SCALE_MAT) # 1 to tx the normals also
		
		if PREF_GRID_SNAP.val:
			for v in dummy_mesh.verts:
				co= v.co
				co.x= round(co.x)
				co.y= round(co.y)
				co.z= round(co.z)
		
		# High quality normals
		BPyMesh.meshCalcNormals(dummy_mesh)
		
		# Split mesh into connected regions
		for face_group in BPyMesh.mesh2linkedFaces(dummy_mesh):
			if is_cube_facegroup(face_group):
				write_cube2brush(file, face_group)
				TOTBRUSH+=1
			elif is_tricyl_facegroup(face_group):
				write_cube2brush(file, face_group)
				TOTBRUSH+=1
			else:
				for f in face_group:
					write_face2brush(file, f)
					TOTBRUSH+=1
			
			#print 'warning, not exporting "%s" it is not a cube' % ob.name
			
	
	dummy_mesh.verts= None
	

	valid_dims= 3,5,7,9,11,13,15
	for ob in obs_surf:
		'''
		Surf, patches
		'''
		surf_name= ob.getData(name_only=1)
		data= Curve.Get(surf_name)
		mat = ob.matrixWorld*SCALE_MAT
		
		# This is what a valid patch looks like
		
		"""
// brush 0
{
patchDef2
{
NULL
( 3 3 0 0 0 )
(
( ( -64 -64 0 0 0 ) ( -64 0 0 0 -2 ) ( -64 64 0 0 -4 ) )
( ( 0 -64 0 2 0 ) ( 0 0 0 2 -2 ) ( 0 64 0 2 -4 ) )
( ( 64 -64 0 4 0 ) ( 64 0 0 4 -2 ) ( 80 88 0 4 -4 ) )
)
}
}
		"""
		for i, nurb in enumerate(data):
			u= nurb.pointsU
			v= nurb.pointsV
			if u in valid_dims and v in valid_dims:
				
				file.write('// brush %d surf_name\n' % i)
				file.write('{\n')
				file.write('patchDef2\n')
				file.write('{\n')
				file.write('NULL\n')
				file.write('( %d %d 0 0 0 )\n' % (u, v) )
				file.write('(\n')
				
				u_iter = 0
				for p in nurb:
					
					if u_iter == 0:
						file.write('(')
					
					u_iter += 1
					
					# add nmapping 0 0 ?
					if PREF_GRID_SNAP.val:
						file.write(' ( %d %d %d 0 0 )' % round_vec(Mathutils.Vector(p[0:3]) * mat))
					else:
						file.write(' ( %.6f %.6f %.6f 0 0 )' % tuple(Mathutils.Vector(p[0:3]) * mat))
					
					# Move to next line
					if u_iter == u:
						file.write(' )\n')
						u_iter = 0
				
				file.write(')\n')
				file.write('}\n')
				file.write('}\n')
				
				
				# Debugging
				# for p in nurb: print 'patch', p
				
			else:
				print "NOT EXPORTING PATCH", surf_name, u,v, 'Unsupported'
	
	
	if obs_mesh or obs_surf:
		file.write('}\n') # end worldspan
	
	
	print '\twriting lamps'
	for ob in obs_lamp:
		print '\t\t%s' % ob.name
		lamp= ob.data
		file.write('{\n')
		file.write('"classname" "light"\n')
		file.write('"light" "%.6f"\n' % (lamp.dist* PREF_SCALE.val))
		if PREF_GRID_SNAP.val:
			file.write('"origin" "%d %d %d"\n' % tuple([round(axis*PREF_SCALE.val) for axis in ob.getLocation('worldspace')]) )
		else:
			file.write('"origin" "%.6f %.6f %.6f"\n' % tuple([axis*PREF_SCALE.val for axis in ob.getLocation('worldspace')]) )
		file.write('"_color" "%.6f %.6f %.6f"\n' % tuple(lamp.col))
		file.write('"style" "0"\n')
		file.write('}\n')
		TOTLAMP+=1
	
	
	print '\twriting empty objects as nodes'
	for ob in obs_empty:
		if write_node_map(file, ob):
			print '\t\t%s' % ob.name
			TOTNODE+=1
		else:
			print '\t\tignoring %s' % ob.name
	
	Window.WaitCursor(0)
	
	print 'Exported Map in %.4fsec' % (sys.time()-time)
	print 'Brushes: %d  Nodes: %d  Lamps %d\n' % (TOTBRUSH, TOTNODE, TOTLAMP)
	
	
def main():
	Window.FileSelector(export_map, 'EXPORT MAP', '*.map')

if __name__ == '__main__': main()
# export_map('/foo.map')
