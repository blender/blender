
import os
import os.path
import SCons.Options
try:
    import SCons.Options.BoolOption
except ImportError:
    pass
try:
    import subprocess
except ImportError:
    pass
import string
import glob
import shutil
import sys

Options = SCons.Options
BoolOption = SCons.Options.BoolOption

def print_arguments(args, bc):
    if len(args):
        for k,v in args.iteritems():
            print '\t'+bc.OKBLUE+k+bc.ENDC+' = '+bc.OKGREEN + v + bc.ENDC
    else:
        print '\t'+bc.WARNING+'No  command-line arguments given'+bc.ENDC

def validate_arguments(args, bc):
    opts_list = [
            'BF_PYTHON', 'BF_PYTHON_VERSION', 'BF_PYTHON_INC', 'BF_PYTHON_BINARY', 'BF_PYTHON_LIB', 'BF_PYTHON_LIBPATH', 'BF_PYTHON_LINKFLAGS',
            'WITH_BF_OPENAL', 'BF_OPENAL', 'BF_OPENAL_INC', 'BF_OPENAL_LIB', 'BF_OPENAL_LIBPATH',
            'WITH_BF_SDL', 'BF_SDL', 'BF_SDL_INC', 'BF_SDL_LIB', 'BF_SDL_LIBPATH',
            'BF_PTHREADS', 'BF_PTHREADS_INC', 'BF_PTHREADS_LIB', 'BF_PTHREADS_LIBPATH',
            'WITH_BF_FMOD',
            'WITH_BF_OPENEXR', 'BF_OPENEXR', 'BF_OPENEXR_INC', 'BF_OPENEXR_LIB', 'BF_OPENEXR_LIBPATH',
            'WITH_BF_DDS',
            'WITH_BF_FFMPEG', 'BF_FFMPEG_LIB','BF_FFMPEG_EXTRA', 'BF_FFMPEG',  'BF_FFMPEG_INC',
            'WITH_BF_JPEG', 'BF_JPEG', 'BF_JPEG_INC', 'BF_JPEG_LIB', 'BF_JPEG_LIBPATH',
            'WITH_BF_PNG', 'BF_PNG', 'BF_PNG_INC', 'BF_PNG_LIB', 'BF_PNG_LIBPATH',
            'BF_TIFF', 'BF_TIFF_INC',
            'WITH_BF_ZLIB', 'BF_ZLIB', 'BF_ZLIB_INC', 'BF_ZLIB_LIB', 'BF_ZLIB_LIBPATH',
            'WITH_BF_INTERNATIONAL',
            'BF_GETTEXT', 'BF_GETTEXT_INC', 'BF_GETTEXT_LIB', 'BF_GETTEXT_LIBPATH',
            'WITH_BF_ICONV', 'BF_ICONV', 'BF_ICONV_INC', 'BF_ICONV_LIB', 'BF_ICONV_LIBPATH',
            'WITH_BF_ODE', 'BF_ODE', 'BF_ODE_INC', 'BF_ODE_LIB',
            'WITH_BF_GAMEENGINE', 'WITH_BF_BULLET', 'BF_BULLET', 'BF_BULLET_INC', 'BF_BULLET_LIB',
            'BF_SOLID', 'BF_SOLID_INC', 'BF_WINTAB', 'BF_WINTAB_INC',
            'WITH_BF_YAFRAY',
            'BF_FREETYPE', 'BF_FREETYPE_INC', 'BF_FREETYPE_LIB', 'BF_FREETYPE_LIBPATH',
            'WITH_BF_QUICKTIME', 'BF_QUICKTIME', 'BF_QUICKTIME_INC', 'BF_QUICKTIME_LIB', 'BF_QUICKTIME_LIBPATH',
            'WITH_BF_STATICOPENGL', 'BF_OPENGL', 'BF_OPENGL_INC', 'BF_OPENGL_LIB', 'BF_OPENGL_LIBPATH', 'BF_OPENGL_LIB_STATIC', 'BF_OPENGL_LINKFLAGS',
            'WITH_BF_FTGL', 'BF_FTGL', 'BF_FTGL_INC', 'BF_FTGL_LIB',
            'WITH_BF_PLAYER',
	    'WITH_BF_GLEXT',
            'WITH_BF_BINRELOC',	
            'CFLAGS', 'CCFLAGS', 'CPPFLAGS', 
            'REL_CFLAGS', 'REL_CCFLAGS',
            'C_WARN', 'CC_WARN', 'LLIBS', 'PLATFORM_LINKFLAGS',
            'BF_PROFILE_FLAGS', 'LCGDIR', 'WITH_BF_VERSE', 
            'BF_VERSE_INCLUDE',
            'VERSE_BUILD_BINARY', 'VERSE_BUILD_DIR', 'VERSE_REGEN_PROTO',
            'BF_TWEAK_MODE', 'BF_SPLIT_SRC',
            'WITHOUT_BF_INSTALL',
            'WITH_BF_OPENMP',
            'WITHOUT_BF_INSTALL',
            'BF_FANCY', 'BF_QUIET',
            'BF_X264_CONFIG',
            'BF_XVIDCORE_CONFIG',
            ]

    arg_list = ['BF_DEBUG', 'BF_QUIET', 'BF_CROSS', 'BF_UPDATE',
            'BF_INSTALLDIR', 'BF_TOOLSET', 'BF_BINNAME',
            'BF_BUILDDIR', 'BF_FANCY', 'BF_QUICK', 'BF_PROFILE',
            'BF_DEBUG_FLAGS', 'BF_BSC', 'BF_CONFIG',
            'BF_PRIORITYLIST', 'BF_BUILDINFO','CC', 'CXX', 'BF_QUICKDEBUG',
            'BF_LISTDEBUG', 'LCGDIR', 'BF_X264_CONFIG', 'BF_XVIDCORE_CONFIG']

    all_list = opts_list + arg_list
    okdict = {}

    for k,v in args.iteritems():
        if k in all_list:
            okdict[k] = v
        else:
            print '\t'+bc.WARNING+'Invalid argument: '+bc.ENDC+k+'='+v

    return okdict

