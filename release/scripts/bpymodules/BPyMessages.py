from Blender import Draw, sys
def Error_NoMeshSelected():
	Draw.PupMenu('ERROR%t|No mesh objects selected')
def Error_NoMeshActive():
	Draw.PupMenu('ERROR%t|Active object is not a mesh')
def Error_NoMeshUvSelected():
	Draw.PupMenu('ERROR%t|No mesh objects with texface selected')
def Error_NoMeshUvActive():
	Draw.PupMenu('ERROR%t|Active object is not a mesh with texface')

# File I/O messages
def Error_NoFile(path):
	'''True if file missing, False if files there'''
	if not sys.exists(sys.expandpath(path)):
		Draw.PupMenu("Error%t|Can't open file: " + path)
		return True
	return False

def Warning_SaveOver(path):
	'''Returns - True to save, False dont save'''
	if sys.exists(sys.expandpath(path)):
		ret= Draw.PupMenu('Save over%t|' + path)
		if ret == -1:
			return False
	
	return True
