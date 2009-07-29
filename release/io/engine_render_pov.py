import bpy

from math import atan, pi, degrees
import subprocess
import os
import sys
import time

import platform as pltfrm

if pltfrm.architecture()[0] == '64bit':
	bitness = 64
else:
	bitness = 32

def write_pov(filename, scene=None, info_callback = None):
	file = open(filename, 'w')
	
	# Only for testing
	if not scene:
		scene = bpy.data.scenes[0]
		
	render = scene.render_data
	materialTable = {}
	
	def saneName(name):
		name = name.lower()
		for ch in ' /\\+=-[]{}().,<>\'":;~!@#$%^&*|?':
			name = name.replace(ch, '_')
		return name
	
	def writeMatrix(matrix):
		file.write('\tmatrix <%.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f>\n' %\
		(matrix[0][0], matrix[0][1], matrix[0][2],  matrix[1][0], matrix[1][1], matrix[1][2],  matrix[2][0], matrix[2][1], matrix[2][2],  matrix[3][0], matrix[3][1], matrix[3][2]) )
	
	def exportCamera():
		camera = scene.camera
		matrix = camera.matrix
		
		# compute resolution
		Qsize=float(render.resolution_x)/float(render.resolution_y)
		
		file.write('camera {\n')
		file.write('\tlocation  <0, 0, 0>\n')
		file.write('\tlook_at  <0, 0, -1>\n')
		file.write('\tright <%s, 0, 0>\n' % -Qsize)
		file.write('\tup <0, 1, 0>\n')
		file.write('\tangle  %f \n' % (360.0*atan(16.0/camera.data.lens)/pi))
		
		file.write('\trotate  <%.6f, %.6f, %.6f>\n' % tuple([degrees(e) for e in matrix.rotationPart().toEuler()]))
		file.write('\ttranslate <%.6f, %.6f, %.6f>\n' % (matrix[3][0], matrix[3][1], matrix[3][2]))
		file.write('}\n')
	
	
	
	def exportLamps(lamps):
		# Get all lamps
		for ob in lamps:
			lamp = ob.data
			
			matrix = ob.matrix
			
			color = tuple([c * lamp.energy for c in lamp.color]) # Colour is modified by energy
			
			file.write('light_source {\n')
			file.write('\t< 0,0,0 >\n')
			file.write('\tcolor red %.6f green %.6f blue %.6f\n' % color)
			
			if lamp.type == 'POINT': # Point Lamp 
				pass
			elif lamp.type == 'SPOT': # Spot
				file.write('\tspotlight\n')
				
				# Falloff is the main radius from the centre line
				file.write('\tfalloff %.2f\n' % (lamp.spot_size/2.0) ) # 1 TO 179 FOR BOTH
				file.write('\tradius %.6f\n' % ((lamp.spot_size/2.0) * (1-lamp.spot_blend)) ) 
				
				# Blender does not have a tightness equivilent, 0 is most like blender default.
				file.write('\ttightness 0\n') # 0:10f
				
				file.write('\tpoint_at  <0, 0, -1>\n')
			elif lamp.type == 'AREA':
				
				size_x = lamp.size
				samples_x = lamp.shadow_ray_samples_x
				if lamp.shape == 'SQUARE':
					size_y = size_x
					samples_y = samples_x
				else:
					size_y = lamp.size_y
					samples_y = lamp.shadow_ray_samples_y
				
				
				
				file.write('\tarea_light <%d,0,0>,<0,0,%d> %d, %d\n' % (size_x, size_y, samples_x, samples_y))
				if lamp.shadow_ray_sampling_method == 'CONSTANT_JITTERED':
					if lamp.jitter:
						file.write('\tjitter\n')
				else:
					file.write('\tadaptive 1\n')
					file.write('\tjitter\n')
			
			if lamp.shadow_method == 'NOSHADOW':
				file.write('\tshadowless\n')	
			
			file.write('\tfade_distance %.6f\n' % lamp.distance)
			file.write('\tfade_power %d\n' % 1) # Could use blenders lamp quad?
			writeMatrix(matrix)
			
			file.write('}\n')
	
	def exportMeshs(sel):
		def bMat2PovString(material):
			povstring = 'finish {'
			if world != None:
				povstring += 'ambient <%.6f, %.6f, %.6f> ' % tuple([c*material.ambient for c in world.ambient_color])
			
			povstring += 'diffuse %.6f ' % material.diffuse_reflection
			povstring += 'specular %.6f ' % material.specular_reflection
			
			
			if material.raytrace_mirror.enabled:
				#povstring += 'interior { ior %.6f } ' % material.IOR
				raytrace_mirror= material.raytrace_mirror
				if raytrace_mirror.reflect:
					povstring += 'reflection {'
					povstring += '<%.6f, %.6f, %.6f>' % tuple(material.mirror_color) # Should ask for ray mirror flag
					povstring += 'fresnel 1 falloff %.6f exponent %.6f metallic %.6f} ' % (raytrace_mirror.fresnel, raytrace_mirror.fresnel_fac, raytrace_mirror.reflect)
				
				
					
			if material.raytrace_transparency.enabled:
				#povstring += 'interior { ior %.6f } ' % material.IOR
				pass
			
			#file.write('\t\troughness %.6f\n' % (material.hard*0.5))
			#file.write('\t\t\tcrand 0.0\n') # Sand granyness
			#file.write('\t\t\tmetallic %.6f\n' % material.spec)
			#file.write('\t\t\tphong %.6f\n' % material.spec)
			#file.write('\t\t\tphong_size %.6f\n' % material.spec)
			povstring += 'brilliance %.6f ' % (material.specular_hardness/256.0) # Like hardness
			povstring += '}'
			#file.write('\t}\n')
			return povstring
			
		
		world = scene.world
		
		# Convert all materials to strings we can access directly per vertex.
		for material in bpy.data.materials:
			materialTable[material.name] = bMat2PovString(material)
		
		
		ob_num = 0
		
		for ob in sel:
			ob_num+= 1
			
			if ob.type in ('LAMP', 'CAMERA', 'EMPTY'):
				continue
			
			me = ob.data
			me_materials= me.materials
			
			me = ob.create_render_mesh(scene)
			
			if not me:
				continue
			
			if info_callback:
				info_callback('Object %2.d of %2.d (%s)' % (ob_num, len(sel), ob.name))
			
			#if ob.type!='MESH':
			#	continue
			# me = ob.data
			
			matrix = ob.matrix
			try:	uv_layer = me.active_uv_texture.data
			except:uv_layer = None
				
			try:	vcol_layer = me.active_vertex_color.data
			except:vcol_layer = None
			
			
			def regular_face(f):
				fv = f.verts
				if fv[3]== 0:
					return fv[0], fv[1], fv[2]
				return fv[0], fv[1], fv[2], fv[3]
			
			faces_verts = [regular_face(f) for f in me.faces]
			faces_normals = [tuple(f.normal) for f in me.faces]
			verts_normals = [tuple(v.normal) for v in me.verts]
			
			# quads incur an extra face
			quadCount = len([f for f in faces_verts if len(f)==4])
			
			file.write('mesh2 {\n')
			file.write('\tvertex_vectors {\n')
			file.write('\t\t%s' % (len(me.verts))) # vert count
			for v in me.verts:
				file.write(',\n\t\t<%.6f, %.6f, %.6f>' % tuple(v.co)) # vert count
			file.write('\n  }\n')
			
			
			# Build unique Normal list
			uniqueNormals = {}
			for fi, f in enumerate(me.faces):
				fv = faces_verts[fi]
				# [-1] is a dummy index, use a list so we can modify in place
				if f.smooth: # Use vertex normals
					for v in fv:
						key = verts_normals[v]
						uniqueNormals[key] = [-1]
				else: # Use face normal
					key = faces_normals[fi]
					uniqueNormals[key] = [-1]
			
			file.write('\tnormal_vectors {\n')
			file.write('\t\t%d' % len(uniqueNormals)) # vert count
			idx = 0
			for no, index in uniqueNormals.items():
				file.write(',\n\t\t<%.6f, %.6f, %.6f>' % no) # vert count
				index[0] = idx
				idx +=1
			file.write('\n  }\n')
			
			
			# Vertex colours
			vertCols = {} # Use for material colours also.
			
			if uv_layer:
				# Generate unique UV's
				uniqueUVs = {}
				
				for fi, uv in enumerate(uv_layer):
					
					if len(faces_verts[fi])==4:
						uvs = uv.uv1, uv.uv2, uv.uv3, uv.uv4
					else:
						uvs = uv.uv1, uv.uv2, uv.uv3
					
					for uv in uvs:
						uniqueUVs[tuple(uv)] = [-1]
				
				file.write('\tuv_vectors {\n')
				#print unique_uvs
				file.write('\t\t%s' % (len(uniqueUVs))) # vert count
				idx = 0
				for uv, index in uniqueUVs.items():
					file.write(',\n\t\t<%.6f, %.6f>' % uv)
					index[0] = idx
					idx +=1
				'''
				else:
					# Just add 1 dummy vector, no real UV's
					file.write('\t\t1') # vert count
					file.write(',\n\t\t<0.0, 0.0>')
				'''
				file.write('\n  }\n')
			
			
			if me.vertex_colors:
				
				for fi, f in enumerate(me.faces):
					material_index = f.material_index
					material = me_materials[material_index]
					
					if material and material.vertex_color_paint:
						
						col = vcol_layer[fi]
						
						if len(faces_verts[fi])==4:
							cols = col.color1, col.color2, col.color3, col.color4
						else:
							cols = col.color1, col.color2, col.color3
						
						for col in cols:					
							key = col[0], col[1], col[2], material_index # Material index!
							vertCols[key] = [-1]
						
					else:
						if material:
							diffuse_color = tuple(material.diffuse_color)
							key = diffuse_color[0], diffuse_color[1], diffuse_color[2], material_index
							vertCols[key] = [-1]
						
			
			else:
				# No vertex colours, so write material colours as vertex colours
				for i, material in enumerate(me_materials):
					
					if material:
						diffuse_color = tuple(material.diffuse_color)
						key = diffuse_color[0], diffuse_color[1], diffuse_color[2], i # i == f.mat
						vertCols[key] = [-1]
				
			
			# Vert Colours
			file.write('\ttexture_list {\n')
			file.write('\t\t%s' % (len(vertCols))) # vert count
			idx=0
			for col, index in vertCols.items():
				
				if me_materials:
					material = me_materials[col[3]]
					materialString = materialTable[material.name]
				else:
					materialString = '' # Dont write anything
				
				float_col = col[0], col[1], col[2], 1-material.alpha, materialString
				#print material.apl
				file.write(',\n\t\ttexture { pigment {rgbf<%.6f, %.6f, %.6f, %.6f>}%s}' % float_col)
				index[0] = idx
				idx+=1
			
			file.write( '\n  }\n' )
			
			# Face indicies
			file.write('\tface_indices {\n')
			file.write('\t\t%d' % (len(me.faces) + quadCount)) # faces count
			for fi, f in enumerate(me.faces):
				fv = faces_verts[fi]
				material_index= f.material_index
				if len(fv) == 4:	indicies = (0,1,2), (0,2,3)
				else:				indicies = ((0,1,2),)
				
				if vcol_layer:
					col = vcol_layer[fi]
					
					if len(fv) == 4:
						cols = col.color1, col.color2, col.color3, col.color4
					else:
						cols = col.color1, col.color2, col.color3
				
				
				if not me_materials or me_materials[material_index] == None: # No materials
					for i1, i2, i3 in indicies:
						file.write(',\n\t\t<%d,%d,%d>' % (fv[i1], fv[i2], fv[i3])) # vert count
				else:
					material = me_materials[material_index]
					for i1, i2, i3 in indicies:
						if me.vertex_colors and material.vertex_color_paint:
							# Colour per vertex - vertex colour
							
							col1 = cols[i1]
							col2 = cols[i2]
							col3 = cols[i3]
						
							ci1 = vertCols[col1[0], col1[1], col1[2], material_index][0]
							ci2 = vertCols[col2[0], col2[1], col2[2], material_index][0]
							ci3 = vertCols[col3[0], col3[1], col3[2], material_index][0]
						else:
							# Colour per material - flat material colour
							diffuse_color= material.diffuse_color
							ci1 = ci2 = ci3 = vertCols[diffuse_color[0], diffuse_color[1], diffuse_color[2], f.material_index][0]
						
						file.write(',\n\t\t<%d,%d,%d>, %d,%d,%d' % (fv[i1], fv[i2], fv[i3], ci1, ci2, ci3)) # vert count
					
					
					
			file.write('\n  }\n')
			
			# normal_indices indicies
			file.write('\tnormal_indices {\n')
			file.write('\t\t%d' % (len(me.faces) + quadCount)) # faces count
			for fi, fv in enumerate(faces_verts):
				
				if len(fv) == 4:	indicies = (0,1,2), (0,2,3)
				else:				indicies = ((0,1,2),)
				
				for i1, i2, i3 in indicies:
					if f.smooth:
						file.write(',\n\t\t<%d,%d,%d>' %\
						(uniqueNormals[verts_normals[fv[i1]]][0],\
						 uniqueNormals[verts_normals[fv[i2]]][0],\
						 uniqueNormals[verts_normals[fv[i3]]][0])) # vert count
					else:
						idx = uniqueNormals[faces_normals[fi]][0]
						file.write(',\n\t\t<%d,%d,%d>' % (idx, idx, idx)) # vert count
						
			file.write('\n  }\n')
			
			if uv_layer:
				file.write('\tuv_indices {\n')
				file.write('\t\t%d' % (len(me.faces) + quadCount)) # faces count
				for fi, fv in enumerate(faces_verts):
					
					if len(fv) == 4:	indicies = (0,1,2), (0,2,3)
					else:				indicies = ((0,1,2),)
					
					uv = uv_layer[fi]
					if len(faces_verts[fi])==4:
						uvs = tuple(uv.uv1), tuple(uv.uv2), tuple(uv.uv3), tuple(uv.uv4)
					else:
						uvs = tuple(uv.uv1), tuple(uv.uv2), tuple(uv.uv3)
					
					for i1, i2, i3 in indicies:
						file.write(',\n\t\t<%d,%d,%d>' %\
						(uniqueUVs[uvs[i1]][0],\
						 uniqueUVs[uvs[i2]][0],\
						 uniqueUVs[uvs[i2]][0])) # vert count
				file.write('\n  }\n')
			
			if me.materials:
				material = me.materials[0] # dodgy
				if material and material.raytrace_transparency.enabled:
					file.write('\tinterior { ior %.6f }\n' % material.raytrace_transparency.ior)
			
			writeMatrix(matrix)
			file.write('}\n')
			
			bpy.data.remove_mesh(me)
	
	
	exportCamera()
	#exportMaterials()
	sel = scene.objects
	lamps = [l for l in sel if l.type == 'LAMP']
	exportLamps(lamps)
	exportMeshs(sel)
	
	file.close()


