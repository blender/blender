#!BPY
"""
Name: 'Clean meshes'
Blender: 228
Group: 'Mesh'
Tooltip: 'Clean unused data from all selected mesh objects.'
"""
from Blender import *
from Blender.Mathutils import TriangleArea

def rem_free_verts(me):
	vert_users = [0] * len(me.verts)
	for f in me.faces:
		for v in f.v:
			vert_users[v.index]+=1
	
	for e in me.edges:
		for v in e: # loop on edge verts
			vert_users[v.index]+=1
	
	verts_free = []
	for i, users in enumerate(vert_users):
		if not users:
			verts_free.append(i)
	
	if verts_free:
		pass
		me.verts.delete(verts_free)
	return len(verts_free)
	
def rem_free_edges(me, limit=None):
	''' Only remove based on limit if a limit is set, else remove all '''
	def sortPair(a,b):
		return min(a,b), max(a,b)
	
	edgeDict = {} # will use a set when python 2.4 is standard.
	
	for f in me.faces:
		for i in xrange(len(f.v)):
			edgeDict[sortPair(f.v[i].index, f.v[i-1].index)] = None
	
	edges_free = []
	for e in me.edges:
		if not edgeDict.has_key(sortPair(e.v1.index, e.v2.index)):
			edges_free.append(e)
	
	if limit != None:
		edges_free = [e for e in edges_free if (e.v1.co-e.v2.co).length <= limit]
	
	me.edges.delete(edges_free)
	return len(edges_free)

def rem_area_faces(me, limit=0.001):
	''' Faces that have an area below the limit '''
	def faceArea(f):
		if len(f.v) == 3:
			return TriangleArea(f.v[0].co, f.v[1].co, f.v[2].co)
		elif len(f.v) == 4:
			return\
			 TriangleArea(f.v[0].co, f.v[1].co, f.v[2].co) +\
			 TriangleArea(f.v[0].co, f.v[2].co, f.v[3].co)
	rem_faces = [f for f in me.faces if faceArea(f) <= limit]
	if rem_faces:
		me.faces.delete( 0, rem_faces )
	return len(rem_faces)

def rem_perimeter_faces(me, limit=0.001):
	''' Faces whos combine edge length is below the limit '''
	def faceEdLen(f):
		if len(f.v) == 3:
			return\
			(f.v[0].co-f.v[1].co).length +\
			(f.v[1].co-f.v[2].co).length +\
			(f.v[2].co-f.v[0].co).length
		elif len(f.v) == 4:
			return\
			(f.v[0].co-f.v[1].co).length +\
			(f.v[1].co-f.v[2].co).length +\
			(f.v[2].co-f.v[3].co).length +\
			(f.v[3].co-f.v[0].co).length
	rem_faces = [f for f in me.faces if faceEdLen(f) <= limit]
	if rem_faces:
		me.faces.delete( 0, rem_faces )
	return len(rem_faces)

def main():
	
	def getLimit(text):
		return Draw.PupFloatInput(text, 0.001, 0.0, 1.0, 0.1, 4)
	
	
	scn = Scene.GetCurrent()
	obsel = Object.GetSelected()
	actob = scn.getActiveObject()
	
	is_editmode = Window.EditMode()
	
	# Edit mode object is not active, add it to the list.
	if is_editmode and (not actob.sel):
		obsel.append(actob)
	
	meshes = [ob.getData(mesh=1) for ob in obsel if ob.getType() == 'Mesh']
	
	
	#====================================#
	# Popup menu to select the functions #
	#====================================#
	'''
	if not meshes:
		Draw.PupMenu('ERROR%t|no meshes in selection')
		return
	method = Draw.PupMenu("""
Clean Mesh, Remove...%t|
Verts: free standing|
Edges: not in a face|
Edges: below a length|
Faces: below an area|%l|
All of the above|""")
	if method == -1:
		return
	
	if method >= 3:
		limit = getLimit('threshold: ')
	
	print 'method', method
	'''
	
	
	CLEAN_VERTS_FREE = Draw.Create(1)
	CLEAN_EDGE_NOFACE = Draw.Create(0)
	CLEAN_EDGE_SMALL = Draw.Create(0)
	CLEAN_FACE_PERIMETER = Draw.Create(0)
	CLEAN_FACE_SMALL = Draw.Create(0)
	limit = Draw.Create(0.01)
	
	# Get USER Options
	
	pup_block = [\
	('Verts: free', CLEAN_VERTS_FREE, 'Remove verts that are not used by an edge or a face.'),\
	('Edges: free', CLEAN_EDGE_NOFACE, 'Remove edges that are not in a face.'),\
	('Edges: short', CLEAN_EDGE_SMALL, 'Remove edges that are below the length limit.'),\
	('Faces: small perimeter', CLEAN_FACE_PERIMETER, 'Remove faces below the perimeter limit.'),\
	('Faces: small area', CLEAN_FACE_SMALL, 'Remove faces below the area limit (may remove faces stopping T-face artifacts).'),\
	('limit: ', limit, 0.001, 1.0, 'Limit used for the area and length tests above (a higher limit will remove more data).'),\
	]
	
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	
	CLEAN_VERTS_FREE = CLEAN_VERTS_FREE.val
	CLEAN_EDGE_NOFACE = CLEAN_EDGE_NOFACE.val
	CLEAN_EDGE_SMALL = CLEAN_EDGE_SMALL.val
	CLEAN_FACE_PERIMETER = CLEAN_FACE_PERIMETER.val
	CLEAN_FACE_SMALL = CLEAN_FACE_SMALL.val
	limit = limit.val
	
	if is_editmode: Window.EditMode(0)
	
	rem_face_count = rem_edge_count = rem_vert_count = 0
	
	for me in meshes:
		if CLEAN_FACE_SMALL:
			rem_face_count += rem_area_faces(me, limit)
			
		if CLEAN_FACE_PERIMETER:
			rem_face_count += rem_perimeter_faces(me, limit)
		
		if CLEAN_EDGE_SMALL: # for all use 2- remove all edges.
			rem_edge_count += rem_free_edges(me, limit)
		
		if CLEAN_EDGE_NOFACE:
			rem_edge_count += rem_free_edges(me)
		
		if CLEAN_VERTS_FREE:
			rem_vert_count += rem_free_verts(me)
	
	if is_editmode: Window.EditMode(0)
	Draw.PupMenu('Removed from ' + str(len(meshes)) +' Mesh(es)%t|' + 'Verts:' + str(rem_vert_count) + ' Edges:' + str(rem_edge_count) + ' Faces:' + str(rem_face_count))
	
if __name__ == '__main__':
	main()
	
