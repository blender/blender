# bpy.lib submodule

"""
The bpy.libraries submodule.

Libraries
=========

This module provides access to objects stored in .blend files.  With it scripts
can append from Blender files to the current scene, like the File->Append
menu entry in Blender does.  It allows programmers to use .blend files as
data files for their scripts.

@warn: This module is new and being considered as a replacement for the 
L{original Library<Library>} module.  Users should stay tuned to see
which module is supported in the end.

Example::
	import bpy

	scn= bpy.data.scenes.active                       # get current scene
	lib = bpy.libraries.load('//file.blend')          # open file.blend
	ob = scn.objects.link(lib.objects.append('Cube')) # append Cube object from library to current scene
	mat = lib.objects.link('Material')                # get a link to a material
	me = ob.getData(mesh=1)                           # get mesh data
	me.materials[0] = mat                             # assign linked material to mesh
"""

def load(filename,relative=False):
	"""
	Select an existing .blend file for use as a library.  Unlike the 
	Library module, multiple libraries can be defined at the same time.  
	
	@type filename: string
	@param filename: The filename of a Blender file. Filenames starting with "//" will be loaded relative to the blend file's location.
	@type relative: boolean
	@param relative: Convert relative paths to absolute paths (default).  Setting this parameter to True will leave paths relative.
	@rtype: Library
	@return: return a L{Library} object.
	"""

def paths(link=0):
	"""
	Returns a list of paths used in the current blend file.
	
	@type link: int
	@param link: 0 (default if no args given) for all library paths, 1 for directly linked library paths only, 2 for indirectly linked library paths only.
	@rtype: List
	@return: return a list of path strings.
	"""

def replace(pathFrom, pathTo):
	"""
	Replaces an existing directly linked path.
	
	@type pathFrom: string
	@param pathFrom: An existing library path.
	@type pathTo: string
	@param pathTo: A new library path.
	"""

class Libraries:
	"""
	The Library object
	==================
	This class provides a unified way to access and manipulate library types
	in Blender.
	It provides access to scenes, objects, meshes, curves, metaballs,
	materials, textures, images, lattices, lamps, cameras, ipos, worlds,
	fonts, texts, sounds, groups, armatures, and actions.
	@ivar filename: The filename of the library, as supplied by user.
	@type filename: string
	@ivar name: The path to the library, as used by Blender.  If the filename supplied by the user is relative, but the relative option to L{library.load()<load>} is False, the name will be the absolute path.
	@type name: string
	@ivar scenes: library L{scene<Scene.Scene>} data
	@type scenes: L{LibData}
	@ivar objects: library L{object<Object.Object>} data
	@type objects: L{LibData}
	@ivar meshes: library L{mesh<Mesh.Mesh>} data
	@type meshes: L{LibData}
	@ivar curves: library L{curve<Curve.Curve>} data
	@type curves: L{LibData}
	@ivar metaballs: library L{metaball<Metaball.Metaball>} data
	@type metaballs: L{LibData}
	@ivar materials: library L{material<Material.Material>} data
	@type materials: L{LibData}
	@ivar textures: library L{texture<Texture.Texture>} data
	@type textures: L{LibData}
	@ivar images: library L{image<Image.Image>} data
	@type images: L{LibData}
	@ivar lattices: library L{lattice<Lattice.Lattice>} data
	@type lattices: L{LibData}
	@ivar lamps: library L{lamp<Lamp.Lamp>} data
	@type lamps: L{LibData}
	@ivar cameras: library L{camera<Camera.Camera>} data
	@type cameras: L{LibData}
	@ivar ipos: library L{ipo<Ipo.Ipo>} data
	@type ipos: L{LibData}
	@ivar worlds: library L{world<World.World>} data
	@type worlds: L{LibData}
	@ivar fonts: library L{font<Font.Font>} data
	@type fonts: L{LibData}
	@ivar texts: library L{text<Text.Text>} data
	@type texts: L{LibData}
	@ivar sounds: library L{sound<Sound.Sound>} data
	@type sounds: L{LibData}
	@ivar groups: library L{group<Group.Group>} data
	@type groups: L{LibData}
	@ivar armatures: library L{armature<Armature.Armature>} data
	@type armatures: L{LibData}
	@ivar actions: library L{action<NLA.Action>} data
	@type actions: L{LibData}
	"""

class LibData:
	"""
	Generic Library Data Access
	===========================
	This class provides access to a specific type of library data.
	"""

	def append(name):
		"""
		Append a new datablock from a library. The new copy
		is added to the current .blend file.

		B{Note}: Blender Objects cannot be appended or linked without linking
		them to a scene.  For this reason, lib.objects.append() returns a
		special "wrapper object" which must be passed to Scene.objects.link()
		or bpy.data.scenes.active.link() in order to actually create the object.
		So the following code will not create a new object::
			import bpy

			scn= bpy.data.scenes.active                       # get current scene
			lib = bpy.libraries.load('//file.blend')          # open file.blend
			pseudoOb = lib.objects.append('Cube'))            # get an object wrapper
		But this code will::
			import bpy

			scn= bpy.data.scenes.active                       # get current scene
			lib = bpy.libraries.load('//file.blend')          # open file.blend
			pseudoOb = lib.objects.append('Cube'))            # get an object wrapper
			ob = scn.objects.link(pseudoOb)  				  # link to scene
		@rtype: Blender data
		@return: return a Blender datablock or object
		@raise IOError: library cannot be read
		@raise ValueError: library does not contain B{name}
		"""
	
	def link(name):
		"""
		Link a new datablock from a library.  The linked data is not copied
		into the local .blend file.

		See L{append} for notes on special handling of Blender Objects.
		@rtype: Blender data
		@return: return a Blender datablock or object
		@raise IOError: library cannot be read
		@raise ValueError: library does not contain B{name}
		"""
		
