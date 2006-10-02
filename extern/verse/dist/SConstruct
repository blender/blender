#!/usr/bin/env python
#
# I think it is quite straight-forward to add new platforms,
# just look at the old makefile and at the existing platforms.
#
# This SConstruct creates a configuration file which can be
# used for tweaking a build.
#
# For more about SConstruct, see <http://www.scons.org/>.
#

import os
import sys
import re
import time
import string
from distutils import sysconfig

root_build_dir = '..' + os.sep + 'build' + os.sep

config_file = 'config.opts'
version = '1.0'

env = Environment ()

defines = []
cflags = []
debug_flags = []
extra_flags = []
release_flags = []
warn_flags = []
platform_libs = []
platform_libpath = []
platform_linkflags = []

if sys.platform == 'win32':
    print "Building on win32"
    defines += ['_WIN32']
    warn_flags = ['/Wall']
    platform_libs = ['ws2_32']
elif sys.platform == 'linux2':
    print "Building on linux2"
elif sys.platform == 'openbsd3':
    print "Building on openbsd3"


env_dict = env.Dictionary()
if os.path.exists (config_file):
    print "Using config file: " + config_file
else:
    print "Creating new config file: " + config_file
    config = open (config_file, 'w')
    config.write ("#Configuration file for verse SCons user definable options.\n")
    config.write ("BUILD_BINARY = 'release'\n")
    config.write ("\n# Compiler information.\n")
    config.write ("HOST_CC = %r\n"%(env_dict['CC']))
    config.write ("HOST_CXX = %r\n"%(env_dict['CXX']))
    config.write ("TARGET_CC = %r\n"%(env_dict['CC']))
    config.write ("TARGET_CXX = %r\n"%(env_dict['CXX']))
    config.write ("TARGET_AR = %r\n"%(env_dict['AR']))
    config.write ("PATH = %r\n"%(os.environ['PATH']))

user_options_env = Environment()
user_options = Options (config_file)
user_options.AddOptions(
    (EnumOption ('BUILD_BINARY',
        'Build a release or debug binary.', 'release',
        allowed_values = ('release', 'debug'))),
    ('BUILD_DIR', 'Target directory for intermediate files.',
        root_build_dir),
    (EnumOption ('REGEN_PROTO',
        'Whether to regenerate the protocol files', 'no',
        allowed_values = ('yes', 'no'))),
    ('HOST_CC', 'C compiler for the host platfor. This is the same as target platform when not cross compiling.', env_dict['CC']),
    ('HOST_CXX', 'C++ compiler for the host platform. This is the same as target platform when not cross compiling.', env_dict['CXX']),
    ('TARGET_CC', 'C compiler for the target platform.', env_dict['CC']),
    ('TARGET_CXX', 'C++ compiler for the target platform.', env_dict['CXX']),
    ('TARGET_AR', 'Linker command for linking libraries.', env_dict['AR']),
    ('PATH', 'Standard search path', os.environ['PATH'])
)
user_options.Update (user_options_env)
user_options_dict = user_options_env.Dictionary()

root_build_dir = user_options_dict['BUILD_DIR']

if user_options_dict['BUILD_BINARY'] == 'release':
    cflags = extra_flags + release_flags + warn_flags
    if sys.platform == 'win32':
        defines += ['NDEBUG']
else:
    cflags = extra_flags + debug_flags + warn_flags
    if sys.platform == 'win32':
        #defines += ['_DEBUG'] specifying this makes msvc want to link to python22_d.lib??
        platform_linkflags += ['/DEBUG','/PDB:verse.pdb']


env = Environment()
env.Replace (CC = user_options_dict['TARGET_CC'])
env.Replace (CXX = user_options_dict['TARGET_CXX'])
env.Replace (PATH = user_options_dict['PATH'])
env.Replace (AR = user_options_dict['TARGET_AR'])

cmd_gen_files = (['v_cmd_gen.c',
				  'v_cmd_def_a.c',
				  'v_cmd_def_b.c',
				  'v_cmd_def_c.c',
				  'v_cmd_def_g.c',
				  'v_cmd_def_m.c',
				  'v_cmd_def_o.c',
				  'v_cmd_def_s.c',
				  'v_cmd_def_t.c'
				  ])

cmd_gen_deps = (['v_gen_pack_init.c'])

proto_env = env.Copy()
proto_env.Append(CPPDEFINES=['V_GENERATE_FUNC_MODE'])
mkprot_tool = proto_env.Program(target = 'mkprot', source = cmd_gen_files)

mkprot_re = re.compile('v_cmd_def_([a-z]{1}).c')
def mkprot_emitter(target = None, source = None, env = None):
	newtargets = list()
	for s in source:
		m = mkprot_re.match(str(s))
		if m:
			newtargets.append("v_gen_pack_"+m.group(1)+"_node.c")
	newtargets.extend(['verse.h'])
	env.Depends(newtargets, mkprot_tool)
	return (newtargets, source)

mkprot_bld = Builder(action = "\"" + mkprot_tool[0].abspath + "\"",
			emitter = mkprot_emitter)

env['BUILDERS']['Protocol'] = mkprot_bld

cmd_gen_deps.extend(env.Protocol('do_mkprot', cmd_gen_files))
cmd_gen_deps.pop()

lib_source_files = (['v_cmd_buf.c',
					 'v_connect.c',
					 'v_connection.c',
					 'v_encryption.c',
					 'v_func_storage.c',
					 'v_man_pack_node.c',
					 'v_network.c',
					 'v_network_in_que.c',
					 'v_network_out_que.c',
					 'v_pack.c',
					 'v_pack_method.c',
					 'v_prime.c',
					 'v_randgen.c',
					 'v_util.c',
					 'v_bignum.c'
					 ])
lib_source_files.extend(cmd_gen_deps)

server_source_files = (['vs_connection.c',
                        'vs_main.c',
                        'vs_master.c',
                        'vs_node_audio.c',
                        'vs_node_bitmap.c',
                        'vs_node_curve.c',
                        'vs_node_geometry.c',
                        'vs_node_head.c',
                        'vs_node_material.c',
                        'vs_node_object.c',
                        'vs_node_particle.c',
                        'vs_node_storage.c',
                        'vs_node_text.c'
                        ])

verse_example_sources = (['examples/list-nodes.c'])

verselib_env = env.Copy()
verselib_env.Append(CPPDEFINES = defines)

verseserver_env = env.Copy()
verseserver_env.Append(CPPDEFINES = defines)
verseserver_env.Append (LIBS=['libverse'])
verseserver_env.Append (LIBPATH = ['.'])
verseserver_env.Append (LIBS= platform_libs)

verseexample_env = env.Copy()
verseexample_env.Append(CPPDEFINES = defines)
verseexample_env.Append (LIBS=['libverse'])
verseexample_env.Append (LIBPATH = ['.'])
verseexample_env.Append (LIBS= platform_libs)
verseexample_env.Append (CPPPATH = ['.'])

verselib = verselib_env.Library(target='libverse', source=lib_source_files)
verseserver_env.Program(target='verse', source=server_source_files)
verseexample_env.Program(target='list-nodes', source=verse_example_sources)

