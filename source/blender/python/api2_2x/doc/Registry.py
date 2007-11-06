# Blender.Registry module

"""
The Blender.Registry submodule.

B{New}: L{GetKey} and L{SetKey} have been updated to save and load scripts
*configuration data* to files.

Registry
========

This module provides a way to create, retrieve and edit B{persistent data} in
Blender.

When a script is executed it has its own private global dictionary,
which is deleted when the script exits. This is done to avoid problems with
name clashes and garbage collecting.  But because of this, the data created by
a script isn't kept after it leaves: the data is not persistent.  The Registry
module was created to give programmers a way around this limitation.

Possible uses:
	- saving arbitrary data from a script that itself or another one will need
		to access later.
	- saving configuration data for a script: users can view and edit this data
		using the "Scripts Configuration Editor" script.
	- saving the current state of a script's GUI (its button values) to restore it
		when the script is executed again.

Example::

	import Blender
	from Blender import Registry

	# this function updates the Registry when we need to:
	def update_Registry():
		d = {}
		d['myvar1'] = myvar1
		d['myvar2'] = myvar2
		d['mystr'] = mystr
		# cache = True: data is also saved to a file
		Blender.Registry.SetKey('MyScript', d, True)

	# first declare global variables that should go to the Registry:
	myvar1 = 0
	myvar2 = 3.2
	mystr = "hello"

	# then check if they are already there (saved on a
	# previous execution of this script):
	rdict = Registry.GetKey('MyScript', True) # True to check on disk also
	if rdict: # if found, get the values saved there
		try:
			myvar1 = rdict['myvar1']
			myvar2 = rdict['myvar2']
			mystr = rdict['mystr']
		except: update_Registry() # if data isn't valid rewrite it

	# ...
	# here goes the main part of the script ...
	# ...

	# if at some point the data is changed, we update the Registry:
	update_Registry()

@note: In Python terms, the Registry holds a dictionary of dictionaries.
	Technically any Python or BPython object can be stored: there are no
	restrictions, but ...

@note: We have a few recommendations:

	Data saved to the Registry is kept in memory, so if you decide to store large
	amounts your script users should be clearly informed about it --
	always keep in mind that you have no idea about their resources and the
	applications they are running at a given time (unless you are the only
	user), so let them decide.

	There are restrictions to the data that gets automatically saved to disk by
	L{SetKey}(keyname, dict, True):  this feature is only meant for simple data
	(bools, ints, floats, strings and dictionaries or sequences of these types).

	For more demanding needs, it's of course trivial to save data to another 
	file or to a L{Blender Text<Text>}.
"""

def Keys ():
	"""
	Get all keys currently in the Registry's dictionary.
	"""

def GetKey (key, cached = False):
	"""
	Get key 'key' from the Registry.
	@type key: string
	@param key: a key from the Registry dictionary.
	@type cached: bool
	@param cached: if True and the requested key isn't already loaded in the
		Registry, it will also be searched on the user or default scripts config
		data dir (config subdir in L{Blender.Get}('datadir')).
	@return: the dictionary called 'key'.
	"""

def SetKey (key, dict, cache = False):
	"""
	Store a new entry in the Registry.
	@type key: string
	@param key: the name of the new entry, tipically your script's name.
	@type dict: dictionary
	@param dict: a dict with all data you want to save in the Registry.
	@type cache: bool
	@param cache: if True the given key data will also be saved as a file
		in the config subdir of the scripts user or default data dir (see
		L{Blender.Get}).
	@warn: as stated in the notes above, there are restrictions to what can
		be automatically stored in config files.
	"""

def RemoveKey (key):
	"""
	Remove the dictionary with key 'key' from the Registry.
	@type key: string
	@param key: the name of an existing Registry key.
	"""
