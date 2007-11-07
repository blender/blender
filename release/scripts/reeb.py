#!BPY
 
"""
Name: 'Reeb graph import'
Blender: 245
Group: 'Import'
Tooltip: 'Imports a reeb graph saved after skeleton generation'
"""
import Blender

def name(count):
	if count == -1:
		return ""
	else:
		return "%05" % count

def importGraph(count):
	me = Blender.Mesh.New("graph%s" % name(count))
	
	f = open("test%s.txt" % name(count), "r")
	
	verts = []
	edges = []
	faces = []
	
	i = 0
	first = False
	
	SIZE = 0.3
	WITH_NODE = False
	
	def addNode(v, s, verts, faces):
		if WITH_NODE:
			v1 = [v[0], v[1], v[2] + s]
			i1 = len(verts)
			verts.append(v1)
			v2 = [v[0], v[1] + 0.959 * s, v[2] - 0.283 * s]
			i2 = len(verts)
			verts.append(v2)
			v3 = [v[0] - 0.830 * s, v[1] - 0.479 * s, v[2] - 0.283 * s]
			i3 = len(verts)
			verts.append(v3)
			v4 = [v[0] + 0.830 * s, v[1] - 0.479 * s, v[2] - 0.283 * s]
			i4 = len(verts)
			verts.append(v4)
			
			faces.append([i1,i2,i3])
			faces.append([i1,i3,i4])
			faces.append([i2,i3,i4])
			faces.append([i1,i2,i4])
			
			return 4
		else:
			return 0
		
	for line in f:
		data = line.strip().split(" ")
		if data[0] == "v1":
			v = [float(x) for x in data[-3:]]
			i += addNode(v, SIZE, verts, faces)
			verts.append(v)
			i += 1
		elif data[0] == "v2":
			pass
			v = [float(x) for x in data[-3:]]
			verts.append(v)
			edges.append((i-1, i))
			i += 1
			i += addNode(v, SIZE, verts, faces)
		elif data[0] == "b":
			verts.append([float(x) for x in data[-3:]])
			edges.append((i-1, i))
			i += 1
		
	
	me.verts.extend(verts)
	me.edges.extend(edges)
	me.faces.extend(faces)
	
	scn = Blender.Scene.GetCurrent()
	
	ob = scn.objects.new(me, "graph%s" % name(count))
	

#for i in range(16):
#	importGraph(i)

importGraph(-1)
import Blender

def name(count):
	if count == -1:
		return ""
	else:
		return "%05" % count

def importGraph(count):
	me = Blender.Mesh.New("graph%s" % name(count))
	
	f = open("test%s.txt" % name(count), "r")
	
	verts = []
	edges = []
	faces = []
	
	i = 0
	first = False
	
	SIZE = 0.3
	WITH_NODE = False
	
	def addNode(v, s, verts, faces):
		if WITH_NODE:
			v1 = [v[0], v[1], v[2] + s]
			i1 = len(verts)
			verts.append(v1)
			v2 = [v[0], v[1] + 0.959 * s, v[2] - 0.283 * s]
			i2 = len(verts)
			verts.append(v2)
			v3 = [v[0] - 0.830 * s, v[1] - 0.479 * s, v[2] - 0.283 * s]
			i3 = len(verts)
			verts.append(v3)
			v4 = [v[0] + 0.830 * s, v[1] - 0.479 * s, v[2] - 0.283 * s]
			i4 = len(verts)
			verts.append(v4)
			
			faces.append([i1,i2,i3])
			faces.append([i1,i3,i4])
			faces.append([i2,i3,i4])
			faces.append([i1,i2,i4])
			
			return 4
		else:
			return 0
		
	for line in f:
		data = line.strip().split(" ")
		if data[0] == "v1":
			v = [float(x) for x in data[-3:]]
			i += addNode(v, SIZE, verts, faces)
			verts.append(v)
			i += 1
		elif data[0] == "v2":
			pass
			v = [float(x) for x in data[-3:]]
			verts.append(v)
			edges.append((i-1, i))
			i += 1
			i += addNode(v, SIZE, verts, faces)
		elif data[0] == "b":
			verts.append([float(x) for x in data[-3:]])
			edges.append((i-1, i))
			i += 1
		
	
	me.verts.extend(verts)
	me.edges.extend(edges)
	me.faces.extend(faces)
	
	scn = Blender.Scene.GetCurrent()
	
	ob = scn.objects.new(me, "graph%s" % name(count))
	

#for i in range(16):
#	importGraph(i)

importGraph(-1)