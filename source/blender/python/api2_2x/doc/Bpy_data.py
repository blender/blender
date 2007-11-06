# bpy module and the bpy PyType object

"""
The bpy module.

bpy.data (Generic Data Access)
==============================

Example::

	# apply the active image to the active mesh
	# this script has no error checking to keep it small and readable.

	sce= bpy.data.scenes.active
	ob_act = sce.objects.active		# assuming we have an active, might be None
	me = ob_act.getData(mesh=1)		# assuming a mesh type, could be any
	img = bpy.data.images.active			# assuming we have an active image
	
	for f in me.faces:
		f.image = img

	Window.RedrawAll()

Example::

	# make a new object from an existing mesh
	# and make it active
	scn= bpy.data.scenes.active
	me = bpy.data.meshes['mymesh']
	ob = sce.objects.new(me) # new object from the mesh
	sce.objects.active = ob

Example::
	# print the names of any non local objects
	sce= bpy.data.scenes.active
	for ob in sce.objects:
		if ob.lib:
			print 'external object:', ob.name, ob.lib

Example::
	# add an empty object at each vertex of the active mesh
	scn= bpy.data.scenes.active
	ob_act = sce.objects.active
	matrix = ob_act.matrixWorld
	me = ob_act.getData(mesh=1)
	
	for v in me.verts:
		ob = sce.objects.new('Empty')
		ob.loc = v.co * matrix			# transform the vertex location by the objects matrix.
		

Example::
	# load all the wave sound files in a directory
	import os
	sound_dir = '/home/me/soundfiles/'
	sounds_new = []
	for fname in os.listdir(sound_dir):
		if fname.lower().endswith('.wav'):
			try:
				snd = bpy.data.sounds.new(filename = sound_dir + fname)
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
	scn= bpy.data.scenes.active
	
	for ob in sce.objects.context:
		if not ob.lib and ob.type == 'Mesh':	# object isn't from a library and is a mesh
			me = ob.getData(mesh=1)
			me.faceUV = True					# add UV coords and textures if we don't have them.
			
			# Make an image named after the mesh
			img = bpy.data.images.new(me.name, width, height)
			
			for f in me.faces:
				f.image = img
	
	Window.RedrawAll()



@var scenes: sequence for L{scene<Scene.Scene>} data
@type scenes: L{libBlockSeq}
@var objects: sequence for L{object<Object.Object>} data
@type objects: L{libBlockSeq}
@var meshes: sequence for L{mesh<Mesh.Mesh>} data
@type meshes: L{libBlockSeq}
@var curves: sequence for L{curve<Curve.Curve>} data, used to store Curve, Surface and Text3d data.
@type curves: L{libBlockSeq}
@var metaballs: sequence for L{metaball<Metaball.Metaball>} data
@type metaballs: L{libBlockSeq}
@var materials: sequence for L{material<Material.Material>} data
@type materials: L{libBlockSeq}
@var textures: sequence for L{texture<Texture.Texture>} data
@type textures: L{libBlockSeq}
@var images: sequence for L{image<Image.Image>} data
@type images: L{libBlockSeq}
@var lattices: sequence for L{lattice<Lattice.Lattice>} data
@type lattices: L{libBlockSeq}
@var lamps: sequence for L{lamp<Lamp.Lamp>} data
@type lamps: L{libBlockSeq}
@var cameras: sequence for L{camera<Camera.Camera>} data
@type cameras: L{libBlockSeq}
@var ipos: sequence for L{ipo<Ipo.Ipo>} data
@type ipos: L{libBlockSeq}
@var worlds: sequence for L{world<World.World>} data
@type worlds: L{libBlockSeq}
@var fonts: sequence for L{font<Font.Font>} data
@type fonts: L{libBlockSeq}
@var texts: sequence for L{text<Text.Text>} data
@type texts: L{libBlockSeq}
@var sounds: sequence for L{sound<Sound.Sound>} data
@type sounds: L{libBlockSeq}
@var groups: sequence for L{group<Group.Group>} data
@type groups: L{libBlockSeq}
@var armatures: sequence for L{armature<Armature.Armature>} data
@type armatures: L{libBlockSeq}
@var actions: sequence for L{action<NLA.Action>} data
@type actions: L{libBlockSeq}
"""