def print_targets(targs, bc):
    if len(targs)>0:
        for t in targs:
            print '\t'+bc.OKBLUE+t+bc.ENDC
    else:
        print '\t'+bc.WARNING+'No targets given, using '+bc.ENDC+bc.OKGREEN+'default'+bc.ENDC

def validate_targets(targs, bc):
    valid_list = ['.', 'blender', 'blenderstatic', 'blenderplayer', 'webplugin',
            'blendernogame', 'blenderstaticnogame', 'release',
            'everything', 'clean', 'install-bin', 'install', 'nsis']
    oklist = []
    for t in targs:
        if t in valid_list:
            oklist.append(t)
        else:
            print '\t'+bc.WARNING+'Invalid target: '+bc.ENDC+t
    return oklist

class ourSpawn:
    def ourspawn(self, sh, escape, cmd, args, env):
        newargs = string.join(args[1:], ' ')
        cmdline = cmd + " " + newargs
        startupinfo = subprocess.STARTUPINFO()
        startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        proc = subprocess.Popen(cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, startupinfo=startupinfo, shell = False)
        data, err = proc.communicate()
        rv = proc.wait()
        if rv:
            print "====="
            print err
            print "====="
        return rv

def SetupSpawn( env ):
    buf = ourSpawn()
    buf.ourenv = env
    env['SPAWN'] = buf.ourspawn


