#!BPY
"""
Name: 'LightWave (.lwo)...'
Blender: 239
Group: 'Import'
Tooltip: 'Import LightWave Object File Format'
"""

__author__ = ["Alessandro Pirovano, Anthony D'Agostino (Scorpius)", "Campbell Barton (ideasman42)", "ZanQdo"]
__url__ = ("www.blender.org", "blenderartist.org",
"Anthony's homepage, http://www.redrival.com/scorpius", "Alessandro's homepage, http://uaraus.altervista.org")

importername = "lwo_import 0.4.0"

# +---------------------------------------------------------+
# | Save your work before and after use.                    |
# | Please report any useful comment to:                    |
# | uaraus-dem@yahoo.it                                     |
# | Thanks                                                  |
# +---------------------------------------------------------+
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 21, 2002                                          |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write LightWave Object File Format (*.lwo)     |
# +---------------------------------------------------------+
# +---------------------------------------------------------+
# | Alessandro Pirovano tweaked starting on March 2005      |
# | http://uaraus.altervista.org                            |
# +---------------------------------------------------------+
# +----------------------------------------------------------
# | GPL license block
# |
# | This program is free software; you can redistribute it and/or modify
# | it under the terms of the GNU General Public License as published by
# | the Free Software Foundation; either version 2 of the License, or
# | (at your option) any later version.
# |
# | This program is distributed in the hope that it will be useful,
# | but WITHOUT ANY WARRANTY; without even the implied warranty of
# | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# | GNU General Public License for more details.
# |
# | You should have received a copy of the GNU General Public License
# | along with this program; if not, write to the Free Software
# | Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# +----------------------------------------------------------
# +---------------------------------------------------------+
# | Release log:                                            |
# | 0.4.0 : Updated for blender 2.44                        |
# |         ZanQdo - made the mesh import the right way up  |
# |         Ideasman42 - Updated functions for the bew API  |
# |           as well as removing the text object class     |
# | 0.2.2 : This code works with Blender 2.42 RC3           |
# |         Added a new PolyFill function for BPYMesh's     |
# |           ngon() to use, checked compatibility          |
# |           lightwaves ngons are imported as fgons        |
# |         Checked compatibility against 1711 lwo files    |
# | 0.2.1 : This code works with Blender 2.40 RC1           |
# |         modified material mode assignment to deal with  |
# |         Python API modification                         |
# |         Changed script license to GNU GPL               |
# | 0.2.0:  This code works with Blender 2.40a2 or up       |
# |         Major rewrite to deal with large meshes         |
# |         - 2 pass file parsing                           |
# |         - lower memory foot###if DEBUG: print                        |
# |           (as long as python gc allows)                 |
# |         2.40a2 - Removed subsurf settings patches=poly  |
# |         2.40a2 - Edge generation instead of 2vert faces |
# | 0.1.16: fixed (try 2) texture offset calculations       |
# |         added hint on axis mapping                      |
# |         added hint on texture blending mode             |
# |         added hint on texture transparency setting      |
# |         search images in original directory first       |
# |         fixed texture order application                 |
# | 0.1.15: added release log                               |
# |         fixed texture offset calculations (non-UV)      |
# |         fixed reverting vertex order in face generation |
# |         associate texture on game-engine settings       |
# |         vector math definitely based on mathutils       |
# |         search images in "Images" and "../Images" dir   |
# |         revised logging facility                        |
# |         fixed subsurf texture and material mappings     |
# | 0.1.14: patched missing mod_vector (not definitive)     |
# | 0.1.13: first public release                            |
# +---------------------------------------------------------+

#blender related import
import Blender
import bpy

# use for comprehensiveImageLoad
import BPyImage

# Use this ngon function
import BPyMesh

import BPyMessages

#python specific modules import
try:
	import struct, chunk, cStringIO
except:
	struct= chunk= cStringIO= None

# python 2.3 has no reversed() iterator. this will only work on lists and tuples
try:
	reversed
except:
	def reversed(l): return l[::-1]

### # Debuggin disabled in release.
### # do a search replace to enabe debug prints
### DEBUG = False

# ===========================================================
# === Utility Preamble ======================================
# ===========================================================

textname = None
#uncomment the following line to enable logging facility to the named text object
#textname = "lwo_log"

TXMTX = Blender.Mathutils.Matrix(\
[1, 0, 0, 0],\
[0, 0, 1, 0],\
[0, 1, 0, 0],\
[0, 0, 0, 1])

# ===========================================================
# === Make sure it is a string ... deal with strange chars ==
# ===========================================================
def safestring(st):
	myst = ""
	for ll in xrange(len(st)):
		if st[ll] < " ":
			myst += "#"
		else:
			myst += st[ll]
	return myst

# ===========================================================
# === Main read functions ===================================
# ===========================================================

# =============================
# === Read LightWave Format ===
# =============================
def read(filename):
	if BPyMessages.Error_NoFile(filename):
		return

	print "This is: %s" % importername
	print "Importing file:", filename
	bpy.data.scenes.active.objects.selected = []
	
	start = Blender.sys.time()
	file = open(filename, "rb")
	
	editmode = Blender.Window.EditMode()    # are we in edit mode?  If so ...
	if editmode: Blender.Window.EditMode(0) # leave edit mode before getting the mesh    # === LWO header ===
	
	try:
		form_id, form_size, form_type = struct.unpack(">4s1L4s",  file.read(12))
	except:
		Blender.Draw.PupMenu('Error%t|This is not a lightwave file')
		return
	
	if (form_type == "LWOB"):
		read_lwob(file, filename)
	elif (form_type == "LWO2"):
		read_lwo2(file, filename)
	else:
		print "Can't read a file with the form_type: %s" % form_type
		return

	Blender.Window.DrawProgressBar(1.0, "")    # clear progressbar
	file.close()
	end = Blender.sys.time()
	seconds = " in %.2f %s" % (end-start, "seconds")
	if form_type == "LWO2": fmt = " (v6.0 Format)"
	if form_type == "LWOB": fmt = " (v5.5 Format)"
	print "Successfully imported " + filename.split('\\')[-1].split('/')[-1] + fmt + seconds
	
	if editmode: Blender.Window.EditMode(1)  # optional, just being nice
	Blender.Redraw()

# enddef read


# =================================
# === Read LightWave 5.5 format ===
# =================================
def read_lwob(file, filename):
	#This function is directly derived from the LWO2 import routine
	#dropping all the material analysis parts

	###if DEBUG: print "LightWave 5.5 format"

	dir_part = Blender.sys.dirname(filename)
	fname_part = Blender.sys.basename(filename)
	#ask_weird = 1

	#first initialization of data structures
	defaultname = Blender.sys.splitext(fname_part)[0]
	tag_list = []              #tag list: global for the whole file?
	surf_list = []             #surf list: global for the whole file?
	clip_list = []             #clip list: global for the whole file?
	object_index = 0
	object_list = None
	objspec_list = None

	#add default material for orphaned faces, if any
	surf_list.append({'NAME': "_Orphans", 'g_MAT': bpy.data.materials.new("_Orphans")})

	#pass 2: effectively generate objects
	###if DEBUG: print "Pass 1: dry import"
	file.seek(0)
	objspec_list = ["imported", {}, [], [], {}, {}, 0, {}, {}]
	# === LWO header ===
	form_id, form_size, form_type = struct.unpack(">4s1L4s",  file.read(12))
	if (form_type != "LWOB"):
		###if DEBUG: print "??? Inconsistent file type: %s" % form_type
		return
	while 1:
		try:
			lwochunk = chunk.Chunk(file)
		except EOFError:
			break
		###if DEBUG: print ' ',
		if lwochunk.chunkname == "LAYR":
			###if DEBUG: print "---- LAYR",
			objname = read_layr(lwochunk)
			###if DEBUG: print objname
			if objspec_list != None: #create the object
				create_objects(clip_list, objspec_list, surf_list)
				update_material(clip_list, objspec_list, surf_list) #give it all the object
			objspec_list = [objname, {}, [], [], {}, {}, 0, {}, {}]
			object_index += 1
		elif lwochunk.chunkname == "PNTS":                         # Verts
			###if DEBUG: print "---- PNTS",
			verts = read_verts(lwochunk)
			objspec_list[2] = verts
		elif lwochunk.chunkname == "POLS": # Faces v5.5
			###if DEBUG: print "-------- POLS(5.5)"
			faces = read_faces_5(lwochunk)
			flag = 0
			#flag is 0 for regular polygon, 1 for patches (= subsurf), 2 for anything else to be ignored
			if flag<2:
				if objspec_list[3] != []:
					#create immediately the object
					create_objects(clip_list, objspec_list, surf_list)
					update_material(clip_list, objspec_list, surf_list) #give it all the object
					#update with new data
					objspec_list = [objspec_list[0],                  #update name
									{},                               #init
									objspec_list[2],                  #same vertexes
									faces,                            #give it the new faces
									{},                               #no need to copy - filled at runtime
									{},                               #polygon tagging will follow
									flag,                             #patch flag
									objspec_list[7],                  #same uvcoords
									{}]                               #no vmad mapping
					object_index += 1
				#end if already has a face list
				objspec_list[3] = faces
				objname = objspec_list[0]
				if objname == None:
					objname = defaultname
			#end if processing a valid poly type
		else:                                                       # Misc Chunks
			###if DEBUG: print "---- %s: skipping (definitely!)" % lwochunk.chunkname
			lwochunk.skip()
		#uncomment here to log data structure as it is built
		# ###if DEBUG: print object_list
	#last object read
	create_objects(clip_list, objspec_list, surf_list)
	update_material(clip_list, objspec_list, surf_list) #give it all the object
	objspec_list = None
	surf_list = None
	clip_list = None


	###if DEBUG: print "\nFound %d objects:" % object_index

