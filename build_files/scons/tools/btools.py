import os
import os.path
import SCons.Options

import SCons.Variables
try:
    import subprocess
except ImportError:
    pass
import string
import glob
import shutil
import sys

Variables = SCons.Variables
BoolVariable = SCons.Variables.BoolVariable

def get_version():
    import re

    fname = os.path.join(os.path.dirname(__file__), "..", "..", "..", "source", "blender", "blenkernel", "BKE_blender.h")
    ver_base = None
    ver_char = None
    ver_cycle = None

    re_ver = re.compile("^#\s*define\s+BLENDER_VERSION\s+([0-9]+)")
    re_ver_char = re.compile("^#\s*define\s+BLENDER_VERSION_CHAR\s*(\S*)") # optional arg
    re_ver_cycle = re.compile("^#\s*define\s+BLENDER_VERSION_CYCLE\s*(\S*)") # optional arg

    for l in open(fname, "r"):
        match = re_ver.match(l)
        if match:
            ver = int(match.group(1))
            ver_base = "%d.%d" % (ver / 100, ver % 100)

        match = re_ver_char.match(l)
        if match:
            ver_char = match.group(1)
            if ver_char == "BLENDER_CHAR_VERSION":
                ver_char = ""

        match = re_ver_cycle.match(l)
        if match:
            ver_cycle = match.group(1)
            if ver_cycle == "BLENDER_CYCLE_VERSION":
                ver_cycle = ""

        if (ver_base is not None) and (ver_char is not None) and (ver_cycle is not None):
            # eg '2.56a-beta'
            if ver_cycle:
                ver_display = "%s%s-%s" % (ver_base, ver_char, ver_cycle)
            else:
                ver_display = "%s%s" % (ver_base, ver_char)  # assume release

            return ver_base, ver_display, ver_cycle

    raise Exception("%s: missing version string" % fname)

def get_revision():
    build_rev = os.popen('svnversion').read()[:-1] # remove \n
    if build_rev == '' or build_rev==None: 
        build_rev = 'UNKNOWN'

    return 'r' + build_rev


# copied from: http://www.scons.org/wiki/AutoconfRecipes
def checkEndian():
    import struct
    array = struct.pack('cccc', '\x01', '\x02', '\x03', '\x04')
    i = struct.unpack('i', array)
    # Little Endian
    if i == struct.unpack('<i', array):
        return "little"
    # Big Endian
    elif i == struct.unpack('>i', array):
        return "big"
    else:
        raise Exception("cant find endian")


# This is used in creating the local config directories
VERSION, VERSION_DISPLAY, VERSION_RELEASE_CYCLE = get_version()
REVISION = get_revision()
ENDIAN = checkEndian()


def print_arguments(args, bc):
    if len(args):
        for k,v in args.iteritems():
            if type(v)==list:
                v = ' '.join(v)
            print '\t'+bc.OKBLUE+k+bc.ENDC+' = '+bc.OKGREEN + v + bc.ENDC
    else:
        print '\t'+bc.WARNING+'No  command-line arguments given'+bc.ENDC

