# bpy module and the bpy PyType object

"""
The bpy module.

bpy
===

This module is imported automatically and eventually should provide all the functionality as the Blender module does now.

This only modifies the way data is accessed, added and removed.  The objects, groups, meshes, etc., are unchanged.

At the moment it provides an alternative way to access data from python.

Example::

	# apply the active image to the active mesh
	# this script has no error checking to keep it small and readable.

	scn= bpy.scenes.active
	ob_act = scn.objects.active		# assuming we have an active, might be None
	me = ob_act.getData(mesh=1)		# assuming a mesh type, could be any
	img = bpy.images.active			# assuming we have an active image
	
	for f in me.faces:
		f.image = img

	Window.RedrawAll()

Example::

	# make a new object from an existing mesh
	# and make it active
	scn= bpy.scenes.active
	me = bpy.meshes['mymesh']
	ob = scn.objects.new(me) # new object from the mesh
	scn.objects.active = ob

Example::
	# print the names of any non local objects
	scn= bpy.scenes.active
	for ob in scn.objects:
		if ob.lib:
			print 'external object:', ob.name, ob.lib

Example::
	# add an empty object at each vertex of the active mesh
	scn= bpy.scenes.active
	ob_act = scn.objects.active
	matrix = ob_act.matrixWorld
	me = ob_act.getData(mesh=1)
	
	for v in me.verts:
		ob = scn.objects.new('Empty')
		ob.loc = v.co * matrix			# transform the vertex location by the objects matrix.
		

Example::
	# load all the wave sound files in a directory
	import os
	sound_dir = '/home/me/soundfiles/'
	sounds_new = []
	for fname in os.listdir(sound_dir):
		if fname.lower().endswith('.wav'):
			try:
				snd = bpy.sounds.new(filename = sound_dir + fname)
			except:
				snd = None
			
			if snd:
				sounds_new.append(snd)
	
	# Print the sounds
	for snd in sounds_new:
		print snd
	
Example::
	# apply a new image to each selected mesh object as a texface.
	width, height= 512, 512
	scn= bpy.scenes.active
	
	for ob in scn.objects.context:
		if not ob.lib and ob.type == 'Mesh':	# object isn't from a library and is a mesh
			me = ob.getData(mesh=1)
			me.faceUV = True					# add UV coords and textures if we don't have them.
			
			# Make an image named after the mesh
			img = bpy.images.new(me.name, width, height)
			
			for f in me.faces:
				f.image = img
	
	Window.RedrawAll()

@var scenes: iterator for L{scene<Scene.Scene>} data
@type scenes: L{LibBlockSeq}
@var objects: iterator for L{object<Object.Object>} data
@type objects: L{LibBlockSeq}
@var meshes: iterator for L{mesh<Mesh.Mesh>} data
@type meshes: L{LibBlockSeq}
@var curves: iterator for L{curve<Curve.Curve>} data
@type curves: L{LibBlockSeq}
@var metaballs: iterator for L{metaball<Metaball.Metaball>} data
@type metaballs: L{LibBlockSeq}
@var materials: iterator for L{material<Material.Material>} data
@type materials: L{LibBlockSeq}
@var textures: iterator for L{texture<Texture.Texture>} data
@type textures: L{LibBlockSeq}
@var images: iterator for L{image<Image.Image>} data
@type images: L{LibBlockSeq}
@var lattices: iterator for L{lattice<Lattice.Lattice>} data
@type lattices: L{LibBlockSeq}
@var lamps: iterator for L{lamp<Lamp.Lamp>} data
@type lamps: L{LibBlockSeq}
@var cameras: iterator for L{camera<Camera.Camera>} data
@type cameras: L{LibBlockSeq}
@var ipos: iterator for L{ipo<Ipo.Ipo>} data
@type ipos: L{LibBlockSeq}
@var worlds: iterator for L{world<World.World>} data
@type worlds: L{LibBlockSeq}
@var fonts: iterator for L{font<Font.Font>} data
@type fonts: L{LibBlockSeq}
@var texts: iterator for L{text<Text.Text>} data
@type texts: L{LibBlockSeq}
@var sounds: iterator for L{sound<Sound.Sound>} data
@type sounds: L{LibBlockSeq}
@var groups: iterator for L{group<Group.Group>} data
@type groups: L{LibBlockSeq}
@var armatures: iterator for L{armature<Armature.Armature>} data
@type armatures: L{LibBlockSeq}
@var actions: iterator for L{action<NLA.Action>} data
@type actions: L{LibBlockSeq}
@var libraries: L{librarySeq<LibData>} submodule
@type libraries: L{librarySeq<LibData>}

"""


