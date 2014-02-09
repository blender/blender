#!/usr/bin/env python

"""
tools.BlenderEnvironment

This environment builds on SCons.Script.SConscript.SConsEnvironment

* library repository
* custom printout
* wrapper functions

TODO: clean up and sanitise code - crosscheck with btools and SConstruct
to kill any code duplication

"""

import os
import string
import ctypes as ct
import glob
import time
import sys
import tarfile
import shutil
import cStringIO
import platform

from SCons.Script.SConscript import SConsEnvironment
import SCons.Action
import SCons.Util
import SCons.Builder
import SCons.Subst
import SCons.Tool
import bcolors
bc = bcolors.bcolors()
import btools
VERSION = btools.VERSION
VERSION_RELEASE_CYCLE = btools.VERSION_RELEASE_CYCLE

Split = SCons.Util.Split
Action = SCons.Action.Action
Builder = SCons.Builder.Builder
GetBuildPath = SConsEnvironment.GetBuildPath

# a few globals
root_build_dir = ''
doc_build_dir = ''
quickie = None # Anything else than None if BF_QUICK has been passed
quicklist = [] # The list of libraries/programs to compile during a quickie
program_list = [] # A list holding Nodes to final binaries, used to create installs
arguments = None
targets = None
resources = []
allowed_bitnesses = {4 : 32, 8 : 64} # only expecting 32-bit or 64-bit
bitness = allowed_bitnesses[ct.sizeof(ct.c_void_p)]

##### LIB STUFF ##########

possible_types = ['core'] # can be set in ie. SConstruct
libs = {}
vcp = []

def getresources():
    return resources

def init_lib_dict():
    for pt in possible_types:
        libs[pt] = {}

# helper func for add_lib_to_dict
def internal_lib_to_dict(dict = None, libtype = None, libname = None, priority = 100):
    if not libname in dict[libtype]:
        done = None
        while not done:
            if dict[libtype].has_key(priority):
                priority = priority + 1
            else:
                done = True
        dict[libtype][priority] = libname

# libtype and priority can both be lists, for defining lib in multiple places
def add_lib_to_dict(env, dict = None, libtype = None, libname = None, priority = 100):
    if not dict or not libtype or not libname:
        print "Passed wrong arg"
        env.Exit()

    if type(libtype) is str and type(priority) is int:
        internal_lib_to_dict(dict, libtype, libname, priority)
    elif type(libtype) is list and type(priority) is list:
        if len(libtype)==len(priority):
            for lt, p in zip(libtype, priority):
                internal_lib_to_dict(dict, lt, libname, p)
        else:
            print "libtype and priority lists are unequal in length"
            env.Exit()
    else:
        print "Wrong type combinations for libtype and priority. Only str and int or list and list"
        env.Exit()

def create_blender_liblist(lenv = None, libtype = None):
    if not lenv or not libtype:
        print "missing arg"

    lst = []
    if libtype in possible_types:
        curlib = libs[libtype]
        sortlist = curlib.keys()
        sortlist.sort()
        for sk in sortlist:
            v = curlib[sk]
            if not (root_build_dir[0]==os.sep or root_build_dir[1]==':'):
                target = os.path.abspath(os.getcwd() + os.sep + root_build_dir + 'lib' + os.sep +lenv['LIBPREFIX'] + v + lenv['LIBSUFFIX'])
            else:
                target = os.path.abspath(root_build_dir + 'lib' + os.sep +lenv['LIBPREFIX'] + v + lenv['LIBSUFFIX'])
            lst.append(target)

    return lst