def validate_arguments(args, bc):
    opts_list = [
            'WITH_BF_PYTHON', 'WITH_BF_PYTHON_SAFETY', 'BF_PYTHON', 'BF_PYTHON_VERSION', 'BF_PYTHON_INC', 'BF_PYTHON_BINARY', 'BF_PYTHON_LIB', 'BF_PYTHON_LIBPATH', 'WITH_BF_STATICPYTHON', 'WITH_OSX_STATICPYTHON', 'BF_PYTHON_LIB_STATIC', 'BF_PYTHON_DLL', 'BF_PYTHON_ABI_FLAGS', 
            'WITH_BF_OPENAL', 'BF_OPENAL', 'BF_OPENAL_INC', 'BF_OPENAL_LIB', 'BF_OPENAL_LIBPATH', 'WITH_BF_STATICOPENAL', 'BF_OPENAL_LIB_STATIC',
            'WITH_BF_SDL', 'BF_SDL', 'BF_SDL_INC', 'BF_SDL_LIB', 'BF_SDL_LIBPATH',
            'WITH_BF_JACK', 'BF_JACK', 'BF_JACK_INC', 'BF_JACK_LIB', 'BF_JACK_LIBPATH',
            'WITH_BF_SNDFILE', 'BF_SNDFILE', 'BF_SNDFILE_INC', 'BF_SNDFILE_LIB', 'BF_SNDFILE_LIBPATH', 'WITH_BF_STATICSNDFILE', 'BF_SNDFILE_LIB_STATIC',
            'BF_PTHREADS', 'BF_PTHREADS_INC', 'BF_PTHREADS_LIB', 'BF_PTHREADS_LIBPATH',
            'WITH_BF_OPENEXR', 'BF_OPENEXR', 'BF_OPENEXR_INC', 'BF_OPENEXR_LIB', 'BF_OPENEXR_LIBPATH', 'WITH_BF_STATICOPENEXR', 'BF_OPENEXR_LIB_STATIC',
            'WITH_BF_DDS', 'WITH_BF_CINEON', 'WITH_BF_HDR',
            'WITH_BF_FFMPEG', 'BF_FFMPEG_LIB','BF_FFMPEG_EXTRA', 'BF_FFMPEG',  'BF_FFMPEG_INC', 'BF_FFMPEG_DLL',
            'WITH_BF_STATICFFMPEG', 'BF_FFMPEG_LIB_STATIC',
            'WITH_BF_OGG', 'BF_OGG', 'BF_OGG_LIB',
            'WITH_BF_JPEG', 'BF_JPEG', 'BF_JPEG_INC', 'BF_JPEG_LIB', 'BF_JPEG_LIBPATH',
            'WITH_BF_OPENJPEG', 'BF_OPENJPEG', 'BF_OPENJPEG_INC', 'BF_OPENJPEG_LIB', 'BF_OPENJPEG_LIBPATH',
            'WITH_BF_REDCODE', 'BF_REDCODE', 'BF_REDCODE_INC', 'BF_REDCODE_LIB', 'BF_REDCODE_LIBPATH',
            'WITH_BF_PNG', 'BF_PNG', 'BF_PNG_INC', 'BF_PNG_LIB', 'BF_PNG_LIBPATH',
            'WITH_BF_TIFF', 'BF_TIFF', 'BF_TIFF_INC', 'BF_TIFF_LIB', 'BF_TIFF_LIBPATH', 'WITH_BF_STATICTIFF', 'BF_TIFF_LIB_STATIC',
            'WITH_BF_ZLIB', 'BF_ZLIB', 'BF_ZLIB_INC', 'BF_ZLIB_LIB', 'BF_ZLIB_LIBPATH', 'WITH_BF_STATICZLIB', 'BF_ZLIB_LIB_STATIC',
            'WITH_BF_INTERNATIONAL',
            'BF_GETTEXT', 'BF_GETTEXT_INC', 'BF_GETTEXT_LIB', 'WITH_BF_GETTEXT_STATIC', 'BF_GETTEXT_LIB_STATIC', 'BF_GETTEXT_LIBPATH',
            'WITH_BF_ICONV', 'BF_ICONV', 'BF_ICONV_INC', 'BF_ICONV_LIB', 'BF_ICONV_LIBPATH',
            'WITH_BF_GAMEENGINE',
            'WITH_BF_BULLET', 'BF_BULLET', 'BF_BULLET_INC', 'BF_BULLET_LIB',
            'WITH_BF_ELTOPO',
            'BF_WINTAB', 'BF_WINTAB_INC',
            'BF_FREETYPE', 'BF_FREETYPE_INC', 'BF_FREETYPE_LIB', 'BF_FREETYPE_LIBPATH', 'BF_FREETYPE_LIB_STATIC', 'WITH_BF_FREETYPE_STATIC',
            'WITH_BF_QUICKTIME', 'BF_QUICKTIME', 'BF_QUICKTIME_INC', 'BF_QUICKTIME_LIB', 'BF_QUICKTIME_LIBPATH',
            'WITH_BF_FFTW3', 'BF_FFTW3', 'BF_FFTW3_INC', 'BF_FFTW3_LIB', 'BF_FFTW3_LIBPATH', 'WITH_BF_STATICFFTW3', 'BF_FFTW3_LIB_STATIC',
            'WITH_BF_STATICOPENGL', 'BF_OPENGL', 'BF_OPENGL_INC', 'BF_OPENGL_LIB', 'BF_OPENGL_LIBPATH', 'BF_OPENGL_LIB_STATIC',
            'WITH_BF_COLLADA', 'BF_COLLADA', 'BF_COLLADA_INC', 'BF_COLLADA_LIB', 'BF_OPENCOLLADA', 'BF_OPENCOLLADA_INC', 'BF_OPENCOLLADA_LIB', 'BF_OPENCOLLADA_LIBPATH', 'BF_PCRE', 'BF_PCRE_LIB', 'BF_PCRE_LIBPATH', 'BF_EXPAT', 'BF_EXPAT_LIB', 'BF_EXPAT_LIBPATH',
            'WITH_BF_PLAYER',
            'WITH_BF_NOBLENDER',
            'WITH_BF_BINRELOC',
            'WITH_BF_LZO', 'WITH_BF_LZMA',
            'LCGDIR',
            'BF_CXX', 'WITH_BF_STATICCXX', 'BF_CXX_LIB_STATIC',
            'BF_TWEAK_MODE', 'BF_SPLIT_SRC',
            'WITHOUT_BF_INSTALL',
            'WITHOUT_BF_PYTHON_INSTALL',
            'WITHOUT_BF_OVERWRITE_INSTALL',
            'WITH_BF_OPENMP', 'BF_OPENMP', 'BF_OPENMP_LIBPATH',
            'WITH_GHOST_COCOA',
            'WITH_GHOST_SDL',
            'BF_GHOST_DEBUG',
            'USE_QTKIT',
            'BF_FANCY', 'BF_QUIET', 'BF_LINE_OVERWRITE',
            'BF_X264_CONFIG',
            'BF_XVIDCORE_CONFIG',
            'WITH_BF_DOCS',
            'BF_NUMJOBS',
            'BF_MSVS',
            'BF_VERSION',
            'WITH_BF_RAYOPTIMIZATION',
            'BF_RAYOPTIMIZATION_SSE_FLAGS',
            'WITH_BF_FLUID',
            'WITH_BF_DECIMATE',
            'WITH_BF_BOOLEAN',
            'WITH_BF_REMESH',
            'WITH_BF_OCEANSIM',
            'WITH_BF_CXX_GUARDEDALLOC',
            'WITH_BF_JEMALLOC', 'WITH_BF_STATICJEMALLOC', 'BF_JEMALLOC', 'BF_JEMALLOC_INC', 'BF_JEMALLOC_LIBPATH', 'BF_JEMALLOC_LIB', 'BF_JEMALLOC_LIB_STATIC',
            'BUILDBOT_BRANCH',
            'WITH_BF_3DMOUSE', 'WITH_BF_STATIC3DMOUSE', 'BF_3DMOUSE', 'BF_3DMOUSE_INC', 'BF_3DMOUSE_LIB', 'BF_3DMOUSE_LIBPATH', 'BF_3DMOUSE_LIB_STATIC',
            'WITH_BF_CYCLES', 'WITH_BF_CYCLES_CUDA_BINARIES' 'BF_CYCLES_CUDA_NVCC', 'BF_CYCLES_CUDA_NVCC', 'WITH_BF_CYCLES_CUDA_THREADED_COMPILE',
            'WITH_BF_OIIO', 'WITH_BF_STATICOIIO', 'BF_OIIO', 'BF_OIIO_INC', 'BF_OIIO_LIB', 'BF_OIIO_LIB_STATIC', 'BF_OIIO_LIBPATH',
            'WITH_BF_BOOST', 'WITH_BF_STATICBOOST', 'BF_BOOST', 'BF_BOOST_INC', 'BF_BOOST_LIB', 'BF_BOOST_LIB_STATIC', 'BF_BOOST_LIBPATH',
            'WITH_BF_LIBMV'
            ]
    
    # Have options here that scons expects to be lists
    opts_list_split = [
            'BF_PYTHON_LINKFLAGS',
            'BF_OPENGL_LINKFLAGS',
            'CFLAGS', 'CCFLAGS', 'CXXFLAGS', 'CPPFLAGS',
            'REL_CFLAGS', 'REL_CCFLAGS', 'REL_CXXFLAGS',
            'BGE_CXXFLAGS',
            'BF_PROFILE_CFLAGS', 'BF_PROFILE_CCFLAGS', 'BF_PROFILE_CXXFLAGS', 'BF_PROFILE_LINKFLAGS',
            'BF_DEBUG_CFLAGS', 'BF_DEBUG_CCFLAGS', 'BF_DEBUG_CXXFLAGS',
            'C_WARN', 'CC_WARN', 'CXX_WARN',
            'LLIBS', 'PLATFORM_LINKFLAGS','MACOSX_ARCHITECTURE', 'MACOSX_SDK_CHECK', 'XCODE_CUR_VER',
    ]
    
    
    arg_list = ['BF_DEBUG', 'BF_QUIET', 'BF_CROSS', 'BF_UPDATE',
            'BF_INSTALLDIR', 'BF_TOOLSET', 'BF_BINNAME',
            'BF_BUILDDIR', 'BF_FANCY', 'BF_QUICK', 'BF_PROFILE', 'BF_LINE_OVERWRITE',
            'BF_BSC', 'BF_CONFIG',
            'BF_PRIORITYLIST', 'BF_BUILDINFO','CC', 'CXX', 'BF_QUICKDEBUG',
            'BF_LISTDEBUG', 'LCGDIR', 'BF_X264_CONFIG', 'BF_XVIDCORE_CONFIG',
            'BF_UNIT_TEST', 'BF_BITNESS']

    okdict = {}

    for k,v in args.iteritems():
        if (k in opts_list) or (k in arg_list):
            okdict[k] = v
        elif k in opts_list_split:
            okdict[k] = v.split() # "" have already been stripped
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
            'blendernogame', 'blenderstaticnogame', 'blenderlite', 'release',
            'everything', 'clean', 'install-bin', 'install', 'nsis','buildslave']
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