class LibBlockSeq:
	"""
	Generic Data Access
	===================
		This provides a unified way to access and manipulate data types in Blender
		(scene, object, mesh, curve, metaball, material, texture, image, lattice,
		lamp, camera, ipo, world, font, text, sound, groups, armatures, actions).
		
	Get Item
	========
		To get a datablock by name you can use dictionary-like syntax.
		
		>>> ob = bpy.objects['myobject']
		
		Note that this can only be used for getting.
		
		>>> bpy.objects['myobject'] = data # will raise an error
		
		B{Library distinctions}
		
		Blender doesn't allow naming collisions within its own pool of data, but it's
		possible to run into naming collisions when you have data linked from an external file.
		
		You can specify where the data is from by using a (name, library) pair as the key.
		
		>>> group = bpy.groups['mygroup', '//mylib.blend'] # only return data linked from mylib
		
		If you want to get a group from the local data only you can use None
		
		>>> group = bpy.groups['mygroup', None] # always returns local data
	
	Iterator
	========
		generic_datablock's are not lists; however they can be used like lists.
		
		An iterator allows you to loop through data, without wasting resources on a large list.

		>>> for me in bpy.meshes:
		... 	print me.name

		You can also use len() to see how many datablocks exist.
		
		>>> print len(bpy.scenes)
		
		You cannot use indexing to retrieve an item.
		
		>>> ob = bpy.objects[-1] # will raise an error
		
		If you want to access data as a list simply use the list() function.
		
		>>> ipo_list = list(bpy.ipos)
		
	@type tag: Bool
	@ivar tag: A fast way to set the tag value of every member of the sequence to True or False
	
		For example
		
		>>> bpy.meshes.tag = True
		
		Is the same as...
		
		>>> for me in bpy.meshes: me.tag = True
	
	@type active: Datablock or None
	@ivar active: The active member of the datatype
	
		Applies to:
			- L{images}
			- L{scenes}
			- L{texts}
		This can also be used to set the active data.
		
		>>> bpy.images.active = bpy.images.new(filename = '/home/me/someimage.jpg')
		
	"""

	def new(name="", filename=""):
		"""
		This function returns a new datablock containing no data or loaded from a file.
		
		Exceptions
		==========
		
		Use the filename keyword string values to load data from a file, this works with L{images}, L{texts}, L{sounds}, L{fonts} only.
		
		>>> sound = bpy.sounds.new('newsound', '~/mysound.wav') # uses the first string given for the name.
		
		>>> sound = bpy.sounds.new(filename = '~/mysound.wav') # will use the filename to make the name.
		
		
		Images optionally accept 3 arguments: bpy.images.new(name, width=256, height=256)
		The width and height must br between 4 and 5000 if no args are given they will be 256.
		
		Ipos need 2 arguments: bpy.ipos.new(name, type) type must be a string (use in place of filename) can be...
			- 'Camera'
			- 'World'
			- 'Material'
			- 'Texture'
			- 'Lamp'
			- 'Action'
			- 'Constraint'
			- 'Sequence'
			- 'Curve'
			- 'Key'
		Objects cannot be created from bpy.objects;
		objects must be created from the scene.  Here are some examples.
		
		>>> ob = bpy.scenes.active.objects.new('Empty')
		
		>>> scn = bpy.scenes.active
		... ob = scn.objects.new(bpy.meshes.new('mymesh'))
			
		@rtype: datablock
		"""
	
	def unlink(datablock):
		"""
		This function removes a datablock.
		applies to:
			- L{scenes}
			- L{groups}
			- L{texts}
		Other types will raise an error.
		@rtype: None
		"""
	
