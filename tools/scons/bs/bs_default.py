# Default target

import bs_globals

def noaction(env, target, source):
	print "Empty action"
	
def BlenderDefault(target):
	"""
	The default Blender build.
	"""
	def_env = bs_globals.init_env.Copy()
	default = def_env.Command('nozip', 'blender$PROGSUFFIX', noaction)
	if bs_globals.user_options_dict['BUILD_BLENDER_PLAYER'] == 1:
		def_env.Depends(default, 'blenderplayer$PROGSUFFIX')
	def_env.Alias(".", default)