def read_opts(env, cfg, args):
    localopts = Variables.Variables(cfg, args)
    localopts.AddVariables(
        ('LCGDIR', 'location of cvs lib dir'),
        ('LIBDIR', 'root dir of libs'),
        (BoolVariable('WITH_BF_PYTHON', 'Compile with python', True)),
        (BoolVariable('WITH_BF_PYTHON_SAFETY', 'Internal API error checking to track invalid data to prevent crash on access (at the expense of some effeciency)', False)),
        ('BF_PYTHON', 'Base path for python', ''),
        ('BF_PYTHON_VERSION', 'Python version to use', ''),
        ('BF_PYTHON_INC', 'Include path for Python headers', ''),
        ('BF_PYTHON_BINARY', 'Path to the Python interpreter', ''),
        ('BF_PYTHON_LIB', 'Python library', ''),
        ('BF_PYTHON_DLL', 'Python dll - used on Windows only', ''),
        ('BF_PYTHON_LIB_STATIC', 'Python static libraries', ''),
        ('BF_PYTHON_LIBPATH', 'Library path', ''),
        ('BF_PYTHON_LINKFLAGS', 'Python link flags', ''),
        (BoolVariable('WITH_BF_STATICPYTHON', 'Staticly link to python', False)),
        (BoolVariable('WITH_OSX_STATICPYTHON', 'Staticly link to python', True)),
        ('BF_PYTHON_ABI_FLAGS', 'Python ABI flags (suffix in library version: m, mu, etc)', ''),

        (BoolVariable('WITH_BF_FLUID', 'Build with Fluid simulation (Elbeem)', True)),
        (BoolVariable('WITH_BF_DECIMATE', 'Build with decimate modifier', True)),
        (BoolVariable('WITH_BF_BOOLEAN', 'Build with boolean modifier', True)),
        (BoolVariable('WITH_BF_REMESH', 'Build with remesh modifier', True)),
        (BoolVariable('WITH_BF_OCEANSIM', 'Build with ocean simulation', False)),
        ('BF_PROFILE_FLAGS', 'Profiling compiler flags', ''),
        (BoolVariable('WITH_BF_OPENAL', 'Use OpenAL if true', False)),
        ('BF_OPENAL', 'Base path for OpenAL', ''),
        ('BF_OPENAL_INC', 'Include path for python headers', ''),
        ('BF_OPENAL_LIB', 'Path to OpenAL library', ''),
        ('BF_OPENAL_LIB_STATIC', 'Path to OpenAL static library', ''),
        ('BF_OPENAL_LIBPATH', 'Path to OpenAL library', ''),
        (BoolVariable('WITH_BF_STATICOPENAL', 'Staticly link to openal', False)),

        (BoolVariable('WITH_BF_SDL', 'Use SDL if true', False)),
        ('BF_SDL', 'SDL base path', ''),
        ('BF_SDL_INC', 'SDL include path', ''),
        ('BF_SDL_LIB', 'SDL library', ''),
        ('BF_SDL_LIBPATH', 'SDL library path', ''),

        (BoolVariable('WITH_BF_JACK', 'Enable jack support if true', True)),
        ('BF_JACK', 'jack base path', ''),
        ('BF_JACK_INC', 'jack include path', ''),
        ('BF_JACK_LIB', 'jack library', ''),
        ('BF_JACK_LIBPATH', 'jack library path', ''),

        (BoolVariable('WITH_BF_SNDFILE', 'Enable sndfile support if true', True)),
        ('BF_SNDFILE', 'sndfile base path', ''),
        ('BF_SNDFILE_INC', 'sndfile include path', ''),
        ('BF_SNDFILE_LIB', 'sndfile library', ''),
        ('BF_SNDFILE_LIB_STATIC', 'Path to sndfile static library', ''),
        ('BF_SNDFILE_LIBPATH', 'sndfile library path', ''),
        (BoolVariable('WITH_BF_STATICSNDFILE', 'Staticly link to sndfile', False)),

        ('BF_PTHREADS', 'Pthreads base path', ''),
        ('BF_PTHREADS_INC', 'Pthreads include path', ''),
        ('BF_PTHREADS_LIB', 'Pthreads library', ''),
        ('BF_PTHREADS_LIBPATH', 'Pthreads library path', ''),

        (BoolVariable('WITH_BF_OPENEXR', 'Use OPENEXR if true', True)),
        (BoolVariable('WITH_BF_STATICOPENEXR', 'Staticly link to OpenEXR', False)),
        ('BF_OPENEXR', 'OPENEXR base path', ''),
        ('BF_OPENEXR_INC', 'OPENEXR include path', ''),
        ('BF_OPENEXR_LIB', 'OPENEXR library', ''),
        ('BF_OPENEXR_LIBPATH', 'OPENEXR library path', ''),
        ('BF_OPENEXR_LIB_STATIC', 'OPENEXR static library', ''),

        (BoolVariable('WITH_BF_DDS', 'Support DDS image format if true', True)),

        (BoolVariable('WITH_BF_CINEON', 'Support CINEON and DPX image formats if true', True)),

        (BoolVariable('WITH_BF_HDR', 'Support HDR image formats if true', True)),

        (BoolVariable('WITH_BF_FFMPEG', 'Use FFMPEG if true', False)),
        ('BF_FFMPEG', 'FFMPEG base path', ''),
        ('BF_FFMPEG_LIB', 'FFMPEG library', ''),
        ('BF_FFMPEG_DLL', 'FFMPEG dll libraries to be installed', ''),
        ('BF_FFMPEG_EXTRA', 'FFMPEG flags that must be preserved', ''),

        ('BF_FFMPEG_INC', 'FFMPEG includes', ''),
        ('BF_FFMPEG_LIBPATH', 'FFMPEG library path', ''),
        (BoolVariable('WITH_BF_STATICFFMPEG', 'Use static FFMPEG if true', False)),
        ('BF_FFMPEG_LIB_STATIC', 'Static FFMPEG libraries', ''),
        
        (BoolVariable('WITH_BF_OGG', 'Link OGG, THEORA, VORBIS with FFMPEG if true',
                    False)),
        ('BF_OGG', 'OGG base path', ''),
        ('BF_OGG_LIB', 'OGG library', ''),

        (BoolVariable('WITH_BF_JPEG', 'Use JPEG if true', True)),
        ('BF_JPEG', 'JPEG base path', ''),
        ('BF_JPEG_INC', 'JPEG include path', ''),
        ('BF_JPEG_LIB', 'JPEG library', ''),
        ('BF_JPEG_LIBPATH', 'JPEG library path', ''),

        (BoolVariable('WITH_BF_OPENJPEG', 'Use OPENJPEG if true', False)),
        ('BF_OPENJPEG', 'OPENJPEG base path', ''),
        ('BF_OPENJPEG_INC', 'OPENJPEG include path', ''),
        ('BF_OPENJPEG_LIB', 'OPENJPEG library', ''),
        ('BF_OPENJPEG_LIBPATH', 'OPENJPEG library path', ''),

        (BoolVariable('WITH_BF_REDCODE', 'Use REDCODE if true', False)),
        ('BF_REDCODE', 'REDCODE base path', ''),
        ('BF_REDCODE_INC', 'REDCODE include path', ''),
        ('BF_REDCODE_LIB', 'REDCODE library', ''),
        ('BF_REDCODE_LIBPATH', 'REDCODE library path', ''),

        (BoolVariable('WITH_BF_PNG', 'Use PNG if true', True)),
        ('BF_PNG', 'PNG base path', ''),
        ('BF_PNG_INC', 'PNG include path', ''),
        ('BF_PNG_LIB', 'PNG library', ''),
        ('BF_PNG_LIBPATH', 'PNG library path', ''),

        (BoolVariable('WITH_BF_TIFF', 'Use TIFF if true', True)),
        (BoolVariable('WITH_BF_STATICTIFF', 'Staticly link to TIFF', False)),
        ('BF_TIFF', 'TIFF base path', ''),
        ('BF_TIFF_INC', 'TIFF include path', ''),
        ('BF_TIFF_LIB', 'TIFF library', ''),
        ('BF_TIFF_LIBPATH', 'TIFF library path', ''),
        ('BF_TIFF_LIB_STATIC', 'TIFF static library', ''),

        (BoolVariable('WITH_BF_ZLIB', 'Use ZLib if true', True)),
        (BoolVariable('WITH_BF_STATICZLIB', 'Staticly link to ZLib', False)),
        ('BF_ZLIB', 'ZLib base path', ''),
        ('BF_ZLIB_INC', 'ZLib include path', ''),
        ('BF_ZLIB_LIB', 'ZLib library', ''),
        ('BF_ZLIB_LIBPATH', 'ZLib library path', ''),
        ('BF_ZLIB_LIB_STATIC', 'ZLib static library', ''),

        (BoolVariable('WITH_BF_INTERNATIONAL', 'Use Gettext if true', True)),

        ('BF_GETTEXT', 'gettext base path', ''),
        ('BF_GETTEXT_INC', 'gettext include path', ''),
        ('BF_GETTEXT_LIB', 'gettext library', ''),
        (BoolVariable('WITH_BF_GETTEXT_STATIC', 'Use static gettext library if true', False)),
        ('BF_GETTEXT_LIB_STATIC', 'static gettext library', ''),
        ('BF_GETTEXT_LIBPATH', 'gettext library path', ''),
        
        (BoolVariable('WITH_BF_ICONV', 'Use iconv if true', True)),
        ('BF_ICONV', 'iconv base path', ''),
        ('BF_ICONV_INC', 'iconv include path', ''),
        ('BF_ICONV_LIB', 'iconv library', ''),
        ('BF_ICONV_LIBPATH', 'iconv library path', ''),
        
        (BoolVariable('WITH_BF_GAMEENGINE', 'Build with gameengine' , False)),

        (BoolVariable('WITH_BF_BULLET', 'Use Bullet if true', True)),
        (BoolVariable('WITH_BF_ELTOPO', 'Use Eltopo collision library if true', False)),
        
        ('BF_BULLET', 'Bullet base dir', ''),
        ('BF_BULLET_INC', 'Bullet include path', ''),
        ('BF_BULLET_LIB', 'Bullet library', ''),
        
        ('BF_WINTAB', 'WinTab base dir', ''),
        ('BF_WINTAB_INC', 'WinTab include dir', ''),
        ('BF_CXX', 'c++ base path for libstdc++, only used when static linking', ''),
        (BoolVariable('WITH_BF_STATICCXX', 'static link to stdc++', False)),
        ('BF_CXX_LIB_STATIC', 'static library path for stdc++', ''),

        ('BF_FREETYPE', 'Freetype base path', ''),
        ('BF_FREETYPE_INC', 'Freetype include path', ''),
        ('BF_FREETYPE_LIB', 'Freetype library', ''),
        ('BF_FREETYPE_LIBPATH', 'Freetype library path', ''),
        (BoolVariable('WITH_BF_FREETYPE_STATIC', 'Use Static Freetype if true', False)),
        ('BF_FREETYPE_LIB_STATIC', 'Static Freetype library', ''),

        (BoolVariable('WITH_BF_OPENMP', 'Use OpenMP if true', False)),
        ('BF_OPENMP', 'Base path to OpenMP (used when cross-compiling with older versions of WinGW)', ''),
        ('BF_OPENMP_INC', 'Path to OpenMP includes (used when cross-compiling with older versions of WinGW)', ''),
        ('BF_OPENMP_LIBPATH', 'Path to OpenMP libraries (used when cross-compiling with older versions of WinGW)', ''),
        (BoolVariable('WITH_GHOST_COCOA', 'Use Cocoa-framework if true', False)),
        (BoolVariable('WITH_GHOST_SDL', 'Enable building blender against SDL for windowing rather then the native APIs', False)),
        (BoolVariable('USE_QTKIT', 'Use QTKIT if true', False)),

        (BoolVariable('WITH_BF_QUICKTIME', 'Use QuickTime if true', False)),
        ('BF_QUICKTIME', 'QuickTime base path', ''),
        ('BF_QUICKTIME_INC', 'QuickTime include path', ''),
        ('BF_QUICKTIME_LIB', 'QuickTime library', ''),
        ('BF_QUICKTIME_LIBPATH', 'QuickTime library path', ''),
        
        (BoolVariable('WITH_BF_FFTW3', 'Use FFTW3 if true', False)),
        ('BF_FFTW3', 'FFTW3 base path', ''),
        ('BF_FFTW3_INC', 'FFTW3 include path', ''),
        ('BF_FFTW3_LIB', 'FFTW3 library', ''),
        ('BF_FFTW3_LIB_STATIC', 'FFTW3 static libraries', ''),
        ('BF_FFTW3_LIBPATH', 'FFTW3 library path', ''),
        (BoolVariable('WITH_BF_STATICFFTW3', 'Staticly link to FFTW3', False)),

        (BoolVariable('WITH_BF_STATICOPENGL', 'Use MESA if true', True)),
        ('BF_OPENGL', 'OpenGL base path', ''),
        ('BF_OPENGL_INC', 'OpenGL include path', ''),
        ('BF_OPENGL_LIB', 'OpenGL libraries', ''),
        ('BF_OPENGL_LIBPATH', 'OpenGL library path', ''),
        ('BF_OPENGL_LIB_STATIC', 'OpenGL static libraries', ''),
        ('BF_OPENGL_LINKFLAGS', 'OpenGL link flags', ''),

        (BoolVariable('WITH_BF_COLLADA', 'Build COLLADA import/export module if true', False)),
        ('BF_COLLADA', 'COLLADA base path', ''),
        ('BF_COLLADA_INC', 'COLLADA include path', ''),
        ('BF_COLLADA_LIB', 'COLLADA library', ''),
        ('BF_OPENCOLLADA', 'OpenCollada base path', ''),
        ('BF_OPENCOLLADA_INC', 'OpenCollada base include path', ''),
        ('BF_OPENCOLLADA_LIB', 'OpenCollada library', ''),
        ('BF_OPENCOLLADA_LIBPATH', 'OpenCollada library path', ''),
        ('BF_PCRE', 'PCRE base path', ''),
        ('BF_PCRE_LIB', 'PCRE library', ''),
        ('BF_PCRE_LIBPATH', 'PCRE library path', ''),
        ('BF_EXPAT', 'Expat base path', ''),
        ('BF_EXPAT_LIB', 'Expat library', ''),
        ('BF_EXPAT_LIBPATH', 'Expat library path', ''),
        
        (BoolVariable('WITH_BF_JEMALLOC', 'Use jemalloc if true', False)),
        (BoolVariable('WITH_BF_STATICJEMALLOC', 'Staticly link to jemalloc', False)),
        ('BF_JEMALLOC', 'jemalloc base path', ''),
        ('BF_JEMALLOC_INC', 'jemalloc include path', ''),
        ('BF_JEMALLOC_LIB', 'jemalloc library', ''),
        ('BF_JEMALLOC_LIBPATH', 'jemalloc library path', ''),
        ('BF_JEMALLOC_LIB_STATIC', 'jemalloc static library', ''),

        (BoolVariable('WITH_BF_PLAYER', 'Build blenderplayer if true', False)),
        (BoolVariable('WITH_BF_NOBLENDER', 'Do not build blender if true', False)),

        (BoolVariable('WITH_BF_3DMOUSE', 'Build blender with support of 3D mouses', False)),
        (BoolVariable('WITH_BF_STATIC3DMOUSE', 'Staticly link to 3d mouse library', False)),
        ('BF_3DMOUSE', '3d mouse library base path', ''),
        ('BF_3DMOUSE_INC', '3d mouse library include path', ''),
        ('BF_3DMOUSE_LIB', '3d mouse library', ''),
        ('BF_3DMOUSE_LIBPATH', '3d mouse library path', ''),
        ('BF_3DMOUSE_LIB_STATIC', '3d mouse static library', ''),

        ('CFLAGS', 'C only flags', []),
        ('CCFLAGS', 'Generic C and C++ flags', []),
        ('CXXFLAGS', 'C++ only flags', []),
        ('BGE_CXXFLAGS', 'C++ only flags for BGE', []),
        ('CPPFLAGS', 'Defines', []),
        ('REL_CFLAGS', 'C only release flags', []),
        ('REL_CCFLAGS', 'Generic C and C++ release flags', []),
        ('REL_CXXFLAGS', 'C++ only release flags', []),

        ('C_WARN', 'C warning flags', []),
        ('CC_WARN', 'Generic C and C++ warning flags', []),
        ('CXX_WARN', 'C++ only warning flags', []),

        ('LLIBS', 'Platform libs', []),
        ('PLATFORM_LINKFLAGS', 'Platform linkflags', []),
        ('MACOSX_ARCHITECTURE', 'python_arch.zip select', ''),
        ('MACOSX_SDK_CHECK', 'detect available OSX sdk`s', ''),
        ('XCODE_CUR_VER', 'detect XCode version', ''),

        (BoolVariable('BF_PROFILE', 'Add profiling information if true', False)),
        ('BF_PROFILE_CFLAGS', 'C only profiling flags', []),
        ('BF_PROFILE_CCFLAGS', 'C and C++ profiling flags', []),
        ('BF_PROFILE_CXXFLAGS', 'C++ only profiling flags', []),
        ('BF_PROFILE_LINKFLAGS', 'Profile linkflags', []),

        (BoolVariable('BF_DEBUG', 'Add debug flags if true', False)),
        ('BF_DEBUG_CFLAGS', 'C only debug flags', []),
        ('BF_DEBUG_CCFLAGS', 'C and C++ debug flags', []),
        ('BF_DEBUG_CXXFLAGS', 'C++ only debug flags', []),

        (BoolVariable('BF_BSC', 'Create .bsc files (msvc only)', False)),

        ('BF_BUILDDIR', 'Build dir', ''),
        ('BF_INSTALLDIR', 'Installation dir', ''),

        ('CC', 'C compiler to use', env['CC']),
        ('CXX', 'C++ compiler to use', env['CXX']),

        (BoolVariable('BF_BUILDINFO', 'Buildtime in splash if true', True)),

        (BoolVariable('BF_TWEAK_MODE', 'Enable tweak mode if true', False)),
        (BoolVariable('BF_SPLIT_SRC', 'Split src lib into several chunks if true', False)),
        (BoolVariable('WITHOUT_BF_INSTALL', 'dont install if true', False)),
        (BoolVariable('WITHOUT_BF_PYTHON_INSTALL', 'dont install Python modules if true', False)),
        (BoolVariable('WITHOUT_BF_OVERWRITE_INSTALL', 'dont remove existing files before breating the new install directory (set to False when making packages for others)', False)),
        (BoolVariable('BF_FANCY', 'Enable fancy output if true', True)),
        (BoolVariable('BF_QUIET', 'Enable silent output if true', True)),
        (BoolVariable('BF_LINE_OVERWRITE', 'Enable overwriting of compile line in BF_QUIET mode if true', False)),
        (BoolVariable('WITH_BF_BINRELOC', 'Enable relocatable binary (linux only)', False)),
        
        (BoolVariable('WITH_BF_LZO', 'Enable fast LZO pointcache compression', True)),
        (BoolVariable('WITH_BF_LZMA', 'Enable best LZMA pointcache compression', True)),
        
        (BoolVariable('WITH_BF_LIBMV', 'Enable libmv structure from motion library', True)),

        ('BF_X264_CONFIG', 'configuration flags for x264', ''),
        ('BF_XVIDCORE_CONFIG', 'configuration flags for xvidcore', ''),
#        (BoolVariable('WITH_BF_DOCS', 'Generate API documentation', False)),
        
        ('BF_CONFIG', 'SCons python config file used to set default options', 'user_config.py'),
        ('BF_NUMJOBS', 'Number of build processes to spawn', '1'),
        ('BF_MSVS', 'Generate MSVS project files and solution', False),

        ('BF_VERSION', 'The root path for Unix (non-apple)', '2.5'),

        (BoolVariable('BF_UNIT_TEST', 'Build with unit test support.', False)),
        
        (BoolVariable('BF_GHOST_DEBUG', 'Make GHOST print events and info to stdout. (very verbose)', False)),
        
        (BoolVariable('WITH_BF_RAYOPTIMIZATION', 'Enable raytracer SSE/SIMD optimization.', False)),
        ('BF_RAYOPTIMIZATION_SSE_FLAGS', 'SSE flags', ''),
        (BoolVariable('WITH_BF_CXX_GUARDEDALLOC', 'Enable GuardedAlloc for C++ memory allocation tracking.', False)),

        ('BUILDBOT_BRANCH', 'Buildbot branch name', ''),
    ) # end of opts.AddOptions()

    localopts.AddVariables(
        (BoolVariable('WITH_BF_CYCLES', 'Build with the Cycles engine', True)),
        (BoolVariable('WITH_BF_CYCLES_CUDA_BINARIES', 'Build with precompiled CUDA binaries', False)),
        (BoolVariable('WITH_BF_CYCLES_CUDA_THREADED_COMPILE', 'Build several render kernels at once (using BF_NUMJOBS)', False)),
        ('BF_CYCLES_CUDA_NVCC', 'CUDA nvcc compiler path', ''),
        ('BF_CYCLES_CUDA_BINARIES_ARCH', 'CUDA architectures to compile binaries for', []),

        (BoolVariable('WITH_BF_OIIO', 'Build with OpenImageIO', False)),
        (BoolVariable('WITH_BF_STATICOIIO', 'Staticly link to OpenImageIO', False)),
        ('BF_OIIO', 'OIIO root path', ''),
        ('BF_OIIO_INC', 'OIIO include path', ''),
        ('BF_OIIO_LIB', 'OIIO library', ''),
        ('BF_OIIO_LIBPATH', 'OIIO library path', ''),
        ('BF_OIIO_LIB_STATIC', 'OIIO static library', ''),

        (BoolVariable('WITH_BF_BOOST', 'Build with Boost', False)),
        (BoolVariable('WITH_BF_STATICBOOST', 'Staticly link to boost', False)),
        ('BF_BOOST', 'Boost root path', ''),
        ('BF_BOOST_INC', 'Boost include path', ''),
        ('BF_BOOST_LIB', 'Boost library', ''),
        ('BF_BOOST_LIBPATH', 'Boost library path', ''),
        ('BF_BOOST_LIB_STATIC', 'Boost static library', '')
    ) # end of opts.AddOptions()

    return localopts