## TODO: static linking
def setup_staticlibs(lenv):
    statlibs = [
        #here libs for static linking
    ]

    libincs = []

    if lenv['WITH_BF_FFMPEG']:
        libincs += Split(lenv['BF_FFMPEG_LIBPATH'])

    libincs.extend([
        lenv['BF_OPENGL_LIBPATH'],
        lenv['BF_JPEG_LIBPATH'],
        lenv['BF_ZLIB_LIBPATH'],
        lenv['BF_PNG_LIBPATH'],
        lenv['BF_ICONV_LIBPATH']
        ])

    if lenv['WITH_BF_STATICJPEG']:
        statlibs += Split(lenv['BF_JPEG_LIB_STATIC'])
    if lenv['WITH_BF_STATICPNG']:
        statlibs += Split(lenv['BF_PNG_LIB_STATIC'])

    libincs += Split(lenv['BF_FREETYPE_LIBPATH'])
    if lenv['WITH_BF_PYTHON']:
        libincs += Split(lenv['BF_PYTHON_LIBPATH'])
    if lenv['WITH_BF_SDL']:
        libincs += Split(lenv['BF_SDL_LIBPATH'])
    if lenv['WITH_BF_JACK'] and not lenv['WITH_BF_JACK_DYNLOAD']:
        libincs += Split(lenv['BF_JACK_LIBPATH'])
    if lenv['WITH_BF_SNDFILE']:
        libincs += Split(lenv['BF_SNDFILE_LIBPATH'])
    if lenv['WITH_BF_TIFF']:
        libincs += Split(lenv['BF_TIFF_LIBPATH'])
        if lenv['WITH_BF_STATICTIFF']:
            statlibs += Split(lenv['BF_TIFF_LIB_STATIC'])
    if lenv['WITH_BF_FFTW3']:
        libincs += Split(lenv['BF_FFTW3_LIBPATH'])
        if lenv['WITH_BF_STATICFFTW3']:
            statlibs += Split(lenv['BF_FFTW3_LIB_STATIC'])
    '''
    if lenv['WITH_BF_ELTOPO']:
        libincs += Split(lenv['BF_LAPACK_LIBPATH'])
        if lenv['WITH_BF_STATICLAPACK']:
            statlibs += Split(lenv['BF_LAPACK_LIB_STATIC'])
    '''
    if lenv['WITH_BF_FFMPEG'] and lenv['WITH_BF_STATICFFMPEG']:
        statlibs += Split(lenv['BF_FFMPEG_LIB_STATIC'])
    if lenv['WITH_BF_INTERNATIONAL']:
        if lenv['WITH_BF_FREETYPE_STATIC']:
            statlibs += Split(lenv['BF_FREETYPE_LIB_STATIC'])
    if lenv['WITH_BF_OPENAL']:
        libincs += Split(lenv['BF_OPENAL_LIBPATH'])
        if lenv['WITH_BF_STATICOPENAL']:
            statlibs += Split(lenv['BF_OPENAL_LIB_STATIC'])
    if lenv['WITH_BF_STATICOPENGL']:
        statlibs += Split(lenv['BF_OPENGL_LIB_STATIC'])
    if lenv['WITH_BF_STATICCXX']:
        statlibs += Split(lenv['BF_CXX_LIB_STATIC'])

    if lenv['WITH_BF_PYTHON'] and lenv['WITH_BF_STATICPYTHON']:
        statlibs += Split(lenv['BF_PYTHON_LIB_STATIC'])

    if lenv['WITH_BF_SNDFILE'] and lenv['WITH_BF_STATICSNDFILE']:
        statlibs += Split(lenv['BF_SNDFILE_LIB_STATIC'])

    if lenv['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc', 'win64-mingw'):
        libincs += Split(lenv['BF_PTHREADS_LIBPATH'])

    if lenv['WITH_BF_COLLADA']:
        libincs += Split(lenv['BF_OPENCOLLADA_LIBPATH'])
        if lenv['OURPLATFORM'] not in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc', 'win64-mingw'):
            libincs += Split(lenv['BF_PCRE_LIBPATH'])
            libincs += Split(lenv['BF_EXPAT_LIBPATH'])
        if lenv['WITH_BF_STATICOPENCOLLADA']:
            statlibs += Split(lenv['BF_OPENCOLLADA_LIB_STATIC'])

    if lenv['WITH_BF_OPENMP']:
        if lenv['OURPLATFORM'] == 'linuxcross':
            libincs += Split(lenv['BF_OPENMP_LIBPATH'])
        if lenv['WITH_BF_STATICOPENMP']:
            statlibs += Split(lenv['BF_OPENMP_LIB_STATIC'])
            
    if lenv['WITH_BF_OIIO']:
        libincs += Split(lenv['BF_OIIO_LIBPATH'])
        if lenv['WITH_BF_STATICOIIO']:
            statlibs += Split(lenv['BF_OIIO_LIB_STATIC'])
    if lenv['WITH_BF_OPENEXR']:
        libincs += Split(lenv['BF_OPENEXR_LIBPATH'])
        if lenv['WITH_BF_STATICOPENEXR']:
            statlibs += Split(lenv['BF_OPENEXR_LIB_STATIC'])
    if lenv['WITH_BF_ZLIB'] and lenv['WITH_BF_STATICZLIB']:
        statlibs += Split(lenv['BF_ZLIB_LIB_STATIC'])

    if lenv['WITH_BF_OCIO']:
        libincs += Split(lenv['BF_OCIO_LIBPATH'])
        if lenv['WITH_BF_STATICOCIO']:
            statlibs += Split(lenv['BF_OCIO_LIB_STATIC'])

    if lenv['WITH_BF_BOOST']:
        libincs += Split(lenv['BF_BOOST_LIBPATH'])
        if lenv['WITH_BF_STATICBOOST']:
            statlibs += Split(lenv['BF_BOOST_LIB_STATIC'])

    if lenv['WITH_BF_CYCLES_OSL']:
        libincs += Split(lenv['BF_OSL_LIBPATH'])
        if lenv['WITH_BF_STATICOSL']:
            statlibs += Split(lenv['BF_OSL_LIB_STATIC'])

    if lenv['WITH_BF_LLVM']:
        libincs += Split(lenv['BF_LLVM_LIBPATH'])
        if lenv['WITH_BF_STATICLLVM']:
            statlibs += Split(lenv['BF_LLVM_LIB_STATIC'])

    if lenv['WITH_BF_JEMALLOC']:
        libincs += Split(lenv['BF_JEMALLOC_LIBPATH'])
        if lenv['WITH_BF_STATICJEMALLOC']:
            statlibs += Split(lenv['BF_JEMALLOC_LIB_STATIC'])

    if lenv['OURPLATFORM']=='linux':
        if lenv['WITH_BF_3DMOUSE']:
            libincs += Split(lenv['BF_3DMOUSE_LIBPATH'])
            if lenv['WITH_BF_STATIC3DMOUSE']:
                statlibs += Split(lenv['BF_3DMOUSE_LIB_STATIC'])

    # setting this last so any overriding of manually libs could be handled
    if lenv['OURPLATFORM'] not in ('win32-vc', 'win32-mingw', 'win64-vc', 'linuxcross', 'win64-mingw'):
        # We must remove any previous items defining this path, for same reason stated above!
        libincs = [e for e in libincs if SCons.Subst.scons_subst(e, lenv, gvars=lenv.Dictionary()) != "/usr/lib"]
        libincs.append('/usr/lib')

    return statlibs, libincs