def read_opts(cfg, args):
    localopts = Options.Options(cfg, args)
    localopts.AddOptions(
        ('VERSE_BUILD_BINARY', 'Build a release or debug binary.', 'release'),
        ('VERSE_BUILD_DIR', 'Target directory for intermediate files.', "${BF_BUILDDIR}/extern/verse"),
        ('VERSE_REGEN_PROTO', 'Whether to regenerate the protocol files', 'yes'),
        (BoolOption('WITH_BF_VERSE', 'Use VERSE if true', 'false')),
        ('BF_VERSE_INCLUDE', 'verse include dir', '/usr/include'),
        ('LCGDIR', 'location of cvs lib dir'),
        ('VERSE_BUILD_BINARY', 'Build a release or debug binary.', 'release'),
        ('VERSE_BUILD_DIR', 'Target directory for intermediate files.', "${BF_BUILDDIR}/extern/verse"),
        ('VERSE_REGEN_PROTO', 'Whether to regenerate the protocol files', 'yes'),
        ('BF_DEBUG_LIBS', 'list of libraries to build with debug symbols'),
        ('BF_PYTHON', 'base path for python', ''),
        ('BF_PYTHON_VERSION', 'Python version to use', ''),
        ('BF_PYTHON_INC', 'include path for Python headers', ''),
        ('BF_PYTHON_BINARY', 'Path to the Python interpreter', ''),
        ('BF_PYTHON_LIB', 'Python library', ''),
        ('BF_PYTHON_LIBPATH', 'Library path', ''),
        ('BF_PYTHON_LINKFLAGS', 'Python link flags', ''),
        (BoolOption('BF_NO_ELBEEM', 'Disable Fluid Sim', 'false')),
        (BoolOption('WITH_BF_YAFRAY', 'Enable Yafray', 'true')),
        (BoolOption('WITH_BF_OPENAL', 'Use OpenAL if true', '')),
        ('BF_OPENAL', 'base path for OpenAL', ''),
        ('BF_OPENAL_INC', 'include path for python headers', ''),
        ('BF_OPENAL_LIB', 'Path to OpenAL library', ''),
        ('BF_OPENAL_LIBPATH', 'Path to OpenAL library', ''),

        (BoolOption('WITH_BF_SDL', 'Use SDL if true', '')),
        ('BF_SDL', 'SDL base path', ''),
        ('BF_SDL_INC', 'SDL include path', ''),     #$(shell $(BF_SDL)/bin/sdl-config --cflags)
        ('BF_SDL_LIB', 'SDL library', ''),      #$(shell $(BF_SDL)/bin/sdl-config --libs) -lSDL_mixer
        ('BF_SDL_LIBPATH', 'SDL library path', ''),

        ('BF_PTHREADS', 'Pthreads base path', ''),
        ('BF_PTHREADS_INC', 'Pthreads include path', ''),
        ('BF_PTHREADS_LIB', 'Pthreads library', ''),
        ('BF_PTHREADS_LIBPATH', 'Pthreads library path', ''),

        (BoolOption('WITH_BF_FMOD', 'Use FMOD if true', 'false')),
        #  BF_FMOD = $(LCGDIR)/fmod

        (BoolOption('WITH_BF_OPENEXR', 'Use OPENEXR if true', 'true')),
        ('BF_OPENEXR', 'OPENEXR base path', ''),
        ('BF_OPENEXR_INC', 'OPENEXR include path', ''),
        ('BF_OPENEXR_LIB', 'OPENEXR library', ''),
        ('BF_OPENEXR_LIBPATH', 'OPENEXR library path', ''),

        (BoolOption('WITH_BF_DDS', 'Use DDS if true', 'true')),

        (BoolOption('WITH_BF_FFMPEG', 'Use FFMPEG if true', 'false')),
        ('BF_FFMPEG', 'FFMPEG base path', ''),
        ('BF_FFMPEG_LIB', 'FFMPEG library', ''),
        ('BF_FFMPEG_EXTRA', 'FFMPEG flags that must be preserved', ''),

        ('BF_FFMPEG_INC', 'FFMPEG includes', ''),
        ('BF_FFMPEG_LIBPATH', 'FFMPEG library path', ''),
        

        (BoolOption('WITH_BF_JPEG', 'Use JPEG if true', 'true')),
        ('BF_JPEG', 'JPEG base path', ''),
        ('BF_JPEG_INC', 'JPEG include path', ''),
        ('BF_JPEG_LIB', 'JPEG library', ''),
        ('BF_JPEG_LIBPATH', 'JPEG library path', ''),

        (BoolOption('WITH_BF_OPENJPEG', 'Use OPENJPEG if true', 'false')),
        ('BF_OPENJPEG', 'OPENJPEG base path', ''),
        ('BF_OPENJPEG_INC', 'OPENJPEG include path', ''),
        ('BF_OPENJPEG_LIB', 'OPENJPEG library', ''),
        ('BF_OPENJPEG_LIBPATH', 'OPENJPEG library path', ''),

        (BoolOption('WITH_BF_REDCODE', 'Use REDCODE if true', 'false')),
        ('BF_REDCODE', 'REDCODE base path', ''),
        ('BF_REDCODE_INC', 'REDCODE include path', ''),
        ('BF_REDCODE_LIB', 'REDCODE library', ''),
        ('BF_REDCODE_LIBPATH', 'REDCODE library path', ''),

        (BoolOption('WITH_BF_PNG', 'Use PNG if true', 'true')),
        ('BF_PNG', 'PNG base path', ''),
        ('BF_PNG_INC', 'PNG include path', ''),
        ('BF_PNG_LIB', 'PNG library', ''),
        ('BF_PNG_LIBPATH', 'PNG library path', ''),

        ('BF_TIFF', 'TIFF base path', ''),
        ('BF_TIFF_INC', 'TIFF include path', ''),

        (BoolOption('WITH_BF_ZLIB', 'Use ZLib if true', 'true')),
        ('BF_ZLIB', 'ZLib base path', ''),
        ('BF_ZLIB_INC', 'ZLib include path', ''),
        ('BF_ZLIB_LIB', 'ZLib library', ''),
        ('BF_ZLIB_LIBPATH', 'ZLib library path', ''),

        (BoolOption('WITH_BF_INTERNATIONAL', 'Use Gettext and Freetype if true', 'true')),

        ('BF_GETTEXT', 'gettext base path', ''),
        ('BF_GETTEXT_INC', 'gettext include path', ''),
        ('BF_GETTEXT_LIB', 'gettext library', ''),
        ('BF_GETTEXT_LIBPATH', 'gettext library path', ''),
        
        (BoolOption('WITH_BF_ICONV', 'Use iconv if true', 'true')),
        ('BF_ICONV', 'iconv base path', ''),
        ('BF_ICONV_INC', 'iconv include path', ''),
        ('BF_ICONV_LIB', 'iconv library', ''),
        ('BF_ICONV_LIBPATH', 'iconv library path', ''),
        
        (BoolOption('WITH_BF_GAMEENGINE', 'Build with gameengine' , 'true')),

        (BoolOption('WITH_BF_ODE', 'Use ODE if true', 'true')),
        ('BF_ODE', 'ODE base path', ''),
        ('BF_ODE_INC', 'ODE include path' , ''),
        ('BF_ODE_LIB', 'ODE library', ''),

        (BoolOption('WITH_BF_BULLET', 'Use Bullet if true', 'true')),
        ('BF_BULLET', 'Bullet base dir', ''),
        ('BF_BULLET_INC', 'Bullet include path', ''),
        ('BF_BULLET_LIB', 'Bullet library', ''),
        
        ('BF_SOLID', 'Solid base dir', '#/extern/solid'),
        ('BF_SOLID_INC', 'Solid include path', ''),
        ('BF_WINTAB', 'WinTab base dir', ''),
        ('BF_WINTAB_INC', 'WinTab include dir', ''),
##
##WITH_BF_NSPR = 'true'
##BF_NSPR = $(LCGDIR)/nspr
##BF_NSPR_INC = -I$(BF_NSPR)/include -I$(BF_NSPR)/include/nspr
##BF_NSPR_LIB = 
### Uncomment the following line to use Mozilla inplace of netscape
##CPPFLAGS += -DMOZ_NOT_NET
### Location of MOZILLA/Netscape header files...
##BF_MOZILLA = $(LCGDIR)/mozilla
##BF_MOZILLA_INC = -I$(BF_MOZILLA)/include/mozilla/nspr -I$(BF_MOZILLA)/include/mozilla -I$(BF_MOZILLA)/include/mozilla/xpcom -I$(BF_MOZILLA)/include/mozilla/idl
##BF_MOZILLA_LIB = 
### Will fall back to look in BF_MOZILLA_INC/nspr and BF_MOZILLA_LIB
### if this is not set.
##
### Be paranoid regarding library creation (do not update archives)
##BF_PARANOID = 'true'
##
### enable freetype2 support for text objects
        ('BF_FREETYPE', 'Freetype base path', ''),
        ('BF_FREETYPE_INC', 'Freetype include path', ''),
        ('BF_FREETYPE_LIB', 'Freetype library', ''),
        ('BF_FREETYPE_LIBPATH', 'Freetype library path', ''),

        (BoolOption('WITH_BF_OPENMP', 'Use OpenMP if true', 'false')),

        (BoolOption('WITH_BF_QUICKTIME', 'Use QuickTime if true', 'false')),
        ('BF_QUICKTIME', 'QuickTime base path', ''),
        ('BF_QUICKTIME_INC', 'QuickTime include path', ''),
        ('BF_QUICKTIME_LIB', 'QuickTime library', ''),
        ('BF_QUICKTIME_LIBPATH', 'QuickTime library path', ''),

        (BoolOption('WITH_BF_STATICOPENGL', 'Use MESA if true', 'true')),
        ('BF_OPENGL', 'OpenGL base path', ''),
        ('BF_OPENGL_INC', 'OpenGL include path', ''),
        ('BF_OPENGL_LIB', 'OpenGL libraries', ''),
        ('BF_OPENGL_LIBPATH', 'OpenGL library path', ''),
        ('BF_OPENGL_LIB_STATIC', 'OpenGL static libraries', ''),
        ('BF_OPENGL_LINKFLAGS', 'OpenGL link flags', ''),
        
        (BoolOption('WITH_BF_FTGL', 'Use FTGL if true', 'true')),
        ('BF_FTGL', 'FTGL base path', ''),
        ('BF_FTGL_INC', 'FTGL include path', ''),
        ('BF_FTGL_LIB', 'FTGL libraries', ''),

        (BoolOption('WITH_BF_GLEXT', 'Enable GL Extensions', 'true')),
        (BoolOption('WITH_BF_PLAYER', 'Build blenderplayer if true', 'false')),

        ('CFLAGS', 'C-compiler flags', ''),
        ('CCFLAGS', 'C++-compiler flags', ''),
        ('CPPFLAGS', 'Defines', ''),
        ('REL_CFLAGS', 'C release flags', ''),
        ('REL_CCFLAGS', 'C++ release flags', ''),

        ('C_WARN', 'C warning flags', ''),
        ('CC_WARN', 'C++ warning flags', ''),

        ('LLIBS', 'Platform libs', ''),
        ('PLATFORM_LINKFLAGS', 'Platform linkflags', ''),

        (BoolOption('BF_PROFILE', 'Add profiling information if true', 'false')),
        ('BF_PROFILE_FLAGS', 'Profiling flags', ''),

        (BoolOption('BF_DEBUG', 'Add debug flags if true', 'false')),
        ('BF_DEBUG_FLAGS', 'Debug flags', ''),

        (BoolOption('BF_BSC', 'Create .bsc files (msvc only)', 'true')),

        ('BF_BUILDDIR', 'Build dir', ''),
        ('BF_INSTALLDIR', 'Installation dir', ''),

        ('CC', 'C compiler to use', ''),
        ('CXX', 'C++ compiler to use', ''),

        (BoolOption('BF_BUILDINFO', 'Buildtime in splash if true', 'true')),

        (BoolOption('BF_TWEAK_MODE', 'Enable tweak mode if true', 'false')),
        (BoolOption('BF_SPLIT_SRC', 'Split src lib into several chunks if true', 'false')),
        (BoolOption('WITHOUT_BF_INSTALL', 'dont install if true', 'false')),
        (BoolOption('BF_FANCY', 'Enable fancy output if true', 'true')),
        (BoolOption('BF_QUIET', 'Enable silent output if true', 'true')),
        (BoolOption('WITH_BF_BINRELOC', 'Enable relocatable binary (linux only)', 'false')),

        ('BF_X264_CONFIG', 'configuration flags for x264', ''),
        ('BF_XVIDCORE_CONFIG', 'configuration flags for xvidcore', ''),
        
        ('BF_CONFIG', 'SCons python config file used to set default options', 'user_config.py'),

    ) # end of opts.AddOptions()

    return localopts