def write_pov_ini(filename_ini, filename_pov, filename_image):
	scene = bpy.data.scenes[0]
	render = scene.render_data
	
	x= int(render.resolution_x*render.resolution_percentage*0.01)
	y= int(render.resolution_y*render.resolution_percentage*0.01)
	
	file = open(filename_ini, 'w')
	
	file.write('Input_File_Name="%s"\n' % filename_pov)
	file.write('Output_File_Name="%s"\n' % filename_image)
	
	file.write('Width=%d\n' % x)
	file.write('Height=%d\n' % y)
	
	# Needed for border render.
	'''
	file.write('Start_Column=%d\n' % part.x)
	file.write('End_Column=%d\n' % (part.x+part.w))
	
	file.write('Start_Row=%d\n' % (part.y))
	file.write('End_Row=%d\n' % (part.y+part.h))
	'''
	
	file.write('Display=0\n')
	file.write('Pause_When_Done=0\n')
	file.write('Output_File_Type=C\n') # TGA, best progressive loading
	file.write('Output_Alpha=1\n')
	
	if render.antialiasing: 
		aa_mapping = {'OVERSAMPLE_5':2, 'OVERSAMPLE_8':3, 'OVERSAMPLE_11':4, 'OVERSAMPLE_16':5} # method 1 assumed
		file.write('Antialias=1\n')
		file.write('Antialias_Depth=%d\n' % aa_mapping[render.antialiasing_samples])
	else:
		file.write('Antialias=0\n')
	
	file.close()