# enddef read_lwob


# =============================
# === Read LightWave Format ===
# =============================
def read_lwo2(file, filename, typ="LWO2"):

	###if DEBUG: print "LightWave 6 (and above) format"

	dir_part = Blender.sys.dirname(filename)
	fname_part = Blender.sys.basename(filename)
	ask_weird = 1

	#first initialization of data structures
	defaultname = Blender.sys.splitext(fname_part)[0]
	tag_list = []              #tag list: global for the whole file?
	surf_list = []             #surf list: global for the whole file?
	clip_list = []             #clip list: global for the whole file?
	object_index = 0
	object_list = None
	objspec_list = None
	# init value is: object_list = [[None, {}, [], [], {}, {}, 0, {}, {}]]
	#0 - objname                    #original name
	#1 - obj_dict = {TAG}           #objects created
	#2 - verts = []                 #object vertexes
	#3 - faces = []                 #object faces (associations poly -> vertexes)
	#4 - obj_dim_dict = {TAG}       #tuples size and pos in local object coords - used for NON-UV mappings
	#5 - polytag_dict = {TAG}       #tag to polygons mapping
	#6 - patch_flag                 #0 = surf; 1 = patch (subdivision surface) - it was the image list
	#7 - uvcoords_dict = {name}     #uvmap coordinates (mixed mode per vertex/per face)
	#8 - facesuv_dict = {name}      #vmad only coordinates associations poly & vertex -> uv tuples

	#pass 1: look in advance for materials
	###if DEBUG: print "Starting Pass 1: hold on tight"
	while 1:
		try:
			lwochunk = chunk.Chunk(file)
		except EOFError:
			break
		###if DEBUG: print ' ',
		if lwochunk.chunkname == "TAGS":                         # Tags
			###if DEBUG: print "---- TAGS"
			tag_list.extend(read_tags(lwochunk))
		elif lwochunk.chunkname == "SURF":                         # surfaces
			###if DEBUG: print "---- SURF"
			surf_list.append(read_surfs(lwochunk, surf_list, tag_list))
		elif lwochunk.chunkname == "CLIP":                         # texture images
			###if DEBUG: print "---- CLIP"
			clip_list.append(read_clip(lwochunk, dir_part))
			###if DEBUG: print "read total %s clips up to now" % len(clip_list)
		else:                                                       # Misc Chunks
			if ask_weird:
				ckname = safestring(lwochunk.chunkname)
				if "#" in ckname:
					choice = Blender.Draw.PupMenu("WARNING: file could be corrupted.%t|Import anyway|Give up")
					if choice != 1:
						###if DEBUG: print "---- %s: Maybe file corrupted. Terminated by user" % lwochunk.chunkname
						return
					ask_weird = 0
			###if DEBUG: print "---- %s: skipping (maybe later)" % lwochunk.chunkname
			lwochunk.skip()

	#add default material for orphaned faces, if any
	surf_list.append({'NAME': "_Orphans", 'g_MAT': bpy.data.materials.new("_Orphans")})

	#pass 2: effectively generate objects
	###if DEBUG: print "Pass 2: now for the hard part"
	file.seek(0)
	# === LWO header ===
	form_id, form_size, form_type = struct.unpack(">4s1L4s",  file.read(12))
	if (form_type != "LWO2"):
		###if DEBUG: print "??? Inconsistent file type: %s" % form_type
		return
	while 1:
		try:
			lwochunk = chunk.Chunk(file)
		except EOFError:
			break
		###if DEBUG: print ' ',
		if lwochunk.chunkname == "LAYR":
			###if DEBUG: print "---- LAYR"
			objname = read_layr(lwochunk)
			###if DEBUG: print objname
			if objspec_list != None: #create the object
				create_objects(clip_list, objspec_list, surf_list)
				update_material(clip_list, objspec_list, surf_list) #give it all the object
			objspec_list = [objname, {}, [], [], {}, {}, 0, {}, {}]
			object_index += 1
		elif lwochunk.chunkname == "PNTS":                         # Verts
			###if DEBUG: print "---- PNTS"
			verts = read_verts(lwochunk)
			objspec_list[2] = verts
		elif lwochunk.chunkname == "VMAP":                         # MAPS (UV)
			###if DEBUG: print "---- VMAP"
			#objspec_list[7] = read_vmap(objspec_list[7], len(objspec_list[2]), lwochunk)
			read_vmap(objspec_list[7], len(objspec_list[2]), lwochunk)
		elif lwochunk.chunkname == "VMAD":                         # MAPS (UV) per-face
			###if DEBUG: print "---- VMAD"
			#objspec_list[7], objspec_list[8] = read_vmad(objspec_list[7], objspec_list[8], len(objspec_list[3]), len(objspec_list[2]), lwochunk)
			read_vmad(objspec_list[7], objspec_list[8], len(objspec_list[3]), len(objspec_list[2]), lwochunk)
		elif lwochunk.chunkname == "POLS": # Faces v6.0
			###if DEBUG: print "-------- POLS(6)"
			faces, flag = read_faces_6(lwochunk)
			#flag is 0 for regular polygon, 1 for patches (= subsurf), 2 for anything else to be ignored
			if flag<2:
				if objspec_list[3] != []:
					#create immediately the object
					create_objects(clip_list, objspec_list, surf_list)
					update_material(clip_list, objspec_list, surf_list) #give it all the object
					#update with new data
					objspec_list = [objspec_list[0],                  #update name
									{},                               #init
									objspec_list[2],                  #same vertexes
									faces,                            #give it the new faces
									{},                               #no need to copy - filled at runtime
									{},                               #polygon tagging will follow
									flag,                             #patch flag
									objspec_list[7],                  #same uvcoords
									{}]                               #no vmad mapping
					object_index += 1
				#end if already has a face list
				objspec_list[3] = faces
				objname = objspec_list[0]
				if objname == None:
					objname = defaultname
			#end if processing a valid poly type
		elif lwochunk.chunkname == "PTAG":                         # PTags
			###if DEBUG: print "---- PTAG"
			polytag_dict = read_ptags(lwochunk, tag_list)
			for kk, polytag_dict_val in polytag_dict.iteritems(): objspec_list[5][kk] = polytag_dict_val
		else:                                                       # Misc Chunks
			###if DEBUG: print "---- %s: skipping (definitely!)" % lwochunk.chunkname
			lwochunk.skip()
		#uncomment here to log data structure as it is built
		
	#last object read
	create_objects(clip_list, objspec_list, surf_list)
	update_material(clip_list, objspec_list, surf_list) #give it all the object
	objspec_list = None
	surf_list = None
	clip_list = None

	###if DEBUG: print "\nFound %d objects:" % object_index
# enddef read_lwo2






# ===========================================================
# === File reading routines =================================
# ===========================================================
# ==================
# === Read Verts ===
# ==================
def read_verts(lwochunk):
	#data = cStringIO.StringIO(lwochunk.read())
	numverts = lwochunk.chunksize/12
	return [struct.unpack(">fff", lwochunk.read(12)) for i in xrange(numverts)]
# enddef read_verts


# =================
# === Read Name ===
# =================
# modified to deal with odd lenght strings
def read_name(file):
	name = ""
	while 1:
		char = file.read(1)
		if char == "\0": break
		else: name += char
	len_name = len(name) + 1 #count the trailing zero
	if len_name%2==1:
		char = file.read(1) #remove zero padding to even lenght
		len_name += 1
	return name, len_name