def NSIS_print(target, source, env):
    return "Creating NSIS installer for Blender 3D"

def NSIS_Installer(target=None, source=None, env=None):

    if env['OURPLATFORM'] != 'win32-vc' and env['OURPLATFORM'] != 'win32-mingw':
        print "NSIS installer is only available on Windows."
        Exit()
        
    start_dir = os.getcwd()
    rel_dir = start_dir + "\\release\\windows\\installer\\"
    install_base_dir = start_dir + "\\"
    
    if not os.path.exists(install_base_dir+env['BF_INSTALLDIR']+'/plugins/include'):
        os.mkdir(install_base_dir+env['BF_INSTALLDIR']+'/plugins/include')
        
    for f in glob.glob('source/blender/blenpluginapi/*.h'):
        shutil.copy(f,install_base_dir+env['BF_INSTALLDIR']+'/plugins/include')

    shutil.copy('source/blender/blenpluginapi/plugin.def',install_base_dir+env['BF_INSTALLDIR']+'/plugins/include/')
    
    os.chdir("release")
    v = open("VERSION")
    version = v.read()[:-1]	
    shortver = version.split('.')[0] + version.split('.')[1]
    v.close()

    #### change to suit install dir ####
    inst_dir = install_base_dir + env['BF_INSTALLDIR']
    
    os.chdir("windows/installer")

    ns = open("00.sconsblender.nsi","r")

    ns_cnt = str(ns.read())
    ns.close()

    # do root
    rootlist = []
    rootdir = os.listdir(inst_dir+"\\")
    for rootitem in rootdir:
        if os.path.isdir(inst_dir+"\\"+ rootitem) == 0:
            rootlist.append("File \"" + os.path.normpath(inst_dir) + "\\" + rootitem+"\"")
    rootstring = string.join(rootlist, "\n  ")
    rootstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[ROOTDIRCONTS]", rootstring)

    # do delete items
    delrootlist = []
    for rootitem in rootdir:
        if os.path.isdir(inst_dir + rootitem) == 0:
            delrootlist.append("Delete $INSTDIR\\" + rootitem)
    delrootstring = string.join(delrootlist, "\n ")
    delrootstring += "\n"
    ns_cnt = string.replace(ns_cnt, "[DELROOTDIRCONTS]", delrootstring)

    # do scripts
    scriptlist = []
    scriptpath = "%s%s" % (inst_dir, "\\.blender\\scripts")
    scriptdir = os.listdir(scriptpath)
    for scriptitem in scriptdir:
        scriptfile = "%s\\%s" % (scriptpath, scriptitem)
        if os.path.isdir(scriptfile) == 0:
            scriptfile = os.path.normpath(scriptfile)
            scriptlist.append("File \"%s\"" % scriptfile)
    scriptstring = string.join(scriptlist, "\n  ")
    scriptstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[SCRIPTCONTS]", scriptstring)

    # do scripts\bpymodules
    bpymodlist = []
    bpymodpath = "%s%s" % (inst_dir, "\\.blender\\scripts\\bpymodules")
    bpymoddir = os.listdir(bpymodpath)

    for bpymoditem in bpymoddir:
        bpymodfile = "%s\\%s" % (bpymodpath, bpymoditem)
        if os.path.isdir(bpymodfile) == 0:
            bpymodfile = os.path.normpath(bpymodfile)
            bpymodlist.append("File \"%s\"" % bpymodfile)
    bpymodstring = string.join(bpymodlist, "\n  ")
    bpymodstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[SCRIPTMODCONTS]", bpymodstring)

    # do scripts\bpymodules\colladaimex
    colladalist = []
    bpymodpath = "%s%s" % (inst_dir, "\\.blender\\scripts\\bpymodules\\ColladaImEx")
    bpymoddir = os.listdir(bpymodpath)

    for bpymoditem in bpymoddir:
        bpymodfile = "%s\\%s" % (bpymodpath, bpymoditem)
        if os.path.isdir(bpymodfile) == 0:
            bpymodfile=os.path.normpath(bpymodfile)
            colladalist.append("File \"%s\"" % bpymodfile)
    bpymodstring = string.join(colladalist, "\n  ")
    bpymodstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[SCRIPTMODCOLLADACONT]", bpymodstring)

    # do scripts\bpydata
    bpydatalist = []
    bpydatapath = "%s%s" % (inst_dir, "\\.blender\\scripts\\bpydata")
    bpydatadir = os.listdir(bpydatapath)
    for bpydataitem in bpydatadir:
        bpydatafile = "%s\\%s" % (bpydatapath, bpydataitem)
        if os.path.isdir(bpydatafile) == 0:
            bpydatalist.append("File \"%s\"" % bpydatafile)
    bpydatastring = string.join(bpydatalist, "\n  ")
    bpydatastring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[SCRIPTDATACONTS]", bpydatastring)

    # do plugins\include
    plugincludelist = []
    plugincludepath = "%s%s" % (inst_dir, "\\plugins\\include")
    plugincludedir = os.listdir(plugincludepath)
    for plugincludeitem in plugincludedir:
        plugincludefile = "%s\\%s" % (plugincludepath, plugincludeitem)
        if os.path.isdir(plugincludefile) == 0:
            if plugincludefile.find('.h') or plugincludefile.find('.DEF'):
                plugincludefile = os.path.normpath(plugincludefile)
                plugincludelist.append("File \"%s\"" % plugincludefile)
    plugincludestring = string.join(plugincludelist, "\n  ")
    plugincludestring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[PLUGINCONTS]", plugincludestring)

    # do scripts\bpydata\config
    cfglist = []
    cfgpath = "%s%s" % (inst_dir, "\\.blender\\scripts\\bpydata\\config")
    cfgdir = os.listdir(cfgpath)
    for cfgitem in cfgdir:
        cfgfile = "%s\\%s" % (cfgpath, cfgitem)
        if os.path.isdir(cfgfile) == 0:
            cfglist.append("File \"%s\"" % cfgfile)
    cfgstring = string.join(cfglist, "\n  ")
    cfgstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[SCRIPTDATACFGCONTS]", cfgstring)

    # do dotblender
    dotblendlist = []
    dotblenddir = os.listdir(inst_dir+"\\.blender")
    for dotblenditem in dotblenddir:
        if os.path.isdir(inst_dir + "\\.blender\\" + dotblenditem) == 0:
            dotblendlist.append("File \"" + os.path.normpath(inst_dir) + "\\.blender\\" +
            dotblenditem+"\"")
    dotblendstring = string.join(dotblendlist, "\n  ")
    dotblendstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[DOTBLENDERCONTS]", dotblendstring)

    # do language files
    langlist = []
    langfiles = []
    langdir = os.listdir(inst_dir + "\\.blender\\locale")
    for langitem in langdir:
        if os.path.isdir(inst_dir + "\\.blender\\locale\\" + langitem) == 1:
            langfiles.append("SetOutPath $BLENDERHOME\\.blender\\locale\\" + langitem + "\\LC_MESSAGES")
            langfiles.append("File \"" + os.path.normpath(inst_dir) + "\\.blender\\locale\\"
                    + langitem + "\\LC_MESSAGES\\blender.mo\"")
    langstring = string.join(langfiles, "\n  ")
    langstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[LANGUAGECONTS]", langstring)

    # var replacements
    ns_cnt = string.replace(ns_cnt, "DISTDIR", os.path.normpath(inst_dir+"\\"))
    ns_cnt = string.replace(ns_cnt, "SHORTVER", shortver)
    ns_cnt = string.replace(ns_cnt, "VERSION", version)
    ns_cnt = string.replace(ns_cnt, "RELDIR", os.path.normpath(rel_dir))

    tmpnsi = os.path.normpath(install_base_dir+os.sep+env['BF_BUILDDIR']+os.sep+"00.blender_tmp.nsi")
    new_nsis = open(tmpnsi, 'w')
    new_nsis.write(ns_cnt)
    new_nsis.close()

    os.chdir(start_dir)

    cmdline = "makensis " + "\""+tmpnsi+"\""

    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    proc = subprocess.Popen(cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, startupinfo=startupinfo, shell = True)
    data, err = proc.communicate()
    rv = proc.wait()

    return 0