class libBlockSeq:
	"""
	Generic Data Access
	===================
		This provides a unified way to access and manipulate data types in Blender
		(scene, object, mesh, curve, metaball, material, texture, image, lattice,
		lamp, camera, ipo, world, font, text, sound, groups, armatures, actions).
		
	Get Item
	========
		To get a datablock by name you can use dictionary-like syntax.
		
		>>> ob = bpy.data.objects['myobject']
		
		Note that this can only be used for getting.
		
		>>> bpy.data.objects['myobject'] = data # will raise an error
		
		B{Library distinctions}
		
		Blender doesn't allow naming collisions within its own data, but it's
		possible to run into naming collisions when you have data linked from an external blend file.
		
		You can specify where the data is from by using a (name, library) pair as the key.
		
		>>> group = bpy.data.groups['mygroup', '//mylib.blend'] # only return data linked from mylib
		
		If you want to get a group from the local data only you can use None
		
		>>> group = bpy.data.groups['mygroup', None] # always returns local data
	
	Sequence
	========
		These generic datablocks are sequence datatypes.  They are not lists.  They support the dictionary and iterator protocols.  This implies the following

				- A B{for} statement allows you to loop through data using the iterator protocol without wasting resources on creating a large list.

		>>> for me in bpy.data.meshes:
		... 	print me.name

				- You can also use len() to see how many datablocks exist.
		
		>>> print len(bpy.data.scenes)
		
				- Because the sequences are not lists and the [] operator is used to get items by name, you cannot use indexing to retrieve an item.
		
		>>> ob = bpy.data.objects[-1] # will raise an error
		
				- If you want to access the entire sequence as a list simply use the list() constructor.
		
		>>> ipo_list = list(bpy.data.ipos)
		
	@type tag: Bool
	@ivar tag: A fast way to set the tag value of every member of the sequence to True or False
	
		For example
		
		>>> bpy.data.meshes.tag = True
		
		Is the same as...
		
		>>> for me in bpy.data.meshes: me.tag = True
	
	@type active: Datablock or None
	@ivar active: The active member of the datatype
	
		Applies to:
			- L{images}
			- L{scenes}
			- L{texts}
		This can also be used to set the active data.
		
		>>> bpy.data.images.active = bpy.data.images.new(filename = '/home/me/someimage.jpg')
		
	"""

	def new(name):
		"""
		fixme: need description for parameters.
		This function returns a new datablock containing no data or loaded from a file.
		
		Most datatypes accept a name for their argument except for L{sounds}, L{fonts}, L{ipos} and L{curves} that need an additional argument.
		
		The name argument is optional if not given a default name will be assigned.
		
		The name given may be modified by blender to make it unique.
		
		Loading From File
		=================
		For L{images}, L{texts}, L{sounds}, L{fonts} types you can use the filename keyword to make a new datablock from a file.
		
		New L{sounds}, L{fonts} can only be made with the a filename given.
		
		The filename can a keyword or the second argument, use the keyword only for the datablocks new name to be set by the filename.
		
		>>> sound = bpy.data.sounds.new('newsound', '~/mysound.wav') # uses the first string given for the name.
		
		>>> sound = bpy.data.sounds.new(filename = '~/mysound.wav') # will use the filename to make the name.
		
		Images
		======
		Images optionally accept extra 2 arguments for width and height, values between 4 and 5000 if no args are given they will be 256.
		
		>>> img = bpy.data.images.new(name, 512, 512)
		
		Curves
		======
		Curves need 2 arguments: bpy.data.curves.new(name, type) type must be one of the following...
			- 'Curve'
			- 'Text3d'
		
		>>> text3d = bpy.data.curves.new('MyCurve', 'Text3d')
		
		Ipos
		====
		Ipos need 2 arguments: bpy.data.ipos.new(name, type) type must be one of the following...
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
		Objects cannot be created from bpy.data.objects;
		objects must be created from the scene.  Here are some examples.
		
		>>> ob = bpy.data.scenes.active.objects.new('Empty')
		
		>>> scn = bpy.data.scenes.active
		... ob = sce.objects.new(bpy.data.meshes.new('mymesh'))
		
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


	