# -*- coding: utf8 -*-
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
#
import datetime, os, configparser, threading, tempfile

import bpy

config_paths = bpy.utils.script_paths()

'''
This path is set at the start of export, so that
calls to path_relative_to_export() can make all
exported paths relative to this one
'''
export_path = '';

def path_relative_to_export(p):
	'''
	Return a path that is relative to the export path
	'''
	global export_path
	p = filesystem_path(p)
	try:
		relp = os.path.relpath(p, os.path.dirname(export_path))
	except ValueError: # path on different drive on windows
		relp = p
	
	#print('Resolving rel path %s -> %s'  % (p, relp))
	
	return relp.replace('\\', '/')

def filesystem_path(p):
	'''
	Resolve a relative Blender path to a real filesystem path
	'''
	if p.startswith('//'):
		pout = bpy.path.abspath(p)
	else:
		pout = os.path.realpath(p)
	
	#print('Resolving FS path %s -> %s' % (p,pout))
	
	return pout.replace('\\', '/')

# TODO: - somehow specify TYPES to get/set from config

def find_config_value(module, section, key, default):
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
	global config_paths
	fc = []
	for p in config_paths:
		if os.path.exists(p) and os.path.isdir(p) and os.access(p, os.W_OK):
			fc.append( '/'.join([p, '%s.cfg' % module]))
	
	if len(fc) < 1:
		raise Exception('Cannot find a writable path to store %s config file' % module)
		
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
	'''
	Construct a safe scene filename
	'''
	filename = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
	if filename == '':
		filename = 'untitled'
	return bpy.path.clean_name(filename)

def temp_directory():
	'''
	Return the system temp directory
	'''
	return tempfile.gettempdir()

def temp_file(ext='tmp'):
	'''
	Get a temporary filename with the given extension
	'''
	tf, fn = tempfile.mkstemp(suffix='.%s'%ext)
	os.close(tf)
	return fn

class TimerThread(threading.Thread):
	'''
	Periodically call self.kick()
	'''
	STARTUP_DELAY = 0
	KICK_PERIOD = 8
	
	active = True
	timer = None
	
	LocalStorage = None
	
	def __init__(self, LocalStorage=dict()):
		threading.Thread.__init__(self)
		self.LocalStorage = LocalStorage
	
	def set_kick_period(self, period):
		self.KICK_PERIOD = period + self.STARTUP_DELAY
	
	def stop(self):
		self.active = False
		if self.timer is not None:
			self.timer.cancel()
			
	def run(self):
		'''
		Timed Thread loop
		'''
		
		while self.active:
			self.timer = threading.Timer(self.KICK_PERIOD, self.kick_caller)
			self.timer.start()
			if self.timer.isAlive(): self.timer.join()
	
	def kick_caller(self):
		if self.STARTUP_DELAY > 0:
			self.KICK_PERIOD -= self.STARTUP_DELAY
			self.STARTUP_DELAY = 0
		
		self.kick()
	
	def kick(self):
		'''
		sub-classes do their work here
		'''
		pass

def format_elapsed_time(t):
	'''
	Format a duration in seconds as an HH:MM:SS format time
	'''
	
	td = datetime.timedelta(seconds=t)
	min = td.days*1440  + td.seconds/60.0
	hrs = td.days*24	+ td.seconds/3600.0
	
	return '%i:%02i:%02i' % (hrs, min%60, td.seconds%60)
