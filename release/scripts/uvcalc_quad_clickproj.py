#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Click Project from face'
Blender: 242
Group: 'UVCalculation'
Tooltip: 'click'
"""



import Blender
import BPyMesh
import BPyWindow

mouseViewRay= BPyWindow.mouseViewRay
from Blender import Mathutils, Window, Scene, Draw, sys
from Blender.Mathutils import CrossVecs, Vector, Intersect, LineIntersect, AngleBetweenVecs
LMB= Window.MButs['L']

def mouseup():
	# Loop until click
	mouse_buttons = Window.GetMouseButtons()
	while not mouse_buttons & LMB:
		sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()
	while mouse_buttons & LMB:
		sys.sleep(10)
		mouse_buttons = Window.GetMouseButtons()

def mousedown_wait():
	# If the menu has just been pressed dont use its mousedown,
	mouse_buttons = Window.GetMouseButtons()
	while mouse_buttons & LMB:
		mouse_buttons = Window.GetMouseButtons()
		

def main():
	
	scn = Scene.GetCurrent()
	ob = scn.objects.active
	if not ob or ob.type!='Mesh':
		return
	
	mousedown_wait()
	
	Window.DrawProgressBar (0.0, '')
	Window.DrawProgressBar (0.1, '(1 of 3) Click on a face corner')	
	
	# wait for a click
	mouseup()
	
	Window.DrawProgressBar (0.2, '(2 of 3 ) Click confirms the U coords')
	
	
	mousedown_wait()
	
	obmat= ob.matrixWorld
	screen_x, screen_y = Window.GetMouseCoords()
	mouseInView, OriginA, DirectionA = mouseViewRay(screen_x, screen_y, obmat)
	
	if not mouseInView or not OriginA:
		Window.DrawProgressBar (1.0, '')
		return
		
	me = ob.getData(mesh=1)
	
	SELECT_FLAG = Blender.Mesh.FaceFlags['SELECT']
	me_faces_sel = [ f for f in me.faces if f.flag & SELECT_FLAG ]
	del SELECT_FLAG
	
	f, isect, side = BPyMesh.pickMeshRayFace(me, OriginA, DirectionA)
	f_no = f.no
	if not f:
		Window.DrawProgressBar (1.0, '')
		return
	
	# find the vertex thats closest
	
	best_v= None
	best_length = 10000000
	vi1 = None
	for i, v in enumerate(f.v):
		l = (v.co-isect).length
		if l < best_length:
			best_v = v
			best_length = l
			vi1 = i
	
	# now find the 2 edges in the face that connect to v
	if len(f)==4:
		if vi1==0: vi2, vi3= 3,1
		if vi1==1: vi2, vi3= 0,2
		if vi1==2: vi2, vi3= 1,3
		if vi1==3: vi2, vi3= 2,0
	else:
		if vi1==0: vi2, vi3= 2,1
		if vi1==1: vi2, vi3= 0,2
		if vi1==2: vi2, vi3= 1,0
	
	
	loc1 =f.v[vi1].co
	loc2 =f.v[vi2].co
	loc3 =f.v[vi3].co
	
	line1_len = (loc2-loc1).length
	line2_len = (loc3-loc1).length
	
	
	# tmat = Mathutils.TranslationMatrix(-loc1)
	
	Window.SetCursorPos(loc1.x, loc1.y, loc1.z)
	
	MODE = 0 # firstclick, 1, secondclick
	mouse_buttons = Window.GetMouseButtons()
	Window.SetCursorPos(loc1.x, loc1.y, loc1.z)
	while 1:
		if mouse_buttons & LMB:
			if MODE == 0:
				mousedown_wait()
				Window.DrawProgressBar (0.8, '(3 of 3 ) Click confirms the V coords')
				MODE = 1 # second click
			else:
				break
		
		mouse_buttons = Window.GetMouseButtons()
		screen_x, screen_y = Window.GetMouseCoords()
		mouseInView, OriginA, DirectionA = mouseViewRay(screen_x, screen_y, obmat)
		
		if not mouseInView:
			continue
		
		d1_pair = Mathutils.LineIntersect(OriginA, OriginA+DirectionA, loc1, loc2)
		d2_pair = Mathutils.LineIntersect(OriginA, OriginA+DirectionA, loc1, loc3)
		if MODE == 0:	
			d1 = (d1_pair[0]-d1_pair[1]).length
			d2 = (d2_pair[0]-d2_pair[1]).length
			
			if d1<d2:
				loc_to_use = loc2
				y_dist = line2_len
				x_dist = (d1_pair[1]-loc1).length
			else:
				loc_to_use = loc3
				y_dist = line1_len
				x_dist = (d2_pair[1]-loc1).length
			
			line_x = loc_to_use - loc1
			
			line_y = Mathutils.CrossVecs(line_x, f_no)
			line_x.length = 1/x_dist
			line_y.length = 1/y_dist
			
		else:
			if d1<d2:	line_y.length = 1/(d2_pair[1]-loc1).length
			else:		line_y.length = 1/(d1_pair[1]-loc1).length
		
		# Make a matrix
		project_mat = Mathutils.Matrix(line_x, line_y, f_no)
		project_mat.resize4x4()
		
		
		for f in me_faces_sel:
			f.uv = [project_mat * (v.co-loc1) for v in f]
		
		Window.Redraw(Window.Types.VIEW3D)
	
	Window.DrawProgressBar (1.0, '')
	
if __name__=='__main__':
	main()