# ==================
# === Read Layer ===
# ==================
def read_layr(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	idx, flags = struct.unpack(">hh", data.read(4))
	pivot = struct.unpack(">fff", data.read(12))
	layer_name, discard = read_name(data)
	if not layer_name: layer_name = "NoName"
	return layer_name
# enddef read_layr


# ======================
# === Read Faces 5.5 ===
# ======================
def read_faces_5(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	faces = []
	i = 0
	while i < lwochunk.chunksize:
		#if not i%1000 and my_meshtools.show_progress:
		#   Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
		
		numfaceverts, = struct.unpack(">H", data.read(2))
		facev = [struct.unpack(">H", data.read(2))[0] for j in xrange(numfaceverts)]
		facev.reverse()
		faces.append(facev)
		surfaceindex, = struct.unpack(">H", data.read(2))
		if surfaceindex < 0:
			###if DEBUG: print "***Error. Referencing uncorrect surface index"
			return
		i += (4+numfaceverts*2)
	return faces


# ==================================
# === Read Variable-Length Index ===
# ==================================
def read_vx(data):
	byte1, = struct.unpack(">B", data.read(1))
	if byte1 != 0xFF:    # 2-byte index
		byte2, = struct.unpack(">B", data.read(1))
		index = byte1*256 + byte2
		index_size = 2
	else:                # 4-byte index
		byte2, byte3, byte4 = struct.unpack(">3B", data.read(3))
		index = byte2*65536 + byte3*256 + byte4
		index_size = 4
	return index, index_size


# ======================
# === Read uvmapping ===
# ======================
def read_vmap(uvcoords_dict, maxvertnum, lwochunk):
	
	if maxvertnum == 0:
		###if DEBUG: print "Found VMAP but no vertexes to map!"
		return uvcoords_dict
	data = cStringIO.StringIO(lwochunk.read())
	map_type = data.read(4)
	if map_type != "TXUV":
		###if DEBUG: print "Reading VMAP: No Texture UV map Were Found. Map Type: %s" % map_type
		return uvcoords_dict
	dimension, = struct.unpack(">H", data.read(2))
	name, i = read_name(data) #i initialized with string lenght + zeros
	###if DEBUG: print "TXUV %d %s" % (dimension, name)
	#note if there is already a VMAD it will be lost
	#it is assumed that VMAD will follow the corresponding VMAP
	Vector = Blender.Mathutils.Vector
	try: #if uvcoords_dict.has_key(name):
		my_uv_dict = uvcoords_dict[name]          #update existing
	except: #else:
		my_uv_dict = {}    #start a brand new: this could be made more smart
	while (i < lwochunk.chunksize - 6):      #4+2 header bytes already read
		vertnum, vnum_size = read_vx(data)
		uv = struct.unpack(">ff", data.read(8))
		if vertnum >= maxvertnum:
			###if DEBUG: print "Hem: more uvmap than vertexes? ignoring uv data for vertex %d" % vertnum
			pass
		else:
			my_uv_dict[vertnum] = Vector(uv)
		i += 8 + vnum_size
	#end loop on uv pairs
	uvcoords_dict[name] = my_uv_dict
	#this is a per-vertex mapping AND the uv tuple is vertex-ordered, so faces_uv is the same as faces
	#return uvcoords_dict
	return

# ========================
# === Read uvmapping 2 ===
# ========================
def read_vmad(uvcoords_dict, facesuv_dict, maxfacenum, maxvertnum, lwochunk):
	if maxvertnum == 0 or maxfacenum == 0:
		###if DEBUG: print "Found VMAD but no vertexes to map!"
		return uvcoords_dict, facesuv_dict
	data = cStringIO.StringIO(lwochunk.read())
	map_type = data.read(4)
	if map_type != "TXUV":
		###if DEBUG: print "Reading VMAD: No Texture UV map Were Found. Map Type: %s" % map_type
		return uvcoords_dict, facesuv_dict
	dimension, = struct.unpack(">H", data.read(2))
	name, i = read_name(data) #i initialized with string lenght + zeros
	###if DEBUG: print "TXUV %d %s" % (dimension, name)
	try: #if uvcoords_dict.has_key(name):
		my_uv_dict = uvcoords_dict[name]          #update existing
	except: #else:
		my_uv_dict = {}    #start a brand new: this could be made more smart
	my_facesuv_list = []
	newindex = maxvertnum + 10 #why +10? Why not?
	#end variable initialization
	Vector = Blender.Mathutils.Vector
	while (i < lwochunk.chunksize - 6):  #4+2 header bytes already read
		vertnum, vnum_size = read_vx(data)
		i += vnum_size
		polynum, vnum_size = read_vx(data)
		i += vnum_size
		uv = struct.unpack(">ff", data.read(8))
		if polynum >= maxfacenum or vertnum >= maxvertnum:
			###if DEBUG: print "Hem: more uvmap than vertexes? ignorig uv data for vertex %d" % vertnum
			pass
		else:
			my_uv_dict[newindex] = Vector(uv)
			my_facesuv_list.append([polynum, vertnum, newindex])
			newindex += 1
		i += 8
	#end loop on uv pairs
	uvcoords_dict[name] = my_uv_dict
	facesuv_dict[name] = my_facesuv_list
	###if DEBUG: print "updated %d vertexes data" % (newindex-maxvertnum-10)
	return


# =================
# === Read tags ===
# =================
def read_tags(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	tag_list = []
	current_tag = ""
	i = 0
	while i < lwochunk.chunksize:
		char = data.read(1)
		if char == "\0":
			tag_list.append(current_tag)
			if (len(current_tag) % 2 == 0): char = data.read(1)
			current_tag = ""
		else:
			current_tag += char
		i += 1
	###if DEBUG: print "read %d tags, list follows: %s" % (len(tag_list), tag_list)
	return tag_list


# ==================
# === Read Ptags ===
# ==================
def read_ptags(lwochunk, tag_list):
	data = cStringIO.StringIO(lwochunk.read())
	polygon_type = data.read(4)
	if polygon_type != "SURF":
		###if DEBUG: print "No Surf Were Found. Polygon Type: %s" % polygon_type
		return {}
	ptag_dict = {}
	i = 0
	while(i < lwochunk.chunksize-4): #4 bytes polygon type already read
		#if not i%1000 and my_meshtools.show_progress:
		#   Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading PTAGS")
		poln, poln_size = read_vx(data)
		i += poln_size
		tag_index, = struct.unpack(">H", data.read(2))
		if tag_index > (len(tag_list)):
			###if DEBUG: print "Reading PTAG: Surf belonging to undefined TAG: %d. Skipping" % tag_index
			return {}
		i += 2
		tag_key = tag_list[tag_index]
		try:
			ptag_dict[tag_list[tag_index]].append(poln)
		except: #if not(ptag_dict.has_key(tag_key)):
			ptag_dict[tag_list[tag_index]] = [poln]
	
	###if DEBUG: for i, ptag_dict_val in ptag_dict.iteritems(): print "read %d polygons belonging to TAG %s" % (len(ptag_dict_val ), i)
	return ptag_dict



# ==================
# === Read Clips ===
# ==================
def read_clip(lwochunk, dir_part):
# img, IMG, g_IMG refers to blender image objects
# ima, IMAG, g_IMAG refers to clip dictionary 'ID' entries: refer to blok and surf
	clip_dict = {}
	data = cStringIO.StringIO(lwochunk.read())
	data_str = data.read(4)
	if len(data_str) < 4: # can be zero also??? :/
		# Should not happen but lw can import so we should too
		return 
	
	image_index, = struct.unpack(">L", data_str)
	clip_dict['ID'] = image_index
	i = 4
	while(i < lwochunk.chunksize):
		subchunkname, = struct.unpack("4s", data.read(4))
		subchunklen, = struct.unpack(">H", data.read(2))
		if subchunkname == "STIL":
			###if DEBUG: print "-------- STIL"
			clip_name, k = read_name(data)
			#now split text independently from platform
			#depend on the system where image was saved. NOT the one where the script is run
			no_sep = "\\"
			if Blender.sys.sep == no_sep: no_sep ="/"
			if (no_sep in clip_name):
				clip_name = clip_name.replace(no_sep, Blender.sys.sep)
			short_name = Blender.sys.basename(clip_name)
			if clip_name == "" or short_name == "":
				###if DEBUG: print "Reading CLIP: Empty clip name not allowed. Skipping"
				discard = data.read(subchunklen-k)
			clip_dict['NAME'] = clip_name
			clip_dict['BASENAME'] = short_name
		elif subchunkname == "XREF":                           #cross reference another image
			###if DEBUG: print "-------- XREF"
			image_index, = struct.unpack(">L", data.read(4))
			clip_name, k = read_name(data)
			clip_dict['NAME'] = clip_name
			clip_dict['XREF'] = image_index
		elif subchunkname == "NEGA":                           #negate texture effect
			###if DEBUG: print "-------- NEGA"
			n, = struct.unpack(">H", data.read(2))
			clip_dict['NEGA'] = n
		else:                                                       # Misc Chunks
			###if DEBUG: print "-------- CLIP:%s: skipping" % subchunkname
			discard = data.read(subchunklen)
		i = i + 6 + subchunklen
	#end loop on surf chunks
	###if DEBUG: print "read image:%s" % clip_dict
	if 'XREF' in clip_dict: # has_key
		###if DEBUG: print "Cross-reference: no image pre-allocated."
		return clip_dict
	#look for images
	#img = load_image("",clip_dict['NAME'])
	NAME= BASENAME= None 
	
	try:
		NAME= clip_dict['NAME']
		BASENAME= clip_dict['BASENAME']
	except:
		clip_dict['g_IMG'] = None
		return
	# ###if DEBUG: print 'test', NAME, BASENAME
	img = BPyImage.comprehensiveImageLoad(NAME, dir_part, PLACE_HOLDER= False, RECURSIVE=False)
	if not img:
		###if DEBUG: print "***No image %s found: trying LWO file subdir" % NAME
		img = BPyImage.comprehensiveImageLoad(BASENAME, dir_part, PLACE_HOLDER= False, RECURSIVE=False)
	
	###if DEBUG: if not img: print "***No image %s found: giving up" % BASENAME
	#lucky we are: we have an image
	###if DEBUG: print "Image pre-allocated."
	clip_dict['g_IMG'] = img
	
	return clip_dict


# ===========================
# === Read Surfaces Block ===
# ===========================
def read_surfblok(subchunkdata):
	lenght = len(subchunkdata)
	my_dict = {}
	my_uvname = ""
	data = cStringIO.StringIO(subchunkdata)
	##############################################################
	# blok header sub-chunk
	##############################################################
	subchunkname, = struct.unpack("4s", data.read(4))
	subchunklen, = struct.unpack(">h", data.read(2))
	accumulate_i = subchunklen + 6
	if subchunkname != 'IMAP':
		###if DEBUG: print "---------- SURF: BLOK: %s: block aborting" % subchunkname
		return {}, ""
	###if DEBUG: print "---------- IMAP"
	ordinal, i = read_name(data)
	my_dict['ORD'] = ordinal
	#my_dict['g_ORD'] = -1
	my_dict['ENAB'] = True
	while(i < subchunklen): # ---------left 6------------------------- loop on header parameters
		sub2chunkname, = struct.unpack("4s", data.read(4))
		sub2chunklen, = struct.unpack(">h", data.read(2))
		i = i + 6 + sub2chunklen
		if sub2chunkname == "CHAN":
			###if DEBUG: print "------------ CHAN"
			sub2chunkname, = struct.unpack("4s", data.read(4))
			my_dict['CHAN'] = sub2chunkname
			sub2chunklen -= 4
		elif sub2chunkname == "ENAB":                             #only present if is to be disabled
			###if DEBUG: print "------------ ENAB"
			ena, = struct.unpack(">h", data.read(2))
			my_dict['ENAB'] = ena
			sub2chunklen -= 2
		elif sub2chunkname == "NEGA":                             #only present if is to be enabled
			###if DEBUG: print "------------ NEGA"
			ena, = struct.unpack(">h", data.read(2))
			if ena == 1:
				my_dict['NEGA'] = ena
			sub2chunklen -= 2
		elif sub2chunkname == "OPAC":                             #only present if is to be disabled
			###if DEBUG: print "------------ OPAC"
			opa, = struct.unpack(">h", data.read(2))
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['OPAC'] = opa
			my_dict['OPACVAL'] = s
			sub2chunklen -= 6
		elif sub2chunkname == "AXIS":
			###if DEBUG: print "------------ AXIS"
			ena, = struct.unpack(">h", data.read(2))
			my_dict['DISPLAXIS'] = ena
			sub2chunklen -= 2
		else:                                                       # Misc Chunks
			###if DEBUG: print "------------ SURF: BLOK: IMAP: %s: skipping" % sub2chunkname
			discard = data.read(sub2chunklen)
	#end loop on blok header subchunks
	##############################################################
	# blok attributes sub-chunk
	##############################################################
	subchunkname, = struct.unpack("4s", data.read(4))
	subchunklen, = struct.unpack(">h", data.read(2))
	accumulate_i += subchunklen + 6
	if subchunkname != 'TMAP':
		###if DEBUG: print "---------- SURF: BLOK: %s: block aborting" % subchunkname
		return {}, ""
	###if DEBUG: print "---------- TMAP"
	i = 0
	while(i < subchunklen): # -----------left 6----------------------- loop on header parameters
		sub2chunkname, = struct.unpack("4s", data.read(4))
		sub2chunklen, = struct.unpack(">h", data.read(2))
		i = i + 6 + sub2chunklen
		if sub2chunkname == "CNTR":
			###if DEBUG: print "------------ CNTR"
			x, y, z = struct.unpack(">fff", data.read(12))
			envelope, env_size = read_vx(data)
			my_dict['CNTR'] = [x, y, z]
			sub2chunklen -= (12+env_size)
		elif sub2chunkname == "SIZE":
			###if DEBUG: print "------------ SIZE"
			x, y, z = struct.unpack(">fff", data.read(12))
			envelope, env_size = read_vx(data)
			my_dict['SIZE'] = [x, y, z]
			sub2chunklen -= (12+env_size)
		elif sub2chunkname == "ROTA":
			###if DEBUG: print "------------ ROTA"
			x, y, z = struct.unpack(">fff", data.read(12))
			envelope, env_size = read_vx(data)
			my_dict['ROTA'] = [x, y, z]
			sub2chunklen -= (12+env_size)
		elif sub2chunkname == "CSYS":
			###if DEBUG: print "------------ CSYS"
			ena, = struct.unpack(">h", data.read(2))
			my_dict['CSYS'] = ena
			sub2chunklen -= 2
		else:                                                       # Misc Chunks
			###if DEBUG: print "------------ SURF: BLOK: TMAP: %s: skipping" % sub2chunkname
			pass
		if  sub2chunklen > 0:
			discard = data.read(sub2chunklen)
	#end loop on blok attributes subchunks
	##############################################################
	# ok, now other attributes without sub_chunks
	##############################################################
	while(accumulate_i < lenght): # ---------------------------------- loop on header parameters: lenght has already stripped the 6 bypes header
		subchunkname, = struct.unpack("4s", data.read(4))
		subchunklen, = struct.unpack(">H", data.read(2))
		accumulate_i = accumulate_i + 6 + subchunklen
		if subchunkname == "PROJ":
			###if DEBUG: print "---------- PROJ"
			p, = struct.unpack(">h", data.read(2))
			my_dict['PROJ'] = p
			subchunklen -= 2
		elif subchunkname == "AXIS":
			###if DEBUG: print "---------- AXIS"
			a, = struct.unpack(">h", data.read(2))
			my_dict['MAJAXIS'] = a
			subchunklen -= 2
		elif subchunkname == "IMAG":
			###if DEBUG: print "---------- IMAG"
			i, i_size = read_vx(data)
			my_dict['IMAG'] = i
			subchunklen -= i_size
		elif subchunkname == "WRAP":
			###if DEBUG: print "---------- WRAP"
			ww, wh = struct.unpack(">hh", data.read(4))
			#reduce width and height to just 1 parameter for both
			my_dict['WRAP'] = max([ww,wh])
			#my_dict['WRAPWIDTH'] = ww
			#my_dict['WRAPHEIGHT'] = wh
			subchunklen -= 4
		elif subchunkname == "WRPW":
			###if DEBUG: print "---------- WRPW"
			w, = struct.unpack(">f", data.read(4))
			my_dict['WRPW'] = w
			envelope, env_size = read_vx(data)
			subchunklen -= (env_size+4)
		elif subchunkname == "WRPH":
			###if DEBUG: print "---------- WRPH"
			w, = struct.unpack(">f", data.read(4))
			my_dict['WRPH'] = w
			envelope, env_size = read_vx(data)
			subchunklen -= (env_size+4)
		elif subchunkname == "VMAP":
			###if DEBUG: print "---------- VMAP"
			vmp, i = read_name(data)
			my_dict['VMAP'] = vmp
			my_uvname = vmp
			subchunklen -= i
		else:                                                    # Misc Chunks
			###if DEBUG: print "---------- SURF: BLOK: %s: skipping" % subchunkname
			pass
		if  subchunklen > 0:
			discard = data.read(subchunklen)
	#end loop on blok subchunks
	return my_dict, my_uvname


# =====================
# === Read Surfaces ===
# =====================
def read_surfs(lwochunk, surf_list, tag_list):
	my_dict = {}
	data = cStringIO.StringIO(lwochunk.read())
	surf_name, i = read_name(data)
	parent_name, j = read_name(data)
	i += j
	if (surf_name == "") or not(surf_name in tag_list):
		###if DEBUG: print "Reading SURF: Actually empty surf name not allowed. Skipping"
		return {}
	if (parent_name != ""):
		parent_index = [x['NAME'] for x in surf_list].count(parent_name)
		if parent_index >0:
			my_dict = surf_list[parent_index-1]
	my_dict['NAME'] = surf_name
	###if DEBUG: print "Surface data for TAG %s" % surf_name
	while(i < lwochunk.chunksize):
		subchunkname, = struct.unpack("4s", data.read(4))
		subchunklen, = struct.unpack(">H", data.read(2))
		i = i + 6 + subchunklen #6 bytes subchunk header
		if subchunkname == "COLR":                             #color: mapped on color
			###if DEBUG: print "-------- COLR"
			r, g, b = struct.unpack(">fff", data.read(12))
			envelope, env_size = read_vx(data)
			my_dict['COLR'] = [r, g, b]
			subchunklen -= (12+env_size)
		elif subchunkname == "DIFF":                           #diffusion: mapped on reflection (diffuse shader)
			###if DEBUG: print "-------- DIFF"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['DIFF'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "SPEC":                           #specularity: mapped to specularity (spec shader)
			###if DEBUG: print "-------- SPEC"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['SPEC'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "REFL":                           #reflection: mapped on raymirror
			###if DEBUG: print "-------- REFL"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['REFL'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "TRNL":                           #translucency: mapped on same param
			###if DEBUG: print "-------- TRNL"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['TRNL'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "GLOS":                           #glossiness: mapped on specularity hardness (spec shader)
			###if DEBUG: print "-------- GLOS"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['GLOS'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "TRAN":                           #transparency: inverted and mapped on alpha channel
			###if DEBUG: print "-------- TRAN"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['TRAN'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "LUMI":                           #luminosity: mapped on emit channel
			###if DEBUG: print "-------- LUMI"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['LUMI'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "GVAL":                           #glow: mapped on add channel
			###if DEBUG: print "-------- GVAL"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['GVAL'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "SMAN":                           #smoothing angle
			###if DEBUG: print "-------- SMAN"
			s, = struct.unpack(">f", data.read(4))
			my_dict['SMAN'] = s
			subchunklen -= 4
		elif subchunkname == "SIDE":                           #double sided?
			###if DEBUG: print "-------- SIDE"                             #if 1 side do not define key
			s, = struct.unpack(">H", data.read(2))
			if s == 3:
				my_dict['SIDE'] = s
			subchunklen -= 2
		elif subchunkname == "RIND":                           #Refraction: mapped on IOR
			###if DEBUG: print "-------- RIND"
			s, = struct.unpack(">f", data.read(4))
			envelope, env_size = read_vx(data)
			my_dict['RIND'] = s
			subchunklen -= (4+env_size)
		elif subchunkname == "BLOK":                           #blocks
			###if DEBUG: print "-------- BLOK"
			rr, uvname = read_surfblok(data.read(subchunklen))
			#paranoia setting: preventing adding an empty dict
			if rr: # != {}
				try:
					my_dict['BLOK'].append(rr)
				except:
					my_dict['BLOK'] = [rr]
					
			if uvname: # != "":
				my_dict['UVNAME'] = uvname                            #theoretically there could be a number of them: only one used per surf
			# all are dictionaries - so testing keys 
			if not('g_IMAG' in my_dict) and ('CHAN' in rr) and ('OPAC' in rr) and ('IMAG' in rr):
				if (rr['CHAN'] == 'COLR') and (rr['OPAC'] == 0):
					my_dict['g_IMAG'] = rr['IMAG']                 #do not set anything, just save image object for later assignment
			subchunklen = 0 #force ending
		else:                                                       # Misc Chunks
			pass
			###if DEBUG: print "-------- SURF:%s: skipping" % subchunkname
		if  subchunklen > 0:
			discard = data.read(subchunklen)
	#end loop on surf chunks
	try:#if my_dict.has_key('BLOK'):
	   my_dict['BLOK'].reverse() #texture applied in reverse order with respect to reading from lwo
	except:
		pass
	
	#uncomment this if material pre-allocated by read_surf
	my_dict['g_MAT'] = bpy.data.materials.new(my_dict['NAME'])
	###if DEBUG: print "-> Material pre-allocated."
	return my_dict

# =========================
# === Recalculate Faces ===
# =========================

def get_uvface(complete_list, facenum):
	# extract from the complete list only vertexes of the desired polygon
	'''
	my_facelist = []
	for elem in complete_list:
		if elem[0] == facenum:
			my_facelist.append(elem)
	return my_facelist
	'''
	return [elem for elem in complete_list if elem[0] == facenum]

def get_newindex(polygon_list, vertnum):
	# extract from the polygon list the new index associated to a vertex
	if not polygon_list: # == []
		return -1
	for elem in polygon_list:
		if elem[1] == vertnum:
			return elem[2]
	# ###if DEBUG: print "WARNING: expected vertex %s for polygon %s. Polygon_list dump follows" % (vertnum, polygon_list[0][0])
	# ###if DEBUG: print polygon_list
	return -1

def get_surf(surf_list, cur_tag):
	for elem in surf_list: # elem can be None
		if elem and elem['NAME'] == cur_tag:
			return elem
	return {}



# ====================================
# === Modified Create Blender Mesh ===
# ====================================
def my_create_mesh(clip_list, surf, objspec_list, current_facelist, objname, not_used_faces):
	#take the needed faces and update the not-used face list
	complete_vertlist = objspec_list[2]
	complete_facelist = objspec_list[3]
	uvcoords_dict = objspec_list[7]
	facesuv_dict = objspec_list[8]
	vertex_map = {} #implementation as dict
	cur_ptag_faces = []
	cur_ptag_faces_indexes = []
	maxface = len(complete_facelist)
	for ff in current_facelist:
		if ff >= maxface:
			###if DEBUG: print "Non existent face addressed: Giving up with this object"
			return None, not_used_faces              #return the created object
		cur_face = complete_facelist[ff]
		cur_ptag_faces_indexes.append(ff)
		if not_used_faces: # != []
			not_used_faces[ff] = -1
		for vv in cur_face: vertex_map[vv] = 1
	#end loop on faces
	store_edge = 0
	
	scn= bpy.data.scenes.active
	msh = bpy.data.meshes.new()
	obj = scn.objects.new(msh)
	
	mat = None
	try:
		msh.materials = [surf['g_MAT']]
	except:
		pass
	
	msh.mode |= Blender.Mesh.Modes.AUTOSMOOTH #smooth it anyway
	if 'SMAN' in surf: # has_key
		#not allowed mixed mode mesh (all the mesh is smoothed and all with the same angle)
		#only one smoothing angle will be active! => take the max one
		msh.degr = min(80, int(surf['SMAN']/3.1415926535897932384626433832795*180.0))     #lwo in radians - blender in degrees
	
	try:
		img= lookup_imag(clip_list, surf['g_IMAG'])['g_IMG']
	except:
		img= None
	
	#uv_flag = ((surf.has_key('UVNAME')) and (uvcoords_dict.has_key(surf['UVNAME'])) and (img != None))
	uv_flag = (('UVNAME' in surf) and (surf['UVNAME'] in uvcoords_dict))

	###if DEBUG: print "\n#===================================================================#"
	###if DEBUG: print "Processing Object: %s" % objname
	###if DEBUG: print "#===================================================================#"
	
	if uv_flag:
		msh.verts.extend([(0.0,0.0,0.0),])
		j = 1
	else:
		j = 0
	
	def tmp_get_vert(k, i):
		vertex_map[k] = i+j # j is the dummy vert
		# ###if DEBUG: print complete_vertlist[i]
		return complete_vertlist[k]
	
	
	
	msh.verts.extend([tmp_get_vert(k, i) for i, k in enumerate(vertex_map.iterkeys())])
	msh.transform(TXMTX)					# faster then applying while reading.
	#end sweep over vertexes

	#append faces
	FACE_TEX= Blender.Mesh.FaceModes.TEX
	FACE_ALPHA= Blender.Mesh.FaceTranspModes.ALPHA
	EDGE_DRAW_FLAG= Blender.Mesh.EdgeFlags.EDGEDRAW | Blender.Mesh.EdgeFlags.EDGERENDER
	
	
	edges = []
	face_data = [] # [(indicies, material, uvs, image), ]
	face_uvs = []
	edges_fgon = []
	
	if uv_flag:
		uvcoords_dict_context = uvcoords_dict[surf['UVNAME']]
		try:	current_uvdict = facesuv_dict[surf['UVNAME']]
		except:	current_uvdict = None
		
	default_uv = Blender.Mathutils.Vector(0,0)
	def tmp_get_face_uvs(cur_face, i):
		uvs = []
		if current_uvdict:
			uvface = get_uvface(current_uvdict,i)
			for vi in cur_face:
				ni = get_newindex(uvface, vi)
				if ni == -1: ni = vi
				
				try:
					uvs.append(uvcoords_dict_context[ ni ])
				except:
					###if DEBUG: print '\tWarning, Corrupt UVs'
					uvs.append(default_uv)
		else:
			for vi in cur_face:
				try:
					uvs.append(uvcoords_dict_context[ vi ])
				except:
					###if DEBUG: print '\tWarning, Corrupt UVs'
					uvs.append(default_uv)
		
		return uvs
	cur_face
	for i in cur_ptag_faces_indexes:
		cur_face = complete_facelist[i]
		numfaceverts = len(cur_face)
		
		if numfaceverts == 2:		edges.append((vertex_map[cur_face[0]], vertex_map[cur_face[1]]))
		elif numfaceverts == 3 or numfaceverts == 4:	
			rev_face = [__i for __i in reversed(cur_face)]
			face_data.append( [vertex_map[j] for j in rev_face] )
			if uv_flag: face_uvs.append(tmp_get_face_uvs(rev_face, i))
		elif numfaceverts > 4:
			meta_faces= BPyMesh.ngon(complete_vertlist, cur_face, PREF_FIX_LOOPS= True)
			edge_face_count = {}
			for mf in meta_faces:
				# These will always be tri's since they are scanfill faces
				mf = cur_face[mf[2]], cur_face[mf[1]], cur_face[mf[0]]
				face_data.append( [vertex_map[j] for j in mf] )
				
				if uv_flag: face_uvs.append(tmp_get_face_uvs(mf, i))
				
				#if USE_FGON:
				if len(meta_faces) > 1:
					mf = face_data[-1] # reuse mf
					for j in xrange(3):
						v1= mf[j]
						v2= mf[j-1]
						if v1!=v2:
							if v1>v2:
								v2,v1= v1,v2
							try:
								edge_face_count[v1,v2]+= 1
							except:
								edge_face_count[v1,v2]= 0
				

			
			if edge_face_count:
				edges_fgon.extend( [vert_key for vert_key, count in edge_face_count.iteritems() if count] )
	
	if edges:
		msh.edges.extend(edges)
	
	face_mapping_removed = msh.faces.extend(face_data, indexList=True)
	if 'TRAN' in surf or (mat and mat.alpha<1.0): # incase mat is null
		transp_flag = True
	else:
		transp_flag = False
	
	if uv_flag:
		msh.faceUV = True
		msh_faces= msh.faces
		for i, uvs in enumerate(face_uvs):
			i_mapped = face_mapping_removed[i]
			if i_mapped != None:
				f = msh_faces[i_mapped]
				f.uv = uvs
				if img:
					f.image = img
				
				if transp_flag: f.transp |= FACE_ALPHA
	
	if edges_fgon:
		msh_edges = msh.edges
		FGON= Blender.Mesh.EdgeFlags.FGON
		edges_fgon = msh.findEdges( edges_fgon )
		if type(edges_fgon) != list: edges_fgon = [edges_fgon]
		for ed in edges_fgon:
			if ed!=None:
				msh_edges[ed].flag |= FGON
	
	if not(uv_flag):        #clear eventual UV data
		msh.faceUV = False
	
	if uv_flag:
		msh.verts.delete([0,])
	
	return obj, not_used_faces              #return the created object


# ============================================
# === Set Subsurf attributes on given mesh ===
# ============================================
def set_subsurf(obj):
	mods = obj.modifiers                      # get the object's modifiers
	mod = mods.append(Blender.Modifier.Type.SUBSURF)  # add a new subsurf modifier
	mod[Blender.Modifier.Settings.LEVELS] = 2         # set subsurf subdivision levels to 2
	mod[Blender.Modifier.Settings.RENDLEVELS] = 2     # set subsurf rendertime subdivision levels to 2
	obj.makeDisplayList()


# =================================
# === object size and dimension ===
# =================================
def obj_size_pos(obj):
	bbox = obj.getBoundBox()
	bbox_min = map(lambda *row: min(row), *bbox) #transpose & get min
	bbox_max = map(lambda *row: max(row), *bbox) #transpose & get max
	obj_size = (bbox_max[0]-bbox_min[0], bbox_max[1]-bbox_min[1], bbox_max[2]-bbox_min[2])
	obj_pos = ( (bbox_max[0]+bbox_min[0]) / 2, (bbox_max[1]+bbox_min[1]) / 2, (bbox_max[2]+bbox_min[2]) / 2)
	return (obj_size, obj_pos)


# =========================
# === Create the object ===
# =========================
def create_objects(clip_list, objspec_list, surf_list):
	nf = len(objspec_list[3])
	not_used_faces = range(nf)
	ptag_dict = objspec_list[5]
	obj_dict = {}  #links tag names to object, used for material assignments
	obj_dim_dict = {}
	obj_list = []  #have it handy for parent association
	middlechar = "+"
	endchar = ""
	if (objspec_list[6] == 1):
		middlechar = endchar = "#"
	for cur_tag, ptag_dict_val in ptag_dict.iteritems():
		if ptag_dict_val != []:
			cur_surf = get_surf(surf_list, cur_tag)
			cur_obj, not_used_faces=  my_create_mesh(clip_list, cur_surf, objspec_list, ptag_dict_val, objspec_list[0][:9]+middlechar+cur_tag[:9], not_used_faces)
			# Works now with new modifiers
			if objspec_list[6] == 1:
				set_subsurf(cur_obj)
			if cur_obj: # != None
				obj_dict[cur_tag] = cur_obj
				obj_dim_dict[cur_tag] = obj_size_pos(cur_obj)
				obj_list.append(cur_obj)
	#end loop on current group
	#and what if some faces not used in any named PTAG? get rid of unused faces
	orphans = []
	for tt in not_used_faces:
		if tt > -1: orphans.append(tt)
	#end sweep on unused face list
	not_used_faces = None
	if orphans: # != []
		cur_surf = get_surf(surf_list, "_Orphans")
		cur_obj, not_used_faces = my_create_mesh(clip_list, cur_surf, objspec_list, orphans, objspec_list[0][:9]+middlechar+"Orphans", [])
		if cur_obj: # != None
			if objspec_list[6] == 1:
				set_subsurf(cur_obj)
			obj_dict["_Orphans"] = cur_obj
			obj_dim_dict["_Orphans"] = obj_size_pos(cur_obj)
			obj_list.append(cur_obj)
	objspec_list[1]= obj_dict
	objspec_list[4]= obj_dim_dict
	
	return



# ===========================================
# === Lookup for image index in clip_list ===
# ===========================================
def lookup_imag(clip_list, ima_id):
	for ii in clip_list:
		if ii and ii['ID'] == ima_id:
			if 'XREF' in ii: # has_key
				#cross reference - recursively look for images
				return lookup_imag(clip_list, ii['XREF'])
			else:
				return ii
	return None


# ===================================================
# === Create and assign image mapping to material ===
# ===================================================
def create_blok(surf, mat, clip_list, obj_size, obj_pos):

	def output_size_ofs(size, pos, blok):
		#just automate repetitive task
		# 0 == X, 1 == Y, 2 == Z
		size_default = [1.0] * 3
		size2 = [1.0] * 3
		ofs_default = [0.0] * 3
		offset = [1.0] * 3
		axis_default = [Blender.Texture.Proj.X, Blender.Texture.Proj.Y, Blender.Texture.Proj.Z]
		axis = [1.0] * 3
		c_map_txt = ["    X--", "    -Y-", "    --Z"]
		c_map = [0,1,2]             # standard, good for Z axis projection
		if blok['MAJAXIS'] == 0:
			c_map = [1,2,0]         # X axis projection
		if blok['MAJAXIS'] == 2:
			c_map = [0,2,1]         # Y axis projection

		###if DEBUG: print "!!!axis mapping:"
		#this is the smart way
		###if DEBUG: for mp in c_map: print c_map_txt[mp]

		if blok['SIZE'][0] != 0.0:          #paranoia controls
			size_default[0] = (size[0]/blok['SIZE'][0])
			ofs_default[0] = ((blok['CNTR'][0]-pos[0])/blok['SIZE'][0])
		if blok['SIZE'][1] != 0.0:
			size_default[2] = (size[2]/blok['SIZE'][1])
			ofs_default[2] = ((blok['CNTR'][1]-pos[2])/blok['SIZE'][1])
		if blok['SIZE'][2] != 0.0:
			size_default[1] = (size[1]/blok['SIZE'][2])
			ofs_default[1] = ((blok['CNTR'][2]-pos[1])/blok['SIZE'][2])

		for mp in xrange(3):
			axis[mp] = axis_default[c_map[mp]]
			size2[mp] = size_default[c_map[mp]]
			offset[mp] = ofs_default[c_map[mp]]
			if offset[mp]>10.0: offset[mp]-10.0
			if offset[mp]<-10.0: offset[mp]+10.0
#        size = [size_default[mp] for mp in c_map]

		###if DEBUG: print "!!!texture size and offsets:"
		###if DEBUG: print "    sizeX = %.5f; sizeY = %.5f; sizeZ = %.5f" % (size[0],size[1],size[2])
		###if DEBUG: print "    ofsX = %.5f; ofsY = %.5f; ofsZ = %.5f" % (offset[0],offset[1],offset[2])
		return axis, size2, offset

	ti = 0
	alphaflag = 0 #switched to 1 if some tex in this block is using alpha
	lastimag = 0 #experimental ....
	for blok in surf['BLOK']:
		###if DEBUG: print "#...................................................................#"
		###if DEBUG: print "# Processing texture block no.%s for surf %s" % (ti,surf['NAME'])
		###if DEBUG: print "#...................................................................#"
		# tobj.pdict (blok)
		if ti > 9: break                                    #only 8 channels 0..7 allowed for texture mapping
		#if not blok['ENAB']:
		#    ###if DEBUG: print "***Image is not ENABled! Quitting this block"
		#    break
		if not('IMAG' in blok): # has_key
			###if DEBUG: print "***No IMAGE for this block? Quitting"
			break                 #extract out the image index within the clip_list
		if blok['IMAG'] == 0: blok['IMAG'] = lastimag #experimental ....
		###if DEBUG: print "looking for image number %d" % blok['IMAG']
		ima = lookup_imag(clip_list, blok['IMAG'])
		if ima == None:
			###if DEBUG: print "***Block index image not within CLIP list? Quitting Block"
			break                              #safety check (paranoia setting)
		img = ima['g_IMG']
		lastimag = blok['IMAG']  #experimental ....
		if img == None:
			###if DEBUG: print "***Failed to pre-allocate image %s found: giving up" % ima['BASENAME']
			break
		tname = str(ima['ID'])
		if blok['ENAB']:
			tname += "+"
		else:
			tname += "x" #let's signal when should not be enabled
		if 'CHAN' in blok: # has_key
			tname += blok['CHAN']
		newtex = bpy.data.textures.new(tname)
		newtex.setType('Image')                 # make it anu image texture
		newtex.image = img
		#how does it extends beyond borders
		if 'WRAP' in blok: # has_key
			if (blok['WRAP'] == 3) or (blok['WRAP'] == 2):
				newtex.setExtend('Extend')
			elif (blok['WRAP'] == 1):
				newtex.setExtend('Repeat')
			elif (blok['WRAP'] == 0):
				newtex.setExtend('Clip')
		###if DEBUG: print "generated texture %s" % tname

		#MapTo is determined by CHAN parameter
		#assign some defaults
		colfac = 1.0
		dvar = 1.0
		norfac = 0.5
		nega = False
		mapflag = Blender.Texture.MapTo.COL  #default to color
		maptype = Blender.Texture.Mappings.FLAT
		if 'CHAN' in blok: # has_key
			if blok['CHAN'] == 'COLR' and 'OPACVAL' in blok: # has_key
				colfac = blok['OPACVAL']
				# Blender needs this to be clamped
				colfac = max(0.0, min(1.0, colfac))
				###if DEBUG: print "!!!Set Texture -> MapTo -> Col = %.3f" % colfac
			if blok['CHAN'] == 'BUMP':
				mapflag = Blender.Texture.MapTo.NOR
				if 'OPACVAL' in blok: norfac = blok['OPACVAL'] # has_key
				###if DEBUG: print "!!!Set Texture -> MapTo -> Nor = %.3f" % norfac
			if blok['CHAN'] == 'LUMI':
				mapflag = Blender.Texture.MapTo.EMIT
				if 'OPACVAL' in blok: dvar = blok['OPACVAL'] # has_key
				###if DEBUG: print "!!!Set Texture -> MapTo -> DVar = %.3f" % dvar
			if blok['CHAN'] == 'DIFF':
				mapflag = Blender.Texture.MapTo.REF
				if 'OPACVAL' in blok: dvar = blok['OPACVAL'] # has_key
				###if DEBUG: print "!!!Set Texture -> MapTo -> DVar = %.3f" % dvar
			if blok['CHAN'] == 'SPEC':
				mapflag = Blender.Texture.MapTo.SPEC
				if 'OPACVAL' in blok: dvar = blok['OPACVAL'] # has_key
				###if DEBUG: print "!!!Set Texture -> MapTo -> DVar = %.3f" % dvar
			if blok['CHAN'] == 'TRAN':
				mapflag = Blender.Texture.MapTo.ALPHA
				if 'OPACVAL' in blok: dvar = blok['OPACVAL'] # has_key
				###if DEBUG: print "!!!Set Texture -> MapTo -> DVar = %.3f" % dvar
				alphaflag = 1
				nega = True
		if 'NEGA' in blok: # has_key
			###if DEBUG: print "!!!Watch-out: effect of this texture channel must be INVERTED!"
			nega = not nega

		blendmode_list = ['Mix',
						 'Subtractive',
						 'Difference',
						 'Multiply',
						 'Divide',
						 'Mix with calculated alpha layer and stencil flag',
						 'Texture Displacement',
						 'Additive']
		set_blendmode = 7 #default additive
		if 'OPAC' in blok: # has_key
			set_blendmode = blok['OPAC']
		if set_blendmode == 5: #transparency
			newtex.imageFlags |= Blender.Texture.ImageFlags.CALCALPHA
			if nega: newtex.flags |= Blender.Texture.Flags.NEGALPHA
		###if DEBUG: print "!!!Set Texture -> MapTo -> Blending Mode = %s" % blendmode_list[set_blendmode]

		#the TexCo flag is determined by PROJ parameter
		axis = [Blender.Texture.Proj.X, Blender.Texture.Proj.Y, Blender.Texture.Proj.Z]
		size = [1.0] * 3
		ofs = [0.0] * 3
		if 'PROJ' in blok: # has_key
			if blok['PROJ'] == 0: #0 - Planar
				###if DEBUG: print "!!!Flat projection"
				coordflag = Blender.Texture.TexCo.ORCO
				maptype = Blender.Texture.Mappings.FLAT
			elif blok['PROJ'] == 1: #1 - Cylindrical
				###if DEBUG: print "!!!Cylindrical projection"
				coordflag = Blender.Texture.TexCo.ORCO
				maptype = Blender.Texture.Mappings.TUBE
			elif blok['PROJ'] == 2: #2 - Spherical
				###if DEBUG: print "!!!Spherical projection"
				coordflag = Blender.Texture.TexCo.ORCO
				maptype = Blender.Texture.Mappings.SPHERE
			elif blok['PROJ'] == 3: #3 - Cubic
				###if DEBUG: print "!!!Cubic projection"
				coordflag = Blender.Texture.TexCo.ORCO
				maptype = Blender.Texture.Mappings.CUBE
			elif blok['PROJ'] == 4: #4 - Front Projection
				###if DEBUG: print "!!!Front projection"
				coordflag = Blender.Texture.TexCo.ORCO
				maptype = Blender.Texture.Mappings.FLAT # ??? could it be a FLAT with some other TexCo type?
			elif blok['PROJ'] == 5: #5 - UV
				###if DEBUG: print "UVMapped"
				coordflag = Blender.Texture.TexCo.UV
				maptype = Blender.Texture.Mappings.FLAT  #in case of UV default to FLAT mapping => effectively not used
			if blok['PROJ'] != 5: #This holds for any projection map except UV
				axis, size, ofs = output_size_ofs(obj_size, obj_pos, blok)
				
				# Clamp ofs and size else blender will raise an error
				for ii in xrange(3):
					ofs[ii]= min(10.0, max(-10, ofs[ii]))
					size[ii]= min(100, max(-100, size[ii]))

		mat.setTexture(ti, newtex, coordflag, mapflag)
		current_mtex = mat.getTextures()[ti]
		current_mtex.mapping = maptype
		current_mtex.colfac = colfac
		current_mtex.dvar = dvar
		current_mtex.norfac = norfac
		current_mtex.neg = nega
		current_mtex.xproj = axis[0]
		current_mtex.yproj = axis[1]
		current_mtex.zproj = axis[2]
		current_mtex.size = tuple(size)
		current_mtex.ofs = tuple(ofs)
		if (set_blendmode == 5): #transparency
			current_mtex.stencil = not (nega)

		ti += 1
	#end loop over bloks
	return alphaflag


# ========================================
# === Create and assign a new material ===
# ========================================
#def update_material(surf_list, ptag_dict, obj, clip_list, uv_dict, dir_part):
def update_material(clip_list, objspec, surf_list):
	if (surf_list == []) or (objspec[5] == {}) or (objspec[1] == {}):
		###if DEBUG: print "something getting wrong in update_material: dump follows  ..."
		###if DEBUG: print surf_list
		###if DEBUG: print objspec[5]
		###if DEBUG: print objspec[1]
		return
	obj_dict = objspec[1]
	all_faces = objspec[3]
	obj_dim_dict = objspec[4]
	ptag_dict = objspec[5]
	uvcoords_dict = objspec[7]
	facesuv_dict = objspec[8]
	for surf in surf_list:
		if surf and surf['NAME'] in ptag_dict: # in ptag_dict.keys()
			###if DEBUG: print "#-------------------------------------------------------------------#"
			###if DEBUG: print "Processing surface (material): %s" % surf['NAME']
			###if DEBUG: print "#-------------------------------------------------------------------#"
			#material set up
			facelist = ptag_dict[surf['NAME']]
			#bounding box and position
			cur_obj = obj_dict[surf['NAME']]
			obj_size = obj_dim_dict[surf['NAME']][0]
			obj_pos = obj_dim_dict[surf['NAME']][1]
			###if DEBUG: print surf
			#uncomment this if material pre-allocated by read_surf
			mat = surf['g_MAT']
			if mat == None:
				###if DEBUG: print "Sorry, no pre-allocated material to update. Giving up for %s." % surf['NAME']
				break
			#mat = Blender.Material.New(surf['NAME'])
			#surf['g_MAT'] = mat
			if 'COLR' in surf: # has_key
				mat.rgbCol = surf['COLR']
			if 'LUMI' in surf:
				mat.setEmit(surf['LUMI'])
			if 'GVAL' in surf: # has_key
				mat.setAdd(surf['GVAL'])
			if 'SPEC' in surf: # has_key
				mat.setSpec(surf['SPEC'])							#it should be * 2 but seems to be a bit higher lwo [0.0, 1.0] - blender [0.0, 2.0]
			if 'DIFF' in surf: # has_key
				mat.setRef(surf['DIFF'])							#lwo [0.0, 1.0] - blender [0.0, 1.0]
			if 'GLOS' in surf: # has_key							#lwo [0.0, 1.0] - blender [0, 255]
				glo = int(371.67 * surf['GLOS'] - 42.334)			#linear mapping - seems to work better than exp mapping
				if glo <32:  glo = 32								#clamped to 32-255
				if glo >255: glo = 255
				mat.setHardness(glo)
			if 'TRNL' in surf: # has_key
				mat.setTranslucency(surf['TRNL'])                #NOT SURE ABOUT THIS lwo [0.0, 1.0] - blender [0.0, 1.0]

			mm = mat.mode
			mm |= Blender.Material.Modes.TRANSPSHADOW
			if 'REFL' in surf: # has_key
				mat.setRayMirr(surf['REFL'])                     #lwo [0.0, 1.0] - blender [0.0, 1.0]
				mm |= Blender.Material.Modes.RAYMIRROR
			if 'TRAN' in surf: # has_key
				mat.setAlpha(1.0-surf['TRAN'])                                        #lwo [0.0, 1.0] - blender [1.0, 0.0]
				mm |= Blender.Material.Modes.RAYTRANSP
			if 'RIND' in surf: # has_key
				s = surf['RIND']
				if s < 1.0: s = 1.0
				if s > 3.0: s = 3.0
				mat.setIOR(s)                                                         #clipped to blender [1.0, 3.0]
				mm |= Blender.Material.Modes.RAYTRANSP
			if 'BLOK' in surf and surf['BLOK'] != []:
				#update the material according to texture.
				alphaflag = create_blok(surf, mat, clip_list, obj_size, obj_pos)
				if alphaflag:
					mm |= Blender.Material.Modes.RAYTRANSP
			mat.mode = mm
			#finished setting up the material
		#end if exist SURF
	#end loop on materials (SURFs)
	return


# ======================
# === Read Faces 6.0 ===
# ======================
def read_faces_6(lwochunk):
	data = cStringIO.StringIO(lwochunk.read())
	faces = []
	polygon_type = data.read(4)
	subsurf = 0
	if polygon_type != "FACE" and polygon_type != "PTCH":
		###if DEBUG: print "No FACE/PATCH Were Found. Polygon Type: %s" % polygon_type
		return "", 2
	if polygon_type == 'PTCH': subsurf = 1
	i = 0
	while(i < lwochunk.chunksize-4):
		#if not i%1000 and my_meshtools.show_progress:
		#	Blender.Window.DrawProgressBar(float(i)/lwochunk.chunksize, "Reading Faces")
		facev = []
		numfaceverts, = struct.unpack(">H", data.read(2))
		i += 2

		for j in xrange(numfaceverts):
			index, index_size = read_vx(data)
			i += index_size
			facev.append(index)
		faces.append(facev)
	###if DEBUG: print "read %s faces; type of block %d (0=FACE; 1=PATCH)" % (len(faces), subsurf)
	return faces, subsurf

def main():
	if not struct:
		Blender.Draw.PupMenu('This importer requires a full python install')
		return
	
	Blender.Window.FileSelector(read, "Import LWO", '*.lwo')

if __name__=='__main__':
	main()


# Cams debugging lwo loader
"""
TIME= Blender.sys.time()
import os
print 'Searching for files'
os.system('find /fe/lwo/Objects/ -follow -iname "*.lwo" > /tmp/templwo_list')
# os.system('find /storage/ -iname "*.lwo" > /tmp/templwo_list')
print '...Done'
file= open('/tmp/templwo_list', 'r')
lines= file.readlines()

# sort by filesize for faster testing
lines_size = [(os.path.getsize(f[:-1]), f[:-1]) for f in lines]
lines_size.sort()
lines = [f[1] for f in lines_size]

file.close()

def between(v,a,b):
	if v <= max(a,b) and v >= min(a,b):
		return True
		
	return False
size= 0.0
for i, _lwo in enumerate(lines):
	#if i==425:	 # SCANFILL
	#if 1:
	#if i==520:	 # SCANFILL CRASH
	#if i==47:	 # SCANFILL CRASH
	#if between(i, 525, 550):
	#if i > 1635:
	#if i != 1519: # 730
	if i>141:
		#if 1:
		# _lwo= _lwo[:-1]
		print 'Importing', _lwo, '\nNUMBER', i, 'of', len(lines)
		_lwo_file= _lwo.split('/')[-1].split('\\')[-1]
		newScn= bpy.data.scenes.new(_lwo_file)
		bpy.data.scenes.active = newScn
		size += ((os.path.getsize(_lwo)/1024.0))/ 1024.0
		read(_lwo)
		# Remove objects to save memory?
		'''
		for ob in newScn.objects:
			if ob.type=='Mesh':
				me= ob.getData(mesh=1)
				me.verts= None
			newScn.unlink(ob)
		'''
		print 'mb size so far', size

print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)
"""