def buildbot_zip(src, dest, package_name, extension):
    import zipfile
    ln = len(src)+1 # one extra to remove leading os.sep when cleaning root for package_root
    flist = list()

    # create list of tuples containing file and archive name
    for root, dirs, files in os.walk(src):
        package_root = os.path.join(package_name, root[ln:])
        flist.extend([(os.path.join(root, file), os.path.join(package_root, file)) for file in files])

    if extension == '.zip':
        package = zipfile.ZipFile(dest, 'w', zipfile.ZIP_DEFLATED)
        package.comment = package_name + ' is a zip-file containing the Blender software. Visit http://www.blender.org for more information.'
        for entry in flist:
            package.write(entry[0], entry[1])
        package.close()
    else:
        import tarfile
        package = tarfile.open(dest, 'w:bz2')
        for entry in flist:
            package.add(entry[0], entry[1], recursive=False)
        package.close()
    bb_zip_name = os.path.normpath(src + os.sep + '..' + os.sep + 'buildbot_upload.zip')
    print("creating %s" % (bb_zip_name))
    bb_zip = zipfile.ZipFile(bb_zip_name, 'w', zipfile.ZIP_DEFLATED)
    print("writing %s to %s" % (dest, bb_zip_name))
    bb_zip.write(dest, os.path.split(dest)[1])
    bb_zip.close()
    print("removing unneeded packed file %s (to keep install directory clean)" % (dest))
    os.remove(dest)
    print("done.")

