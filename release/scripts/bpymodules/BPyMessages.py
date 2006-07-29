from Blender import Draw
def Error_NoMeshSelected():
	Draw.PupMenu('ERROR%t|No mesh objects selected')
def Error_NoMeshActive():
	Draw.PupMenu('ERROR%t|Active object is not a mesh')
def Error_NoMeshUvSelected():
	Draw.PupMenu('ERROR%t|No mesh objects with texface selected')
def Error_NoMeshUvActive():
	Draw.PupMenu('ERROR%t|Active object is not a mesh with texface')

