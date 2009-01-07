from Blender import Draw, sys
def Error_NoMeshSelected():
	Draw.PupMenu('Error%t|No mesh objects selected')
def Error_NoActive():
	Draw.PupMenu('Error%t|No active object')
def Error_NoMeshActive():
	Draw.PupMenu('Error%t|Active object is not a mesh')
def Error_NoMeshUvSelected():
	Draw.PupMenu('Error%t|No mesh objects with texface selected')
def Error_NoMeshUvActive():
	Draw.PupMenu('Error%t|Active object is not a mesh with texface')
def Error_NoMeshMultiresEdit():
	Draw.PupMenu('Error%t|Unable to complete action with multires enabled')
def Error_NoMeshFaces():
	Draw.PupMenu('Error%t|Mesh has no faces')

# File I/O messages
def Error_NoFile(path):
	'''True if file missing, False if files there
	
	Use simply by doing...
	if Error_NoFile(path): return
	'''
	if not sys.exists(sys.expandpath(path)):
		Draw.PupMenu("Error%t|Can't open file: " + path)
		return True
	return False

def Error_NoDir(path):
	'''True if dirs missing, False if dirs there
	
	Use simply by doing...
	if Error_NoDir(path): return
	'''
	if not sys.exists(sys.expandpath(path)):
		Draw.PupMenu("Error%t|Path does not exist: " + path)
		return True
	return False


def Warning_MeshDistroyLayers(mesh):
	'''Returns true if we can continue to edit the mesh, warn when using NMesh'''
	if len(mesh.getUVLayerNames()) >1 and len(mesh.getColorLayerNames()) >1:
		return True
	
	ret = Draw.PupMenu('Warning%t|This script will distroy inactive UV and Color layers, OK?')
	if ret == -1:
		return False
	
	return True

def Warning_SaveOver(path):
	'''Returns - True to save, False dont save'''
	if sys.exists(sys.expandpath(path)):
		ret= Draw.PupMenu('Save over%t|' + path)
		if ret == -1:
			return False
	
	return True