def buildslave_print(target, source, env):
    return "Running buildslave target"

def buildslave(target=None, source=None, env=None):
    """
    Builder for buildbot integration. Used by buildslaves of http://builder.blender.org only.
    """

    if env['OURPLATFORM'] in ('win32-vc', 'win64-vc', 'win32-mingw', 'darwin'):
        extension = '.zip'
    else:
        extension = '.tar.bz2'

    platform = env['OURPLATFORM'].split('-')[0]
    if platform == 'linux':
        import platform

        bitness = platform.architecture()[0]
        if bitness == '64bit':
            platform = 'linux-glibc27-x86_64'
        elif bitness == '32bit':
            platform = 'linux-glibc27-i686'
    if platform == 'darwin':
        platform = 'OSX-' + env['MACOSX_ARCHITECTURE']

    branch = env['BUILDBOT_BRANCH']

    outdir = os.path.abspath(env['BF_INSTALLDIR'])
    package_name = 'blender-' + VERSION+'-'+REVISION + '-' + platform
    if branch != '':
        package_name = branch + '-' + package_name
    package_dir = os.path.normpath(outdir + os.sep + '..' + os.sep + package_name)
    package_archive = os.path.normpath(outdir + os.sep + '..' + os.sep + package_name + extension)

    try:
        if os.path.exists(package_archive):
            os.remove(package_archive)
        if os.path.exists(package_dir):
            shutil.rmtree(package_dir)
    except Exception, ex:
        sys.stderr.write('Failed to clean up old package files: ' + str(ex) + '\n')
        return 1

    buildbot_zip(outdir, package_archive, package_name, extension)

    return 0

