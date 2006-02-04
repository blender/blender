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

import os.path
import string
import glob
import time
import sys

from SCons.Script.SConscript import SConsEnvironment
import SCons.Action
import SCons.Util
import SCons.Builder
import SCons.Tool
import bcolors
bc = bcolors.bcolors()

Split = SCons.Util.Split
Action = SCons.Action.Action
Builder = SCons.Builder.Builder
GetBuildPath = SConsEnvironment.GetBuildPath

# a few globals
root_build_dir = ''
quickie = None # Anything else than None if BF_QUICK has been passed
quicklist = [] # The list of libraries/programs to compile during a quickie
program_list = [] # A list holding Nodes to final binaries, used to create installs
arguments = None
targets = None

#some internals
blenderdeps = [] # don't manipulate this one outside this module!

##### LIB STUFF ##########

possible_types = ['core'] # can be set in ie. SConstruct
libs = {}
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
def add_lib_to_dict(dict = None, libtype = None, libname = None, priority = 100):
    if not dict or not libtype or not libname:
        print "Passed wrong arg"
        Exit()

    if type(libtype) is str and type(priority) is int:
        internal_lib_to_dict(dict, libtype, libname, priority)
    elif type(libtype) is list and type(priority) is list:
        if len(libtype)==len(priority):
            for lt, p in zip(libtype, priority):
                internal_lib_to_dict(dict, lt, libname, p)
        else:
            print "libtype and priority lists are unequal in length"
            Exit()
    else:
        print "Wrong type combinations for libtype and priority. Only str and int or list and list"
        Exit()

#libs = init_lib_dict(libs)

def create_blender_liblist(lenv = None, libtype = None):
    if not lenv or not libtype:
        print "missing arg"

    lst = []
    if libtype in possible_types:
        sortlist = []
        for k,v in libs[libtype].iteritems():
            sortlist.append(k)
        sortlist.sort()
        curlib = libs[libtype]
        for sk in sortlist:
            v = curlib[sk]
        #for k,v in sorted(libs[libtype].iteritems()):
            lst.append('#' + root_build_dir + 'lib/'+lenv['LIBPREFIX'] + v + lenv['LIBSUFFIX'])

    return lst

## TODO: static linking
def setup_staticlibs(lenv):
    statlibs = [
        #here libs for static linking
    ]
    libincs = [
        '/usr/lib',
        lenv['BF_PYTHON_LIBPATH'],
        lenv['BF_OPENGL_LIBPATH'],
        lenv['BF_SDL_LIBPATH'],
        lenv['BF_JPEG_LIBPATH'],
        lenv['BF_TIFF_LIBPATH'],
        lenv['BF_PNG_LIBPATH'],
        lenv['BF_GETTEXT_LIBPATH'],
        lenv['BF_ZLIB_LIBPATH'],
        lenv['BF_OPENAL_LIBPATH'],
        lenv['BF_FREETYPE_LIBPATH'],
#        lenv['BF_QUICKTIME_LIBPATH'],
        lenv['BF_ICONV_LIBPATH']
        ]
    libincs += Split(lenv['BF_OPENEXR_LIBPATH'])

    return statlibs, libincs

def setup_syslibs(lenv):
    syslibs = [
        lenv['BF_PYTHON_LIB'],
        lenv['BF_JPEG_LIB'],
        lenv['BF_PNG_LIB'],
        lenv['BF_ZLIB_LIB'],
        lenv['BF_OPENAL_LIB'],
        lenv['BF_FREETYPE_LIB'],
        lenv['BF_GETTEXT_LIB']

        #here libs for linking
        ]
    if lenv['OURPLATFORM']=='win32vc':
            syslibs += Split(lenv['BF_ICONV_LIB'])
    syslibs += Split(lenv['BF_TIFF_LIB'])
    syslibs += Split(lenv['BF_OPENEXR_LIB'])
    syslibs += Split(lenv['BF_SDL_LIB'])
    syslibs += Split(lenv['BF_OPENGL_LIB'])
    syslibs += Split(lenv['LLIBS'])

    return syslibs

def propose_priorities():
    print bc.OKBLUE+"Priorities:"+bc.ENDC
    for t in possible_types:
        print bc.OKGREEN+"\t"+t+bc.ENDC
        new_priority = 0
        sortlist = []
        for k,v in libs[t].iteritems():
            sortlist.append(k)
        sortlist.sort()
        curlib = libs[t]
        for sk in sortlist:
            v = curlib[sk]
            #for p,v in sorted(libs[t].iteritems()):
            print "\t\t",new_priority, v
            new_priority += 5

## TODO: see if this can be made in an emitter
def buildinfo(lenv, build_type):
    """
    Generate a buildinfo object
    """
    build_date = time.strftime ("%Y-%m-%d")
    build_time = time.strftime ("%H:%M:%S")
    obj = []
    if lenv['BF_BUILDINFO']==1: #user_options_dict['USE_BUILDINFO'] == 1:
        if sys.platform=='win32':
            build_info_file = open("source/creator/winbuildinfo.h", 'w')
            build_info_file.write("char *build_date=\"%s\";\n"%build_date)
            build_info_file.write("char *build_time=\"%s\";\n"%build_time)
            build_info_file.write("char *build_platform=\"win32\";\n")
            build_info_file.write("char *build_type=\"dynamic\";\n")
            build_info_file.close()
            lenv.Append (CPPDEFINES = ['NAN_BUILDINFO', 'BUILD_DATE'])
        else:
            lenv.Append (CPPDEFINES = ['BUILD_TIME=\'"%s"\''%(build_time),
                                        'BUILD_DATE=\'"%s"\''%(build_date),
                                        'BUILD_TYPE=\'"dynamic"\'',
                                        'NAN_BUILDINFO',
                                        'BUILD_PLATFORM=\'"%s"\''%(sys.platform)])
        obj = [lenv.Object (root_build_dir+'source/creator/%s_buildinfo'%build_type,
                        [root_build_dir+'source/creator/buildinfo.c'])]
    return obj

