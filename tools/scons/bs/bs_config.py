# configuration functions
import sys
import os

import SCons.Script
import bs_globals

def checkPyVersion():
	if hex(sys.hexversion) < 0x2030000:
		print ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
		print
		print "You need at least Python 2.3 to build Blender with SCons"
		print
		print ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
		sys.exit()

def parseOpts():
	copyloc = ''
	
	all_args = sys.argv[1:]
	parser =  SCons.Script.OptParser()
	options, targets = parser.parse_args(all_args)
	if ('clean' in targets):
		bs_globals.enable_clean = 1
	
	# User configurable options file. This can be controlled by the user by running
	# scons with the following argument: CONFIG=user_config_options_file
	bs_globals.config_file = bs_globals.arguments.get('CONFIG', 'config.opts')
	bs_globals.root_build_dir = bs_globals.arguments.get('root_build_dir', '..' + os.sep + 'build' + os.sep + sys.platform + os.sep)
	
	copyloc = bs_globals.arguments.get('copyto', '0')
	if copyloc == '0':
		bs_globals.docopy = 0;
	else:
		bs_globals.docopy = 1;
		bs_globals.copyto = copyloc
		