def NSIS_print(target, source, env):
    return "Creating NSIS installer for Blender"

def NSIS_Installer(target=None, source=None, env=None):
    print "="*35

    if env['OURPLATFORM'] not in ('win32-vc', 'win32-mingw', 'win64-vc'):
        print "NSIS installer is only available on Windows."
        Exit()
    if env['OURPLATFORM'] == 'win32-vc':
        bitness = '32'
    elif env['OURPLATFORM'] == 'win64-vc':
        bitness = '64'
    else:
        bitness = '-mingw'

    start_dir = os.getcwd()
    rel_dir = os.path.join(start_dir,'release','windows','installer')
    install_base_dir = start_dir + os.sep

    bf_installdir = os.path.join(os.getcwd(),env['BF_INSTALLDIR'])
    bf_installdir = os.path.normpath(bf_installdir)

    doneroot = False
    rootdirconts = []
    datafiles = ''
    deldatafiles = ''
    deldatadirs = ''
    l = len(bf_installdir)
    
    for dp,dn,df in os.walk(bf_installdir):
        # install
        if not doneroot:
            for f in df:
                rootdirconts.append(os.path.join(dp,f))
            doneroot = True
        else:
            if len(df)>0:
                dp_tmp = dp[l:]
                datafiles += "\n" +r'SetOutPath $INSTDIR'+dp[l:]+"\n\n"

                for f in df:
                    outfile = os.path.join(dp,f)
                    datafiles += '  File '+outfile + "\n"

        # uninstall
        deldir = dp[l+1:]

        if len(deldir)>0:
            deldatadirs = "RMDir $INSTDIR\\" + deldir + "\n" + deldatadirs
            deldatadirs = "RMDir /r $INSTDIR\\" + deldir + "\\__pycache__\n" + deldatadirs

            for f in df:
                deldatafiles += 'Delete \"$INSTDIR\\' + os.path.join(deldir, f) + "\"\n"

    #### change to suit install dir ####
    inst_dir = install_base_dir + env['BF_INSTALLDIR']
    
    os.chdir(rel_dir)

    ns = open("00.sconsblender.nsi","r")

    ns_cnt = str(ns.read())
    ns.close()

    # var replacements
    ns_cnt = string.replace(ns_cnt, "[DISTDIR]", os.path.normpath(inst_dir+os.sep))
    ns_cnt = string.replace(ns_cnt, "[VERSION]", VERSION_DISPLAY)
    ns_cnt = string.replace(ns_cnt, "[SHORTVERSION]", VERSION)
    ns_cnt = string.replace(ns_cnt, "[RELDIR]", os.path.normpath(rel_dir))
    ns_cnt = string.replace(ns_cnt, "[BITNESS]", bitness)

    # do root
    rootlist = []
    for rootitem in rootdirconts:
        rootlist.append("File \"" + rootitem + "\"")
    rootstring = string.join(rootlist, "\n  ")
    rootstring = rootstring
    rootstring += "\n\n"
    ns_cnt = string.replace(ns_cnt, "[ROOTDIRCONTS]", rootstring)


    # do delete items
    delrootlist = []
    for rootitem in rootdirconts:
        delrootlist.append("Delete $INSTDIR\\" + rootitem[l+1:])
    delrootstring = string.join(delrootlist, "\n ")
    delrootstring += "\n"
    ns_cnt = string.replace(ns_cnt, "[DELROOTDIRCONTS]", delrootstring)

    ns_cnt = string.replace(ns_cnt, "[DODATAFILES]", datafiles)
    ns_cnt = string.replace(ns_cnt, "[DELDATAFILES]", deldatafiles)
    ns_cnt = string.replace(ns_cnt, "[DELDATADIRS]", deldatadirs)

    tmpnsi = os.path.normpath(install_base_dir+os.sep+env['BF_BUILDDIR']+os.sep+"00.blender_tmp.nsi")
    new_nsis = open(tmpnsi, 'w')
    new_nsis.write(ns_cnt)
    new_nsis.close()
    print "NSIS Installer script created"

    os.chdir(start_dir)
    print "Launching 'makensis'"

    cmdline = "makensis " + "\""+tmpnsi+"\""

    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    proc = subprocess.Popen(cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, startupinfo=startupinfo, shell = True)
    data, err = proc.communicate()
    rv = proc.wait()

    if rv != 0:
        print
        print data.strip().split("\n")[-1]
    return rv

def check_environ():
    problematic_envvars = ""
    for i in os.environ:
        try:
            os.environ[i].decode('ascii')
        except UnicodeDecodeError:
            problematic_envvars = problematic_envvars + "%s = %s\n" % (i, os.environ[i])
    if len(problematic_envvars)>0:
        print("================\n\n")
        print("@@ ABORTING BUILD @@\n")
        print("PROBLEM DETECTED WITH ENVIRONMENT")
        print("---------------------------------\n\n")
        print("A problem with one or more environment variable was found")
        print("Their value contain non-ascii characters. Check the below")
        print("list and override them locally to be ASCII-clean by doing")
        print("'set VARNAME=cleanvalue' on the command-line prior to")
        print("starting the build process:\n")
        print(problematic_envvars)
        return False
    else:
        return True