##### END LIB STUFF ############

##### ACTION STUFF #############

def my_compile_print(target, source, env):
    a = '%s' % (source[0])
    d, f = os.path.split(a)
    return bc.OKBLUE+"Compiling"+bc.ENDC +" ==> '"+bc.OKGREEN+"%s" % (f) + "'"+bc.ENDC

def my_moc_print(target, source, env):
    a = '%s' % (source[0])
    d, f = os.path.split(a)
    return bc.OKBLUE+"Creating MOC"+bc.ENDC+ " ==> '"+bc.OKGREEN+"%s" %(f) + "'"+bc.ENDC

def my_linking_print(target, source, env):
    t = '%s' % (target[0])
    d, f = os.path.split(t)
    return bc.OKBLUE+"Linking library"+bc.ENDC +" ==> '"+bc.OKGREEN+"%s" % (f) + "'"+bc.ENDC

def my_program_print(target, source, env):
    t = '%s' % (target[0])
    d, f = os.path.split(t)
    return bc.OKBLUE+"Linking program"+bc.ENDC +" ==> '"+bc.OKGREEN+"%s" % (f) + "'"+bc.ENDC

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
    shared_ob.add_action('.c', myshcaction)
    shared_ob.add_action('.cpp', myshcppaction)

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


#### END ACTION STUFF #########

class BlenderEnvironment(SConsEnvironment):

    def BlenderLib(self=None, libname=None, sources=None, includes=[], defines=[], libtype='common', priority = 100, compileflags=None):
        if not self or not libname or not sources:
            print bc.FAIL+'Cannot continue. Missing argument for BuildBlenderLib '+libname+bc.ENDC
            Exit()
        if libname in quickie or len(quickie)==0:
            print bc.HEADER+'Configuring library '+bc.ENDC+bc.OKGREEN+libname+bc.ENDC
            lenv = self.Copy()
            lenv.Append(CPPPATH=includes)
            lenv.Append(CPPDEFINES=defines)
            if lenv['WITH_BF_GAMEENGINE']:
                    lenv.Append(CPPDEFINES=['GAMEBLENDER=1'])
            if lenv['BF_DEBUG']:
                    lenv.Append(CFLAGS = lenv['BF_DEBUG_FLAGS'], CCFLAGS = lenv['BF_DEBUG_FLAGS'])
            else:
                    lenv.Append(CFLAGS = lenv['REL_CFLAGS'], CCFLAGS = lenv['REL_CCFLAGS'])
            if lenv['BF_PROFILE']:
                    lenv.Append(CFLAGS = lenv['BF_PROFILE_FLAGS'], CCFLAGS = lenv['BF_PROFILE_FLAGS'])
            if compileflags:
                lenv.Append(CFLAGS = compileflags)
                lenv.Append(CCFLAGS = compileflags)
            lenv.Append(CFLAGS = Split(lenv['C_WARN']))
            lenv.Append(CCFLAGS = Split(lenv['CC_WARN']))
            lib = lenv.Library(target= '#'+root_build_dir+'lib/'+libname, source=sources)
            SConsEnvironment.Default(self, lib) # we add to default target, because this way we get some kind of progress info during build
        else:
            print bc.WARNING+'Not building '+bc.ENDC+bc.OKGREEN+libname+bc.ENDC+' for '+bc.OKBLUE+'BF_QUICK'+bc.ENDC
        # note: libs is a global
        add_lib_to_dict(libs, libtype, libname, priority)

    def BlenderProg(self=None, builddir=None, progname=None, sources=None, includes=None, libs=None, libpath=None):
        print bc.HEADER+'Configuring program '+bc.ENDC+bc.OKGREEN+progname+bc.ENDC
        lenv = self.Copy()
        if lenv['OURPLATFORM']=='win32-vc':
            lenv.Append(LINKFLAGS = Split(lenv['PLATFORM_LINKFLAGS']))
        if  lenv['OURPLATFORM']=='darwin':
            lenv.Append(LINKFLAGS = lenv['PLATFORM_LINKFLAGS'])
            lenv.Append(LINKFLAGS = lenv['BF_PYTHON_LINKFLAGS'])
            lenv.Append(LINKFLAGS = lenv['BF_OPENGL_LINKFLAGS'])
        lenv.Append(CPPPATH=includes)
        lenv.Append(LIBPATH=libpath)
        lenv.Append(LIBS=libs)
        if lenv['WITH_BF_QUICKTIME']:
             lenv.Append(LIBS = lenv['BF_QUICKTIME_LIB'])
             lenv.Append(LIBPATH = lenv['BF_QUICKTIME_LIBPATH'])
        prog = lenv.Program(target=builddir+'bin/'+progname, source=sources)
        SConsEnvironment.Default(self, prog)
        program_list.append(prog)

## TODO: have register for libs/programs, so that we test only that
#  which have expressed their need to be tested in their own sconscript
    def BlenderUnitTest(env, source, **kwargs):
        test = env.Program(source, **kwargs)
        env.AddPostAction(test, test[0].abspath)
        env.Alias('check', test)
        env.AlwaysBuild(test)
        return test

    def Glob(lenv, pattern):
        path = string.replace(GetBuildPath(lenv,'SConscript'),'SConscript', '')
        files = []
        for i in glob.glob(path + pattern):
            files.append(string.replace(i, path, ''))
        return files
