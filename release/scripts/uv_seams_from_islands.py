#!BPY
"""
Name: 'Seams from Islands'
Blender: 243
Group: 'UV'
Tooltip: 'Add seams onto the mesh at the bounds of UV islands'
"""

# Add a licence here if you wish to re-distribute, we recommend the GPL

from Blender import Scene, Mesh, Window, sys
import BPyMessages

def seams_from_islands(me):
	# This function runs out of editmode with a mesh
	# error cases are alredy checked for
	
	# next intex 
	wrap_q = [1,2,3,0]
	wrap_t = [1,2,0]
	edge_uvs = {}
	for f in me.faces:
		f_uv = [(round(uv.x, 6), round(uv.y, 6)) for uv in f.uv]
		f_vi = [v.index for v in f]
		for i, key in enumerate(f.edge_keys):
			if len(f)==3:
				uv1, uv2 = f_uv[i], f_uv[wrap_t[i]]
				vi1, vi2 = f_vi[i], f_vi[wrap_t[i]]
			else: # quad
				uv1, uv2 = f_uv[i], f_uv[wrap_q[i]]
				vi1, vi2 = f_vi[i], f_vi[wrap_q[i]]
				
			if vi1 > vi2: uv1,uv2 = uv2,uv1
			
			edge_uvs.setdefault(key, []).append((uv1, uv2))
	
	# add seams
	SEAM = Mesh.EdgeFlags.SEAM
	for ed in me.edges:
		if len(set(edge_uvs[ed.key])) > 1:
			ed.flag |= SEAM

def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = Scene.GetCurrent()
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	me = ob_act.getData(mesh=1)
	
	if not ob_act or ob_act.type != 'Mesh' or not me.faceUV:
		BPyMessages.Error_NoMeshUvActive()
		return 
	
	# Saves the editmode state and go's out of 
	# editmode if its enabled, we cant make
	# changes to the mesh data while in editmode.
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	
	Window.WaitCursor(1)
	
	t = sys.time()
	
	# Run the mesh editing function
	seams_from_islands(me)
	
	if is_editmode: Window.EditMode(1)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'UV Seams from Islands finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()
