attributes = """
	@ivar name: unique name within each blend file.
		
		The name is case sensitive and 21 characters maximum length.

		B{Note}: a blend file may have naming collisions when external library data is used,
		be sure to check the value of L{lib}.

		B{Note}: Setting a value longer then 21 characters will be shortened
	@type name: string
	@ivar lib: path to the blend file this datablock is stored in (readonly).

		lib will be None unless you are using external blend files with (File, Append/Link)

		B{Note}: the path may be relative, to get the full path use L{Blender.sys.expandpath<Sys.expandpath>}
	@type lib: string or None
	
	@ivar fakeUser: When set to True, this datablock wont be removed, even if nothing is using it.
		All data has this disabled by default except for Actions.	
	@type fakeUser: bool
	@ivar tag: A temporary tag that to flag data as being used within a loop.
		always set all tags to True or False before using since blender uses this flag for its own internal operations.
	@type tag: bool
	@ivar users: The number of users this datablock has. (readonly)
		Zero user datablocks are de-allocated after reloading and saving.
	@type users: int
	@ivar properties: Returns an L{IDGroup<IDProp.IDGroup>} reference to this 
	datablocks's ID Properties.
	@type properties: L{IDGroup<IDProp.IDGroup>}
"""