def setup_syslibs(lenv):
    syslibs = []

    if not lenv['WITH_BF_FREETYPE_STATIC']:
        syslibs += Split(lenv['BF_FREETYPE_LIB'])
    if lenv['WITH_BF_PYTHON'] and not lenv['WITH_BF_STATICPYTHON']:
        if lenv['BF_DEBUG'] and lenv['OURPLATFORM'] in ('win32-vc', 'win64-vc', 'win32-mingw', 'win64-mingw'):
            syslibs.append(lenv['BF_PYTHON_LIB']+'_d')
        else:
            syslibs.append(lenv['BF_PYTHON_LIB'])
    if lenv['WITH_BF_OPENAL']:
        if not lenv['WITH_BF_STATICOPENAL']:
            syslibs += Split(lenv['BF_OPENAL_LIB'])
    if lenv['WITH_BF_OPENMP'] and lenv['CC'] != 'icc' and not lenv['WITH_BF_STATICOPENMP']:
        if lenv['CC'] == 'cl.exe':
            syslibs += ['vcomp']
        elif lenv['OURPLATFORM']=='darwin' and lenv['C_COMPILER_ID'] == 'clang' and lenv['CCVERSION'] >= '3.4': # clang-omp-3.4 !
            syslibs += ['iomp5']
        else:
            syslibs += ['gomp']
    if lenv['WITH_BF_ICONV']:
        syslibs += Split(lenv['BF_ICONV_LIB'])
    if lenv['WITH_BF_OIIO']:
        if not lenv['WITH_BF_STATICOIIO']:
            syslibs += Split(lenv['BF_OIIO_LIB'])

    if lenv['WITH_BF_OCIO']:
        if not lenv['WITH_BF_STATICOCIO']:
            syslibs += Split(lenv['BF_OCIO_LIB'])

    if lenv['WITH_BF_OPENEXR'] and not lenv['WITH_BF_STATICOPENEXR']:
        syslibs += Split(lenv['BF_OPENEXR_LIB'])
    if lenv['WITH_BF_ZLIB'] and not lenv['WITH_BF_STATICZLIB']:
        syslibs += Split(lenv['BF_ZLIB_LIB'])
    if lenv['WITH_BF_TIFF'] and not lenv['WITH_BF_STATICTIFF']:
        syslibs += Split(lenv['BF_TIFF_LIB'])
    if lenv['WITH_BF_FFMPEG'] and not lenv['WITH_BF_STATICFFMPEG']:
        syslibs += Split(lenv['BF_FFMPEG_LIB'])
        if lenv['WITH_BF_OGG']:
            syslibs += Split(lenv['BF_OGG_LIB'])
    if lenv['WITH_BF_JACK'] and not lenv['WITH_BF_JACK_DYNLOAD']:
        syslibs += Split(lenv['BF_JACK_LIB'])
    if lenv['WITH_BF_SNDFILE'] and not lenv['WITH_BF_STATICSNDFILE']:
        syslibs += Split(lenv['BF_SNDFILE_LIB'])
    if lenv['WITH_BF_FFTW3'] and not lenv['WITH_BF_STATICFFTW3']:
        syslibs += Split(lenv['BF_FFTW3_LIB'])
    '''
    if lenv['WITH_BF_ELTOPO']:
        syslibs += Split(lenv['BF_LAPACK_LIB'])
    '''
    if lenv['WITH_BF_SDL']:
        syslibs += Split(lenv['BF_SDL_LIB'])
    if not lenv['WITH_BF_STATICOPENGL']:
        syslibs += Split(lenv['BF_OPENGL_LIB'])
    if lenv['OURPLATFORM'] in ('win32-vc', 'win32-mingw','linuxcross', 'win64-vc', 'win64-mingw'):
        syslibs += Split(lenv['BF_PTHREADS_LIB'])
    if lenv['WITH_BF_COLLADA'] and not lenv['WITH_BF_STATICOPENCOLLADA']:
        syslibs.append(lenv['BF_PCRE_LIB'])
        if lenv['BF_DEBUG'] and (lenv['OURPLATFORM'] != 'linux'):
            syslibs += [colladalib+'_d' for colladalib in Split(lenv['BF_OPENCOLLADA_LIB'])]
        else:
            syslibs += Split(lenv['BF_OPENCOLLADA_LIB'])
        syslibs.append(lenv['BF_EXPAT_LIB'])

    if lenv['WITH_BF_JEMALLOC']:
        if not lenv['WITH_BF_STATICJEMALLOC']:
            syslibs += Split(lenv['BF_JEMALLOC_LIB'])

    if lenv['OURPLATFORM']=='linux':
        if lenv['WITH_BF_3DMOUSE']:
            if not lenv['WITH_BF_STATIC3DMOUSE']:
                syslibs += Split(lenv['BF_3DMOUSE_LIB'])
                
    if lenv['WITH_BF_BOOST'] and not lenv['WITH_BF_STATICBOOST']:
        syslibs += Split(lenv['BF_BOOST_LIB'])
        
        if lenv['WITH_BF_INTERNATIONAL']:
            syslibs += Split(lenv['BF_BOOST_LIB_INTERNATIONAL'])

    if lenv['WITH_BF_CYCLES_OSL'] and not lenv['WITH_BF_STATICOSL']:
        syslibs += Split(lenv['BF_OSL_LIB'])

    if lenv['WITH_BF_LLVM'] and not lenv['WITH_BF_STATICLLVM']:
        syslibs += Split(lenv['BF_LLVM_LIB'])

    if not lenv['WITH_BF_STATICJPEG']:
        syslibs += Split(lenv['BF_JPEG_LIB'])

    if not lenv['WITH_BF_STATICPNG']:
        syslibs += Split(lenv['BF_PNG_LIB'])

    syslibs += lenv['LLIBS']

    return syslibs

def propose_priorities():
    print bc.OKBLUE+"Priorities:"+bc.ENDC
    for t in possible_types:
        print bc.OKGREEN+"\t"+t+bc.ENDC
        new_priority = 0
        curlib = libs[t]
        sortlist = curlib.keys()
        sortlist.sort()

        for sk in sortlist:
            v = curlib[sk]
            #for p,v in sorted(libs[t].iteritems()):
            print "\t\t",new_priority, v
            new_priority += 5

# emits the necessary file objects for creator.c, to be used in creating
# the final blender executable
def creator(env):
    sources = ['creator.c']# + Blender.buildinfo(env, "dynamic") + Blender.resources

    incs = ['#/intern/guardedalloc', '#/source/blender/blenlib', '#/source/blender/blenkernel', '#/source/blender/editors/include', '#/source/blender/blenloader', '#/source/blender/imbuf', '#/source/blender/renderconverter', '#/source/blender/render/extern/include', '#/source/blender/windowmanager', '#/source/blender/makesdna', '#/source/blender/makesrna', '#/source/gameengine/BlenderRoutines', '#/extern/glew/include', '#/source/blender/gpu', env['BF_OPENGL_INC']]

    defs = []

    if env['WITH_BF_BINRELOC']:
        incs.append('#/extern/binreloc/include')
        defs.append('WITH_BINRELOC')

    if env['WITH_BF_SDL']:
        defs.append('WITH_SDL')

    if env['WITH_BF_LIBMV']:
        incs.append('#/extern/libmv')
        defs.append('WITH_LIBMV')

    if env['WITH_BF_FFMPEG']:
        defs.append('WITH_FFMPEG')

    if env['WITH_BF_PYTHON']:
        incs.append('#/source/blender/python')
        defs.append('WITH_PYTHON')
        if env['BF_DEBUG']:
            defs.append('_DEBUG')

    if env['WITH_BF_FREESTYLE']:
        incs.append('#/source/blender/freestyle')
        defs.append('WITH_FREESTYLE')

    if env['OURPLATFORM'] in ('win32-vc', 'win32-mingw', 'linuxcross', 'win64-vc', 'win64-mingw'):
        incs.append(env['BF_PTHREADS_INC'])
        incs.append('#/intern/utfconv')

    env.Append(CPPDEFINES=defs)
    env.Append(CPPPATH=incs)
    obj = [env.Object(root_build_dir+'source/creator/creator/creator', ['#source/creator/creator.c'])]

    return obj

