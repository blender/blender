# bpy module and the bpy PyType object

"""
The bpy module.

bpy
===

This module is imported by automatically and eventually should provide all the functionality as the Blender module does now.

This only modifies the way data is accessed, added and removed, the objects, groups, meshes etc are unchanged.

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
	for filename in os.listdir(sound_dir):
		if filename.lower().endswith('.wav'):
			try:
				snd = bpy.sounds.load(sound_dir + filename)
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
		if not ob.lib and ob.type == 'Mesh':	# object isnt from a library and is a mesh
			me = ob.getData(mesh=1)
			me.faceUV = True					# add UV coords and textures if we dont have them.
			
			# Make an image named after the mesh
			img = bpy.images.new(me.name, 512, 512)
			
			for f in me.faces:
				f.image = img
	
	Window.RedrawAll()

@var scenes: iterator for L{scene<Scene.Scene>} data
@var objects: iterator for L{object<Object.Object>} data
@var meshes: iterator for L{mesh<Mesh.Mesh>} data
@var curves: iterator for L{curve<Curve.Curve>} data
@var metaballs: iterator for L{metaball<Metaball.Metaball>} data
@var materials: iterator for L{material<Material.Material>} data
@var textures: iterator for L{texture<Texture.Texture>} data
@var images: iterator for L{image<Image.Image>} data
@var lattices: iterator for L{lattice<Lattice.Lattice>} data
@var lamps: iterator for L{lamp<Lamp.Lamp>} data
@var cameras: iterator for L{camera<Camera.Camera>} data
@var ipos: iterator for L{ipo<Ipo.Ipo>} data
@var worlds: iterator for L{world<World.World>} data
@var fonts: iterator for L{font<Font.Font>} data
@var texts: iterator for L{text<Text.Text>} data
@var sounds: iterator for L{sound<Sound.Sound>} data
@var groups: iterator for L{group<Group.Group>} data
@var armatures: iterator for L{armature<Armature.Armature>} data
@var actions: iterator for L{action<NLA.Action>} data

"""


class dataIterator:
	"""
	Generic Data Access
	===================
		This provides a unified way to access and manipulate data types in Blender
		(scene, object, mesh, curve, metaball, material, texture, image, lattice,
		lamp, camera, ipo, world, font, text, sound, groups, armatures, actions)
		
	Get Item
	========
		To get a datablock by name you can use dictionary like syntax.
		
		>>> ob = bpy.objects['myobject']
		
		note that this can only be used for getting.
		
		>>> bpy.objects['myobject'] = data # will raise an error
		
		B{Library distinctions}
		
		Blender dosnt allow naming collisions within its own pool of data, however its
		possible to run into naming collisions when you have data linked from an external file.
		
		you can specify where the data is from by using a (name, library) pair as the key.
		
		>>> group = bpy.groups['mygroup', '//mylib.blend'] # only return data linked from mylib
		
		if you want to get a group from the local data only you can use None
		
		>>> group = bpy.groups['mygroup', None] # always returns local data
	
	Iterator
	========
		generic_datablock's are not lists, however they can be used like lists.
		
		an iterator allows you to loop through data, without waisting recources on a large list.

		>>> for me in bpy.meshes:
		... 	print me.name

		you can also use len() to see how many datablocks exist
		
		>>> print len(bpy.scenes)
		
		You cannot use indexing to retrieve an item
		
		>>> ob = bpy.objects[-1] # will raise an error
		
		if you want to access data as a list simply uset he list() function
		
		>>> ipo_list = list(bpy.ipos)
		
	@type active: Datablock or None
	@ivar active:
		applies to:
			- L{images}
			- L{scenes}
			- L{texts}
		this can also be used to set the active data.
		Example::
			bpy.images.active = bpy.images.load('/home/me/someimage.jpg')
	"""

	def new(name):
		"""
		this function returns a new datablock
		exceptions::
			Images optionally accept 2 extra arguments: bpy.images.new(name, width=256, height=256)
				The width and height must br between 4 and 5000 if no args are given they will be 256.
			
			Ipos need 2 arguments: bpy.ipos.new(name, type) 
				type must be a string can be 
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
			
		@rtype: datablock
		"""

	def load(filename):
		"""
		this function loads a new datablock from a file.
		applies to:
			- L{fonts}
			- L{sounds}
			- L{images}
		other types will raise an error.
		@rtype: datablock
		"""
	
	def unlink(datablock):
		"""
		this function removes a datablock.
		applies to:
			- L{scenes}
			- L{groups}
			- L{texts}
		other types will raise an error.
		@rtype: None
		"""
	
