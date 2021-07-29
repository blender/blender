# -*- coding: utf-8 -*-
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# --------------------------------------------------------------------------
# Blender 2.5 Extensions Framework
# --------------------------------------------------------------------------
#
# Authors:
# Doug Hammond
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****
#
import configparser
import datetime
import os
import tempfile
import threading

import bpy

"""List of possibly appropriate paths to load/save addon config from/to"""
config_paths = []
if bpy.utils.user_resource('CONFIG', '') != "": config_paths.append(bpy.utils.user_resource('CONFIG', '', create=True))
if bpy.utils.user_resource('SCRIPTS', '') != "": config_paths.append(bpy.utils.user_resource('SCRIPTS', '', create=True))
# want to scan other script paths in reverse order, since the user path comes last
sp = [p for p in bpy.utils.script_paths() if p != '']
sp.reverse()
config_paths.extend(sp)

"""This path is set at the start of export, so that calls to
path_relative_to_export() can make all exported paths relative to
this one.
"""
export_path = '';

def path_relative_to_export(p):
    """Return a path that is relative to the export path"""
    global export_path
    p = filesystem_path(p)
    ep = os.path.dirname(export_path)

    if os.sys.platform[:3] == "win":
        # Prevent an error whereby python thinks C: and c: are different drives
        if p[1] == ':': p = p[0].lower() + p[1:]
        if ep[1] == ':': ep = ep[0].lower() + ep[1:]

    try:
        relp = os.path.relpath(p, ep)
    except ValueError: # path on different drive on windows
        relp = p

    return relp.replace('\\', '/')

def filesystem_path(p):
    """Resolve a relative Blender path to a real filesystem path"""
    if p.startswith('//'):
        pout = bpy.path.abspath(p)
    else:
        pout = os.path.realpath(p)

    return pout.replace('\\', '/')

# TODO: - somehow specify TYPES to get/set from config

def find_config_value(module, section, key, default):
    """Attempt to find the configuration value specified by string key
    in the specified section of module's configuration file. If it is
    not found, return default.

    """
    global config_paths
    fc = []
    for p in config_paths:
        if os.path.exists(p) and os.path.isdir(p) and os.access(p, os.W_OK):
            fc.append( '/'.join([p, '%s.cfg' % module]))

    if len(fc) < 1:
        print('Cannot find %s config file path' % module)
        return default

    cp = configparser.SafeConfigParser()

    cfg_files = cp.read(fc)
    if len(cfg_files) > 0:
        try:
            val = cp.get(section, key)
            if val == 'true':
                return True
            elif val == 'false':
                return False
            else:
                return val
        except:
            return default
    else:
        return default

def write_config_value(module, section, key, value):
    """Attempt to write the configuration value specified by string key
    in the specified section of module's configuration file.

    """
    global config_paths
    fc = []
    for p in config_paths:
        if os.path.exists(p) and os.path.isdir(p) and os.access(p, os.W_OK):
            fc.append( '/'.join([p, '%s.cfg' % module]))

    if len(fc) < 1:
        raise Exception('Cannot find a writable path to store %s config file' %
            module)

    cp = configparser.SafeConfigParser()

    cfg_files = cp.read(fc)

    if not cp.has_section(section):
        cp.add_section(section)

    if value == True:
        cp.set(section, key, 'true')
    elif value == False:
        cp.set(section, key, 'false')
    else:
        cp.set(section, key, value)

    if len(cfg_files) < 1:
        cfg_files = fc

    fh=open(cfg_files[0],'w')
    cp.write(fh)
    fh.close()

    return True

def scene_filename():
    """Construct a safe scene filename, using 'untitled' instead of ''"""
    filename = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
    if filename == '':
        filename = 'untitled'
    return bpy.path.clean_name(filename)

def temp_directory():
    """Return the system temp directory"""
    return tempfile.gettempdir()

def temp_file(ext='tmp'):
    """Get a temporary filename with the given extension. This function
    will actually attempt to create the file."""
    tf, fn = tempfile.mkstemp(suffix='.%s'%ext)
    os.close(tf)
    return fn

class TimerThread(threading.Thread):
    """Periodically call self.kick(). The period of time in seconds
    between calling is given by self.KICK_PERIOD, and the first call
    may be delayed by setting self.STARTUP_DELAY, also in seconds.
    self.kick() will continue to be called at regular intervals until
    self.stop() is called. Since this is a thread, calling self.join()
    may be wise after calling self.stop() if self.kick() is performing
    a task necessary for the continuation of the program.
    The object that creates this TimerThread may pass into it data
    needed during self.kick() as a dict LocalStorage in __init__().

    """
    STARTUP_DELAY = 0
    KICK_PERIOD = 8

    active = True
    timer = None

    LocalStorage = None

    def __init__(self, LocalStorage=dict()):
        threading.Thread.__init__(self)
        self.LocalStorage = LocalStorage

    def set_kick_period(self, period):
        """Adjust the KICK_PERIOD between __init__() and start()"""
        self.KICK_PERIOD = period + self.STARTUP_DELAY

    def stop(self):
        """Stop this timer. This method does not join()"""
        self.active = False
        if self.timer is not None:
            self.timer.cancel()

    def run(self):
        """Timed Thread loop"""
        while self.active:
            self.timer = threading.Timer(self.KICK_PERIOD, self.kick_caller)
            self.timer.start()
            if self.timer.isAlive(): self.timer.join()

    def kick_caller(self):
        """Intermediary between the kick-wait-loop and kick to allow
        adjustment of the first KICK_PERIOD by STARTUP_DELAY

        """
        if self.STARTUP_DELAY > 0:
            self.KICK_PERIOD -= self.STARTUP_DELAY
            self.STARTUP_DELAY = 0

        self.kick()

    def kick(self):
        """Sub-classes do their work here"""
        pass

def format_elapsed_time(t):
    """Format a duration in seconds as an HH:MM:SS format time"""

    td = datetime.timedelta(seconds=t)
    min = td.days*1440  + td.seconds/60.0
    hrs = td.days*24    + td.seconds/3600.0

    return '%i:%02i:%02i' % (hrs, min%60, td.seconds%60)

def getSequenceTexturePath(it, f):
    import bpy.path
    import os.path
    import string
    fd = it.image_user.frame_duration
    fs = it.image_user.frame_start
    fo = it.image_user.frame_offset
    cyclic = it.image_user.use_cyclic
    ext = os.path.splitext(it.image.filepath)[-1]
    fb = bpy.path.display_name_from_filepath(it.image.filepath)
    dn = os.path.dirname(it.image.filepath)
    rf = fb[::-1]
    nl = 0
    for i in range (len(fb)):
        if rf[i] in string.digits:
            nl += 1
        else:
            break
    head = fb[:len(fb)-nl]
    fnum = f
    if fs != 1:
        if f != fs:
            fnum -= (fs-1)
        elif f == fs:
            fnum = 1
    if fnum <= 0:
        if cyclic:
            fnum = fd - abs(fnum) % fd
        else:
            fnum = 1
    elif fnum > fd:
        if cyclic:
            fnum = fnum % fd
        else:
            fnum = fd
    fnum += fo
    return dn + "/" + head + str(fnum).rjust(nl, "0") + ext