class PovrayRenderEngine(bpy.types.RenderEngine):
	__label__ = "Povray"
	DELAY = 0.02
	def _export(self, scene):
		import tempfile
		
		self.temp_file_in = tempfile.mktemp(suffix='.pov')
		self.temp_file_out = tempfile.mktemp(suffix='.tga')
		self.temp_file_ini = tempfile.mktemp(suffix='.ini')
		
		def info_callback(txt):
			self.update_stats("", "POVRAY: " + txt)
			
		write_pov(self.temp_file_in, scene, info_callback)
		
	def _render(self):
		
		try:		os.remove(self.temp_file_out) # so as not to load the old file
		except:	pass
		
		write_pov_ini(self.temp_file_ini, self.temp_file_in, self.temp_file_out)
		
		print ("***-STARTING-***")
		# This works too but means we have to wait until its done
		# os.system('povray %s' % self.temp_file_ini)
		
		pov_binary = "povray"
		
		if sys.platform=='win32':
			if bitness == 64:
				pov_binary = "pvengine64"
			else:
				pov_binary = "pvengine"
			
		self.process = subprocess.Popen([pov_binary, self.temp_file_ini]) # stdout=subprocess.PIPE, stderr=subprocess.PIPE
		
		print ("***-DONE-***")
	
	def _cleanup(self):
		for f in (self.temp_file_in, self.temp_file_ini, self.temp_file_out):
			try:		os.remove(f)
			except:	pass
		
		self.update_stats("", "")
	
	def render(self, scene):
		
		self.update_stats("", "POVRAY: Exporting data from Blender")
		self._export(scene)
		self.update_stats("", "POVRAY: Parsing File")
		self._render()
		
		r = scene.render_data
		
		# compute resolution
		x= int(r.resolution_x*r.resolution_percentage*0.01)
		y= int(r.resolution_y*r.resolution_percentage*0.01)
		
		
		
		# Wait for the file to be created
		while not os.path.exists(self.temp_file_out):
			if self.test_break():
				try:		self.process.terminate()
				except:	pass
				break
			
			if self.process.poll() != None:
				self.update_stats("", "POVRAY: Failed")
				break
			
			time.sleep(self.DELAY)
		
		if os.path.exists(self.temp_file_out):
			
			self.update_stats("", "POVRAY: Rendering")
			
			prev_size = -1
			
			def update_image():
				result = self.begin_result(0, 0, x, y)
				lay = result.layers[0]
				# possible the image wont load early on.
				try:		lay.rect_from_file(self.temp_file_out, 0, 0)
				except:	pass
				self.end_result(result)
			
			# Update while povray renders
			while True:
				
				# test if povray exists
				if self.process.poll() != None:
					update_image();
					break
				
				# user exit
				if self.test_break():
					try:		self.process.terminate()
					except:	pass
					
					break
				
				# Would be nice to redirect the output
				# stdout_value, stderr_value = self.process.communicate() # locks
				
				
				# check if the file updated
				new_size = os.path.getsize(self.temp_file_out)
				
				if new_size != prev_size:
					update_image()
					prev_size = new_size
				
				time.sleep(self.DELAY)
		
		self._cleanup()


bpy.types.register(PovrayRenderEngine)