## TODO: see if this can be made in an emitter
def buildinfo(lenv, build_type):
    """
    Generate a buildinfo object
    """
    build_date = time.strftime ("%Y-%m-%d")
    build_time = time.strftime ("%H:%M:%S")

    if os.path.isdir(os.path.abspath('.git')):
        build_commit_timestamp = os.popen('git log -1 --format=%ct').read().strip()
        if not build_commit_timestamp:
            # Git command not found
            build_hash = 'unknown'
            build_commit_timestamp = '0'
            build_branch = 'unknown'
        else:
            import subprocess
            no_upstream = False

            process = subprocess.Popen(['git', 'rev-parse', '--short', '@{u}'],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            build_hash, stderr = process.communicate()
            build_hash = build_hash.strip()
            build_branch = os.popen('git rev-parse --abbrev-ref HEAD').read().strip()

            if build_hash == '':
                build_hash = os.popen('git rev-parse --short HEAD').read().strip()
                no_upstream = True

            # ## Check for local modifications
            has_local_changes = False

            # Update GIT index before getting dirty files
            os.system('git update-index -q --refresh')
            changed_files = os.popen('git diff-index --name-only HEAD --').read().strip()

            if changed_files:
                has_local_changes = True
            elif no_upstream == False:
                unpushed_log = os.popen('git log @{u}..').read().strip()
                has_local_changes = unpushed_log != ''

            if has_local_changes:
                build_branch += ' (modified)'
    else:
        build_hash = 'unknown'
        build_commit_timestamp = '0'
        build_branch = 'unknown'

    if lenv['BF_DEBUG']:
        build_type = "Debug"
        build_cflags = ' '.join(lenv['CFLAGS'] + lenv['CCFLAGS'] + lenv['BF_DEBUG_CCFLAGS'] + lenv['CPPFLAGS'])
        build_cxxflags = ' '.join(lenv['CCFLAGS'] + lenv['CXXFLAGS'] + lenv['CPPFLAGS'])
    else:
        build_type = "Release"
        build_cflags = ' '.join(lenv['CFLAGS'] + lenv['CCFLAGS'] + lenv['REL_CFLAGS'] + lenv['REL_CCFLAGS'] + lenv['CPPFLAGS'])
        build_cxxflags = ' '.join(lenv['CCFLAGS'] + lenv['CXXFLAGS'] + lenv['REL_CXXFLAGS'] + lenv['REL_CCFLAGS'] + lenv['CPPFLAGS'])

    build_linkflags = ' '.join(lenv['PLATFORM_LINKFLAGS'])

    obj = []
    if lenv['BF_BUILDINFO']:
        lenv.Append (CPPDEFINES = ['BUILD_TIME=\\"%s\\"'%(build_time),
                                    'BUILD_DATE=\\"%s\\"'%(build_date),
                                    'BUILD_TYPE=\\"%s\\"'%(build_type),
                                    'BUILD_HASH=\\"%s\\"'%(build_hash),
                                    'BUILD_COMMIT_TIMESTAMP=%s'%(build_commit_timestamp),
                                    'BUILD_BRANCH=\\"%s\\"'%(build_branch),
                                    'WITH_BUILDINFO',
                                    'BUILD_PLATFORM=\\"%s:%s\\"'%(platform.system(), platform.architecture()[0]),
                                    'BUILD_CFLAGS=\\"%s\\"'%(build_cflags),
                                    'BUILD_CXXFLAGS=\\"%s\\"'%(build_cxxflags),
                                    'BUILD_LINKFLAGS=\\"%s\\"'%(build_linkflags),
                                    'BUILD_SYSTEM=\\"SCons\\"'
                    ])

        lenv.Append (CPPPATH = [root_build_dir+'source/blender/blenkernel'])

        obj = [lenv.Object (root_build_dir+'source/creator/%s_buildinfo'%build_type, ['#source/creator/buildinfo.c'])]

    return obj

##### END LIB STUFF ############

##### ACTION STUFF #############

def my_print_cmd_line(self, s, target, source, env):
    sys.stdout.write(' ' * 70 + '\r')
    sys.stdout.flush()
    sys.stdout.write(s + "\r")
    sys.stdout.flush()

def my_compile_print(target, source, env):
    a = '%s' % (source[0])
    d, f = os.path.split(a)
    return bc.OKBLUE + "Compiling" + bc.ENDC + " ==> '" + bc.OKGREEN + ("%s" % f) + bc.ENDC + "'"

def my_moc_print(target, source, env):
    a = '%s' % (source[0])
    d, f = os.path.split(a)
    return bc.OKBLUE + "Creating MOC" + bc.ENDC + " ==> '" + bc.OKGREEN + ("%s" % f) + bc.ENDC + "'"

def my_linking_print(target, source, env):
    t = '%s' % (target[0])
    d, f = os.path.split(t)
    return bc.OKBLUE + "Linking library" + bc.ENDC + " ==> '" + bc.OKGREEN + ("%s" % f) + bc.ENDC + "'"

def my_program_print(target, source, env):
    t = '%s' % (target[0])
    d, f = os.path.split(t)
    return bc.OKBLUE + "Linking program" + bc.ENDC + " ==> '" + bc.OKGREEN + ("%s" % f) + bc.ENDC + "'"

def msvc_hack(env):
    static_lib = SCons.Tool.createStaticLibBuilder(env)
    program = SCons.Tool.createProgBuilder(env)
    
    env['BUILDERS']['Library'] = static_lib
    env['BUILDERS']['StaticLibrary'] = static_lib
    env['BUILDERS']['Program'] = program
        
def set_quiet_output(env):
    mycaction = Action("$CCCOM", strfunction=my_compile_print)
    myshcaction = Action("$SHCCCOM", strfunction=my_compile_print)
    mycppaction = Action("$CXXCOM", strfunction=my_compile_print)
    myshcppaction = Action("$SHCXXCOM", strfunction=my_compile_print)
    mylibaction = Action("$ARCOM", strfunction=my_linking_print)
    mylinkaction = Action("$LINKCOM", strfunction=my_program_print)

    static_ob, shared_ob = SCons.Tool.createObjBuilders(env)
    static_ob.add_action('.c', mycaction)
    static_ob.add_action('.cpp', mycppaction)
    static_ob.add_action('.cc', mycppaction)
    shared_ob.add_action('.c', myshcaction)
    shared_ob.add_action('.cc', myshcppaction)

    static_lib = SCons.Builder.Builder(action = mylibaction,
                                       emitter = '$LIBEMITTER',
                                       prefix = '$LIBPREFIX',
                                       suffix = '$LIBSUFFIX',
                                       src_suffix = '$OBJSUFFIX',
                                       src_builder = 'StaticObject')

    program = SCons.Builder.Builder(action = mylinkaction,
                                    emitter = '$PROGEMITTER',
                                    prefix = '$PROGPREFIX',
                                    suffix = '$PROGSUFFIX',
                                    src_suffix = '$OBJSUFFIX',
                                    src_builder = 'Object',
                                    target_scanner = SCons.Defaults.ProgScan)

    env['BUILDERS']['Object'] = static_ob
    env['BUILDERS']['StaticObject'] = static_ob
    env['BUILDERS']['StaticLibrary'] = static_lib
    env['BUILDERS']['Library'] = static_lib
    env['BUILDERS']['Program'] = program
    if env['BF_LINE_OVERWRITE']:
        SCons.Action._ActionAction.print_cmd_line = my_print_cmd_line

def untar_pybundle(from_tar,to_dir,exclude_re):
    tar= tarfile.open(from_tar, mode='r')
    exclude_re= list(exclude_re) #single re object or list of re objects
    debug= 0 #list files instead of unpacking
    good= []
    if debug: print '\nFiles not being unpacked:\n'
    for name in tar.getnames():
        is_bad= 0
        for r in exclude_re:
            if r.match(name):
                is_bad=1
                if debug: print name
                break
        if not is_bad:
            good.append(tar.getmember(name))
    if debug:
        print '\nFiles being unpacked:\n'
        for g in good:
            print g
    else:
        tar.extractall(to_dir, good)

def my_winpybundle_print(target, source, env):
    pass

def WinPyBundle(target=None, source=None, env=None):
    import re
    py_tar= env.subst( env['LCGDIR'] )
    if py_tar[0]=='#':
        py_tar= py_tar[1:]
    if env['BF_DEBUG']:
        py_tar+= '/release/python' + env['BF_PYTHON_VERSION'].replace('.','') + '_d.tar.gz'
    else:
        py_tar+= '/release/python' + env['BF_PYTHON_VERSION'].replace('.','') + '.tar.gz'

    py_target = env.subst( env['BF_INSTALLDIR'] )
    if py_target[0]=='#':
        py_target=py_target[1:]
    py_target = os.path.join(py_target, VERSION, 'python', 'lib')
    def printexception(func,path,ex):
        if os.path.exists(path): #do not report if path does not exist. eg on a fresh build.
            print str(func) + ' failed on ' + str(path)
    print "Trying to remove existing py bundle."
    shutil.rmtree(py_target, False, printexception)
    exclude_re=[re.compile('.*/test'),
                re.compile('^test'),
                re.compile('^distutils'),
                re.compile('^idlelib'),
                re.compile('^lib2to3'),
                re.compile('^tkinter'),
                re.compile('^_tkinter_d.pyd'),
                re.compile('^turtledemo'),
                re.compile('^turtle.py'),
                ]

    print "Unpacking '" + py_tar + "' to '" + py_target + "'"
    untar_pybundle(py_tar,py_target,exclude_re)

def  my_appit_print(target, source, env):
    a = '%s' % (target[0])
    d, f = os.path.split(a)
    return "making bundle for " + f

def AppIt(target=None, source=None, env=None):
    import shutil
    import commands
    import os.path
    
    
    a = '%s' % (target[0])
    builddir, b = os.path.split(a)
    libdir = env['LCGDIR'][1:]
    osxarch = env['MACOSX_ARCHITECTURE']
    installdir = env['BF_INSTALLDIR']
    print("compiled architecture: %s"%(osxarch))
    print("Installing to %s"%(installdir))
    # TODO, use tar.
    python_zip = 'python_' + osxarch + '.zip' # set specific python_arch.zip
    if env['WITH_OSX_STATICPYTHON']:
        print("unzipping to app-bundle: %s"%(python_zip))
    else:
        print("dynamic build - make sure to have python3.x-framework installed")
    bldroot = env.Dir('.').abspath
    binary = env['BINARYKIND']
     
    sourcedir = bldroot + '/release/darwin/%s.app' % binary
    sourceinfo = bldroot + "/release/darwin/%s.app/Contents/Info.plist"%binary
    targetinfo = installdir +'/' + "%s.app/Contents/Info.plist"%binary
    cmd = installdir + '/' +'%s.app'%binary
    
    if os.path.isdir(cmd):
        shutil.rmtree(cmd)
    shutil.copytree(sourcedir, cmd)
    cmd = "cat %s | sed s/\$\{MACOSX_BUNDLE_SHORT_VERSION_STRING\}/%s/ | "%(sourceinfo,VERSION)
    cmd += "sed s/\$\{MACOSX_BUNDLE_LONG_VERSION_STRING\}/%s,\ %s/g > %s"%(VERSION,time.strftime("%Y-%b-%d"),targetinfo)
    commands.getoutput(cmd)
    cmd = 'cp %s/%s %s/%s.app/Contents/MacOS/%s'%(builddir, binary,installdir, binary, binary)
    commands.getoutput(cmd)
    cmd = 'mkdir %s/%s.app/Contents/MacOS/%s/'%(installdir, binary, VERSION)
    commands.getoutput(cmd)
    cmd = installdir + '/%s.app/Contents/MacOS/%s'%(binary,VERSION)

    # blenderplayer doesn't need all the files
    if binary == 'blender':
        cmd = 'mkdir %s/%s.app/Contents/MacOS/%s/datafiles'%(installdir, binary, VERSION)
        commands.getoutput(cmd)
        cmd = 'cp -R %s/release/datafiles/fonts %s/%s.app/Contents/MacOS/%s/datafiles/'%(bldroot,installdir,binary,VERSION)
        commands.getoutput(cmd)
        cmd = 'cp -R %s/release/datafiles/locale/languages %s/%s.app/Contents/MacOS/%s/datafiles/locale/'%(bldroot, installdir, binary, VERSION)
        commands.getoutput(cmd)
        mo_dir = os.path.join(builddir[:-4], "locale")
        for f in os.listdir(mo_dir):
            cmd = 'ditto %s/%s %s/%s.app/Contents/MacOS/%s/datafiles/locale/%s/LC_MESSAGES/blender.mo'%(mo_dir, f, installdir, binary, VERSION, f[:-3])
            commands.getoutput(cmd)

        if env['WITH_BF_OCIO']:
            cmd = 'cp -R %s/release/datafiles/colormanagement %s/%s.app/Contents/MacOS/%s/datafiles/'%(bldroot,installdir,binary,VERSION)
            commands.getoutput(cmd)
        
        cmd = 'cp -R %s/release/scripts %s/%s.app/Contents/MacOS/%s/'%(bldroot,installdir,binary,VERSION)
        commands.getoutput(cmd)

        if VERSION_RELEASE_CYCLE == "release":
            cmd = 'rm -rf %s/%s.app/Contents/MacOS/%s/scripts/addons_contrib'%(installdir,binary,VERSION)
            commands.getoutput(cmd)

        if env['WITH_BF_CYCLES']:
            croot = '%s/intern/cycles' % (bldroot)
            cinstalldir = '%s/%s.app/Contents/MacOS/%s/scripts/addons/cycles' % (installdir,binary,VERSION)

            cmd = 'mkdir %s' % (cinstalldir)
            commands.getoutput(cmd)
            cmd = 'mkdir %s/kernel' % (cinstalldir)
            commands.getoutput(cmd)
            cmd = 'mkdir %s/lib' % (cinstalldir)
            commands.getoutput(cmd)
            cmd = 'cp -R %s/blender/addon/*.py %s/' % (croot, cinstalldir)
            commands.getoutput(cmd)
            cmd = 'cp -R %s/doc/license %s/license' % (croot, cinstalldir)
            commands.getoutput(cmd)
            cmd = 'cp -R %s/kernel/*.h %s/kernel/*.cl %s/kernel/*.cu %s/kernel/' % (croot, croot, croot, cinstalldir)
            commands.getoutput(cmd)
            cmd = 'cp -R %s/kernel/svm %s/kernel/closure %s/util/util_color.h %s/util/util_half.h %s/util/util_math.h %s/util/util_transform.h %s/util/util_types.h %s/kernel/' % (croot, croot, croot, croot, croot, croot, croot, cinstalldir)
            commands.getoutput(cmd)
            cmd = 'cp -R %s/../intern/cycles/kernel/*.cubin %s/lib/' % (builddir, cinstalldir)
            commands.getoutput(cmd)

            if env['WITH_BF_CYCLES_OSL']:
                cmd = 'mkdir %s/shader' % (cinstalldir)
                commands.getoutput(cmd)
                cmd = 'cp -R %s/kernel/shaders/*.h %s/shader' % (croot, cinstalldir)
                commands.getoutput(cmd)
                cmd = 'cp -R %s/../intern/cycles/kernel/shaders/*.oso %s/shader' % (builddir, cinstalldir)
                commands.getoutput(cmd)

    if env['WITH_OSX_STATICPYTHON']:
        cmd = 'mkdir %s/%s.app/Contents/MacOS/%s/python/'%(installdir,binary, VERSION)
        commands.getoutput(cmd)
        cmd = 'unzip -q %s/release/%s -d %s/%s.app/Contents/MacOS/%s/python/'%(libdir,python_zip,installdir,binary,VERSION)
        commands.getoutput(cmd)

    cmd = 'chmod +x  %s/%s.app/Contents/MacOS/%s'%(installdir,binary, binary)
    commands.getoutput(cmd)
    cmd = 'find %s/%s.app -name .svn -prune -exec rm -rf {} \;'%(installdir, binary)
    commands.getoutput(cmd)
    cmd = 'find %s/%s.app -name .DS_Store -exec rm -rf {} \;'%(installdir, binary)
    commands.getoutput(cmd)
    cmd = 'find %s/%s.app -name __MACOSX -exec rm -rf {} \;'%(installdir, binary)
    commands.getoutput(cmd)
    if env['C_COMPILER_ID'] == 'gcc' and env['CCVERSION'] >= '4.6.1': # for correct errorhandling with gcc >= 4.6.1 we need the gcc.dylib and gomp.dylib to link, thus distribute in app-bundle
        print "Bundling libgcc and libgomp"
        instname = env['BF_CXX']
        cmd = 'ditto --arch %s %s/lib/libgcc_s.1.dylib %s/%s.app/Contents/MacOS/lib/'%(osxarch, instname, installdir, binary) # copy libgcc
        commands.getoutput(cmd)
        cmd = 'install_name_tool -id @executable_path/lib/libgcc_s.1.dylib %s/%s.app/Contents/MacOS/lib/libgcc_s.1.dylib'%(installdir, binary) # change id of libgcc
        commands.getoutput(cmd)
        cmd = 'ditto --arch %s %s/lib/libgomp.1.dylib %s/%s.app/Contents/MacOS/lib/'%(osxarch, instname, installdir, binary) # copy libgomp
        commands.getoutput(cmd)
        cmd = 'install_name_tool -id @executable_path/lib/libgomp.1.dylib %s/%s.app/Contents/MacOS/lib/libgomp.1.dylib'%(installdir, binary) # change id of libgomp
        commands.getoutput(cmd)
        cmd = 'install_name_tool -change %s/lib/libgcc_s.1.dylib  @executable_path/lib/libgcc_s.1.dylib %s/%s.app/Contents/MacOS/lib/libgomp.1.dylib'%(instname, installdir, binary) # change ref to libgcc
        commands.getoutput(cmd)
        cmd = 'install_name_tool -change %s/lib/libgcc_s.1.dylib  @executable_path/lib/libgcc_s.1.dylib %s/%s.app/Contents/MacOS/%s'%(instname, installdir, binary, binary) # change ref to libgcc ( blender )
        commands.getoutput(cmd)
        cmd = 'install_name_tool -change %s/lib/libgomp.1.dylib  @executable_path/lib/libgomp.1.dylib %s/%s.app/Contents/MacOS/%s'%(instname, installdir, binary, binary) # change ref to libgomp ( blender )
        commands.getoutput(cmd)
    if env['C_COMPILER_ID'] == 'clang' and env['CCVERSION'] >= '3.4':
        print "Bundling libiomp5"
        instname = env['BF_CXX']
        cmd = 'ditto --arch %s %s/lib/libiomp5.dylib %s/%s.app/Contents/MacOS/lib/'%(osxarch, instname, installdir, binary) # copy libiomp5
        commands.getoutput(cmd)
        cmd = 'install_name_tool -id @executable_path/lib/libiomp5.dylib %s/%s.app/Contents/MacOS/lib/libiomp5.dylib'%(installdir, binary) # change id of libiomp5
        commands.getoutput(cmd)
        cmd = 'install_name_tool -change %s/lib/libiomp5.dylib  @executable_path/lib/libiomp5.dylib %s/%s.app/Contents/MacOS/%s'%(instname, installdir, binary, binary) # change ref to libiomp5 ( blender )
        commands.getoutput(cmd)

# extract copy system python, be sure to update other build systems
# when making changes to the files that are copied.
def my_unixpybundle_print(target, source, env):
    pass

def UnixPyBundle(target=None, source=None, env=None):
    # Any Unix except osx
    #-- VERSION/python/lib/python3.1
    
    import commands
    
    def run(cmd):
        print 'Install command:', cmd
        commands.getoutput(cmd)

    dir = os.path.join(env['BF_INSTALLDIR'], VERSION)

    lib = env['BF_PYTHON_LIBPATH'].split(os.sep)[-1]
    target_lib = "lib64" if lib == "lib64" else "lib"

    py_src =    env.subst( env['BF_PYTHON_LIBPATH'] + '/python'+env['BF_PYTHON_VERSION'] )
    py_target =    env.subst( dir + '/python/' + target_lib + '/python'+env['BF_PYTHON_VERSION'] )
    
    # This is a bit weak, but dont install if its been installed before, makes rebuilds quite slow.
    if os.path.exists(py_target):
        print 'Using existing python from:'
        print '\t"%s"' %            py_target
        print '\t(skipping copy)\n'
        return

    # Copied from source/creator/CMakeLists.txt, keep in sync.
    print 'Install python from:'
    print '\t"%s" into...' % py_src
    print '\t"%s"\n' % py_target

    run("rm -rf '%s'" % py_target)
    try:
        os.makedirs(os.path.dirname(py_target)) # the final part is copied
    except:
        pass

    run("cp -R '%s' '%s'" % (py_src, os.path.dirname(py_target)))
    run("rm -rf '%s/distutils'" % py_target)
    run("rm -rf '%s/lib2to3'" % py_target)
    run("rm -rf '%s/config'" % py_target)

    for f in os.listdir(py_target):
        if f.startswith("config-"):
            run("rm -rf '%s/%s'" % (py_target, f))

    run("rm -rf '%s/site-packages'" % py_target)
    run("mkdir '%s/site-packages'" % py_target)    # python needs it.'
    run("rm -rf '%s/idlelib'" % py_target)
    run("rm -rf '%s/tkinter'" % py_target)
    run("rm -rf '%s/turtledemo'" % py_target)
    run("rm -r '%s/turtle.py'" % py_target)
    run("rm -f '%s/lib-dynload/_tkinter.so'" % py_target)

    if env['WITH_BF_PYTHON_INSTALL_NUMPY']:
        numpy_src = py_src + "/site-packages/numpy"
        numpy_target = py_target + "/site-packages/numpy"

        if os.path.exists(numpy_src):
            print 'Install numpy from:'
            print '\t"%s" into...' % numpy_src
            print '\t"%s"\n' % numpy_target

            run("cp -R '%s' '%s'" % (numpy_src, os.path.dirname(numpy_target)))
            run("rm -rf '%s/distutils'" % numpy_target)
            run("rm -rf '%s/oldnumeric'" % numpy_target)
            run("rm -rf '%s/doc'" % numpy_target)
            run("rm -rf '%s/tests'" % numpy_target)
            run("rm -rf '%s/f2py'" % numpy_target)
            run("find '%s' -type d -name 'include' -prune -exec rm -rf {} ';'" % numpy_target)
            run("find '%s' -type d -name '*.h' -prune -exec rm -rf {} ';'" % numpy_target)
            run("find '%s' -type d -name '*.a' -prune -exec rm -rf {} ';'" % numpy_target)
        else:
            print 'Failed to find numpy at %s, skipping copying' % numpy_src

    run("find '%s' -type d -name 'test' -prune -exec rm -rf {} ';'" % py_target)
    run("find '%s' -type d -name '__pycache__' -exec rm -rf {} ';'" % py_target)
    run("find '%s' -name '*.py[co]' -exec rm -rf {} ';'" % py_target)
    run("find '%s' -name '*.so' -exec strip -s {} ';'" % py_target)

#### END ACTION STUFF #########

def bsc(env, target, source):
    
    bd = os.path.dirname(target[0].abspath)
    bscfile = '\"'+target[0].abspath+'\"'
    bscpathcollect = '\"'+bd + os.sep + '*.sbr\"'
    bscpathtmp = '\"'+bd + os.sep + 'bscmake.tmp\"'

    os.system('dir /b/s '+bscpathcollect+' >'+bscpathtmp)

    myfile = open(bscpathtmp[1:-1], 'r')
    lines = myfile.readlines()
    myfile.close()

    newfile = open(bscpathtmp[1:-1], 'w')
    for l in lines:
        newfile.write('\"'+l[:-1]+'\"\n')
    newfile.close()
                
    os.system('bscmake /nologo /n /o'+bscfile+' @'+bscpathtmp)
    os.system('del '+bscpathtmp)

class BlenderEnvironment(SConsEnvironment):

    PyBundleActionAdded = False

    def BlenderRes(self=None, libname=None, source=None, libtype=['core'], priority=[100]):
        global libs
        if not self or not libname or not source:
            print bc.FAIL+'Cannot continue.  Missing argument for BlenderRes '+libname+bc.ENDC
            self.Exit()
        if self['OURPLATFORM'] not in ('win32-vc','win32-mingw','linuxcross', 'win64-vc', 'win64-mingw'):
            print bc.FAIL+'BlenderRes is for windows only!'+bc.END
            self.Exit()
        
        print bc.HEADER+'Configuring resource '+bc.ENDC+bc.OKGREEN+libname+bc.ENDC
        lenv = self.Clone()
        if not (root_build_dir[0]==os.sep or root_build_dir[1]==':'):
            res = lenv.RES('#'+root_build_dir+'lib/'+libname, source)
        else:
            res = lenv.RES(root_build_dir+'lib/'+libname, source)

        
        SConsEnvironment.Default(self, res)
        resources.append(res)

    def BlenderLib(self=None, libname=None, sources=None, includes=[], defines=[], libtype='common', priority = 100, compileflags=None, cc_compileflags=None, cxx_compileflags=None, cc_compilerchange=None, cxx_compilerchange=None):
        global vcp
        
        # sanity check
        # run once in a while to check we dont have duplicates
        if 0:
            for name, dirs in (("source", sources), ("include", includes)):
                files_clean = [os.path.normpath(f) for f in dirs]
                files_clean_set = set(files_clean)
                if len(files_clean) != len(files_clean_set):
                    for f in sorted(files_clean_set):
                        if f != '.' and files_clean.count(f) > 1:
                            raise Exception("Found duplicate %s %r" % (name, f))
            del name, dirs, files_clean, files_clean_set, f
        # end sanity check

        if not self or not libname or not sources:
            print bc.FAIL+'Cannot continue. Missing argument for BuildBlenderLib '+libname+bc.ENDC
            self.Exit()

        def list_substring(quickie, libname):
            for q in quickie:
                if q in libname:
                    return True
            return False

        if list_substring(quickie, libname) or len(quickie)==0:
            if list_substring(quickdebug, libname):
                print bc.HEADER+'Configuring library '+bc.ENDC+bc.OKGREEN+libname +bc.ENDC+bc.OKBLUE+ " (debug mode)" + bc.ENDC
            else:
                print bc.HEADER+'Configuring library '+bc.ENDC+bc.OKGREEN+libname + bc.ENDC
            lenv = self.Clone()
            lenv.Append(CPPPATH=includes)
            lenv.Append(CPPDEFINES=defines)
            if lenv['BF_DEBUG'] or (libname in quickdebug):
                lenv.Append(CFLAGS = lenv['BF_DEBUG_CFLAGS'])
                lenv.Append(CCFLAGS = lenv['BF_DEBUG_CCFLAGS'])
                lenv.Append(CXXFLAGS = lenv['BF_DEBUG_CXXFLAGS'])
            else:
                lenv.Append(CFLAGS = lenv['REL_CFLAGS'])
                lenv.Append(CCFLAGS = lenv['REL_CCFLAGS'])
                lenv.Append(CXXFLAGS = lenv['REL_CXXFLAGS'])
            if lenv['BF_PROFILE']:
                lenv.Append(CFLAGS = lenv['BF_PROFILE_CFLAGS'])
                lenv.Append(CCFLAGS = lenv['BF_PROFILE_CCFLAGS'])
                lenv.Append(CXXFLAGS = lenv['BF_PROFILE_CXXFLAGS'])
            if compileflags:
                lenv.Replace(CFLAGS = compileflags)
            if cc_compileflags:
                lenv.Replace(CCFLAGS = cc_compileflags)
            if cxx_compileflags:
                lenv.Replace(CXXFLAGS = cxx_compileflags)
            if cc_compilerchange:
                lenv.Replace(CC = cc_compilerchange)
            if cxx_compilerchange:
                lenv.Replace(CXX = cxx_compilerchange)
            lenv.Append(CFLAGS = lenv['C_WARN'])
            lenv.Append(CCFLAGS = lenv['CC_WARN'])
            lenv.Append(CXXFLAGS = lenv['CXX_WARN'])

            if lenv['OURPLATFORM'] == 'win64-vc':
                lenv.Append(LINKFLAGS = ['/MACHINE:X64'])

            if lenv['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
                if lenv['BF_DEBUG']:
                    lenv.Append(CCFLAGS = ['/MTd'])
                else:
                    lenv.Append(CCFLAGS = ['/MT'])
            
            targetdir = root_build_dir+'lib/' + libname
            if not (root_build_dir[0]==os.sep or root_build_dir[1]==':'):
                targetdir = '#'+targetdir
            lib = lenv.Library(target= targetdir, source=sources)
            SConsEnvironment.Default(self, lib) # we add to default target, because this way we get some kind of progress info during build
            if self['BF_MSVS'] and self['OURPLATFORM'] in ('win32-vc', 'win64-vc'):
                #if targetdir[0] == '#':
                #    targetdir = targetdir[1:-1]
                print "! ",targetdir+ '.vcproj' # + self['MSVSPROJECTSUFFIX']
                vcproject = self.MSVSProject(target = targetdir + '.vcproj', # + self['MSVSPROJECTSUFFIX'],
                         srcs = sources,
                         buildtarget = lib,
                         variant = 'Release',
                         auto_build_solution=0)
                vcp.append(vcproject)
                SConsEnvironment.Default(self, vcproject)
        else:
            print bc.WARNING+'Not building '+bc.ENDC+bc.OKGREEN+libname+bc.ENDC+' for '+bc.OKBLUE+'BF_QUICK'+bc.ENDC
        # note: libs is a global
        add_lib_to_dict(self, libs, libtype, libname, priority)

    def BlenderProg(self=None, builddir=None, progname=None, sources=None, libs=None, libpath=None, binarykind=''):
        global vcp
        print bc.HEADER+'Configuring program '+bc.ENDC+bc.OKGREEN+progname+bc.ENDC
        lenv = self.Clone()
        lenv.Append(LINKFLAGS = lenv['PLATFORM_LINKFLAGS'])
        lenv.Append(LINKFLAGS = lenv['BF_PROGRAM_LINKFLAGS'])
        if lenv['OURPLATFORM'] in ('win32-mingw', 'win64-mingw', 'linuxcross', 'cygwin', 'linux'):
            lenv.Replace(LINK = '$CXX')
        if lenv['OURPLATFORM'] in ('win32-vc', 'cygwin', 'win64-vc'):
            if lenv['BF_DEBUG']:
                lenv.Prepend(LINKFLAGS = ['/DEBUG','/PDB:'+progname+'.pdb','/NODEFAULTLIB:libcmt'])
        if  lenv['OURPLATFORM']=='linux':
            if lenv['WITH_BF_PYTHON']:
                lenv.Append(LINKFLAGS = lenv['BF_PYTHON_LINKFLAGS'])
        if  lenv['OURPLATFORM']=='sunos5':
            if lenv['WITH_BF_PYTHON']:
                lenv.Append(LINKFLAGS = lenv['BF_PYTHON_LINKFLAGS'])
            if lenv['CXX'].endswith('CC'):
                lenv.Replace(LINK = '$CXX')
        if  lenv['OURPLATFORM']=='darwin':
            if lenv['WITH_BF_PYTHON']:
                lenv.Append(LINKFLAGS = lenv['BF_PYTHON_LINKFLAGS'])
            lenv.Append(LINKFLAGS = lenv['BF_OPENGL_LINKFLAGS'])
        if lenv['BF_PROFILE']:
            lenv.Append(LINKFLAGS = lenv['BF_PROFILE_LINKFLAGS'])
        if root_build_dir[0]==os.sep or root_build_dir[1]==':':
            lenv.Append(LIBPATH=root_build_dir + '/lib')
        lenv.Append(LIBPATH=libpath)
        lenv.Append(LIBS=libs)
        if lenv['WITH_BF_QUICKTIME']:
            lenv.Append(LIBS = lenv['BF_QUICKTIME_LIB'])
            lenv.Append(LIBPATH = lenv['BF_QUICKTIME_LIBPATH'])
        prog = lenv.Program(target=builddir+'bin/'+progname, source=sources)
        if lenv['BF_DEBUG'] and lenv['OURPLATFORM'] in ('win32-vc', 'win64-vc') and lenv['BF_BSC']:
            f = lenv.File(progname + '.bsc', builddir)
            brs = lenv.Command(f, prog, [bsc])
            SConsEnvironment.Default(self, brs)
        SConsEnvironment.Default(self, prog)
        if self['BF_MSVS'] and self['OURPLATFORM'] in ('win32-vc', 'win64-vc') and progname == 'blender':
            print "! ",builddir + "/" + progname + '.sln'
            sln = self.MSVSProject(target = builddir + "/" + progname + '.sln',
                     projects= vcp,
                     variant = 'Release')
            SConsEnvironment.Default(self, sln)
        program_list.append(prog)
        if  lenv['OURPLATFORM']=='darwin':
            lenv['BINARYKIND'] = binarykind
            lenv.AddPostAction(prog,Action(AppIt,strfunction=my_appit_print))
        elif os.sep == '/' and lenv['OURPLATFORM'] != 'linuxcross': # any unix (except cross-compilation)
            if lenv['WITH_BF_PYTHON']:
                if (not lenv['WITHOUT_BF_INSTALL'] and 
                    not lenv['WITHOUT_BF_PYTHON_INSTALL'] and 
                    not lenv['WITHOUT_BF_PYTHON_UNPACK'] and 
                    not BlenderEnvironment.PyBundleActionAdded):
                    lenv.AddPostAction(prog,Action(UnixPyBundle,strfunction=my_unixpybundle_print))
                    BlenderEnvironment.PyBundleActionAdded = True
        elif lenv['OURPLATFORM'].startswith('win') or lenv['OURPLATFORM'] == 'linuxcross': # windows or cross-compilation
            if lenv['WITH_BF_PYTHON']:
                if (not lenv['WITHOUT_BF_PYTHON_INSTALL'] and 
                    not lenv['WITHOUT_BF_PYTHON_UNPACK'] and 
                    not BlenderEnvironment.PyBundleActionAdded):
                    lenv.AddPostAction(prog,Action(WinPyBundle,strfunction=my_winpybundle_print))
                    BlenderEnvironment.PyBundleActionAdded = True
        return prog

    def Glob(lenv, pattern):
        path = string.replace(GetBuildPath(lenv,'SConscript'),'SConscript', '')
        files = []
        for i in glob.glob(path + pattern):
            files.append(string.replace(i, path, ''))
        return files
