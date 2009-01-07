# --------------------------------------------------------------------------
# Module BPyRegistry version 0.1
#   Helper functions to store / restore configuration data.
# --------------------------------------------------------------------------
# $Id$
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2004: Willian P. Germano, wgermano _at_ ig.com.br
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# --------------------------------------------------------------------------

# The Registry is a Python dictionary that is kept in Blender for as long as
# the program is running, where scripts can store / restore persistent data
# (data that is not lost when the script exits).  This module provides
# functions to save and restore Registry entries as config data in the
# bpydata/config folder.  Scripts just need to give an extra parameter to
# the Blender.Registry.Get/Set() functions to have their data automatically
# saved and restored when needed.
#
# Note: entries starting with an underscore are not saved, so script authors
# can use that fact to define data that is not meant to be stored in a
# config file.  Example: data to be passed to another script and references to
# invalid data, like Blender objects and any function or method.
#
# Check the Blender.Registry documentation for more information.

import Blender
from Blender import Registry, sys as bsys

_EXT = '.cfg' # file extension for saved config data

# limits:
#MAX_ITEMS_NUM = 60 # max number of keys per dict and itens per list and tuple
#MAX_STR_LEN = 300 # max string length (remember this is just for config data)

_CFG_DIR = ''
if Blender.Get('udatadir'):
	_CFG_DIR = Blender.sys.join(Blender.Get('udatadir'), 'config')
if not _CFG_DIR or not bsys.exists(_CFG_DIR):
	_CFG_DIR = Blender.sys.join(Blender.Get('datadir'), 'config')
if not bsys.exists(_CFG_DIR):
	_CFG_DIR = ''

# to compare against, so we don't write to a cvs tree:
_CVS_SUBPATH = 'release/scripts/bpydata/config/'
if bsys.dirsep == '\\':
	_CVS_SUBPATH = _CVS_SUBPATH.replace('/', '\\')

_KEYS = [k for k in Registry.Keys() if k[0] != '_']

# _ITEMS_NUM = 0

def _sanitize(o):
	"Check recursively that all objects are valid, set invalid ones to None"

	# global MAX_ITEMS_NUM, MAX_STR_LEN, _ITEMS_NUM

	valid_types = [int, float, bool, long, type]
	valid_checked_types = [str, unicode]
	# Only very simple types are considered valid for configuration data,
	# functions, methods and Blender objects (use their names instead) aren't.

	t = type(o)

	if t == dict:
		'''
		_ITEMS_NUM += len(o)
		if _ITEMS_NUM > MAX_ITEMS_NUM:
			return None
		'''
		for k, v in o.iteritems():
			o[k] = _sanitize(v)
	elif t in [list, tuple]:
		'''
		_ITEMS_NUM += len(o)
		if _ITEMS_NUM > MAX_ITEMS_NUM:
			return None
		'''
		return [_sanitize(i) for i in o]
	elif t in valid_types:
		return o
	elif t in valid_checked_types:
		'''
		if len(o) > MAX_STR_LEN:
			o = o[:MAX_STR_LEN]
		'''
		return o
	else: return None

	return o


def _dict_to_str(name, d):
	"Return a pretty-print version of the passed dictionary"
	if not d: return 'None' # d can be None if there was no config to pass
	
	if name: l = ['%s = {' % name]
	else: l = ['{']
	#keys = d.keys()
	for k,v in d.iteritems(): # .keys()
		if type(v) == dict:
			l.append("'%s': %s" % (k, _dict_to_str(None, v)))
		else:
			l.append("'%s': %s," % (k, repr(v)))
	if name: l.append('}')
	else: l.append('},')
	return "\n".join(l)

_HELP_MSG = """
Please create a valid scripts config dir tree either by
copying release/scripts/ tree to your <blenderhome> dir
or by copying release/scripts/bpydata/ tree to a user
defined scripts dir that you can set in the 
User Preferences -> Paths tab -> Python path input box.
"""

def _check_dir():
	global _CFG_DIR, _CVS_SUBPATH, _HELP_MSG

	if not _CFG_DIR:
		errmsg = "scripts config dir not found!\n%s" % _HELP_MSG
		raise IOError, errmsg
	elif _CFG_DIR.find(_CVS_SUBPATH) > 0:
		errmsg = """
Your scripts config dir:\n%s
seems to reside in your local Blender's cvs tree.\n%s""" % (_CFG_DIR, _HELP_MSG)
		raise SystemError, errmsg
	else: return


# API:

BPY_KEY_MISSING = 0
BPY_KEY_IN_REGISTRY = 1
BPY_KEY_IN_FILE = 2

def HasConfigData (key):
	"""
	Check if the given key exists, either already loaded in the Registry dict or
	as a file in the script data config dir.
	@type key: string
	@param key: a given key name.
	@returns:
		- 0: key does not exist;
		- 1: key exists in the Registry dict only;
		- 2: key exists as a file only;
		- 3: key exists in the Registry dict and also as a file.
	@note: for readability it's better to check against the constant bitmasks
		BPY_KEY_MISSING = 0, BPY_KEY_IN_REGISTRY = 1 and BPY_KEY_IN_FILE = 2.
	"""

	fname = bsys.join(_CFG_DIR, "%s%s" % (key, _EXT))

	result = BPY_KEY_MISSING
	if key in Registry.Keys(): result |= BPY_KEY_IN_REGISTRY
	if bsys.exists(fname): result |= BPY_KEY_IN_FILE

	return result


def LoadConfigData (key = None):
	"""
	Load config data from file(s) to the Registry dictionary.
	@type key: string
	@param key: a given key name.  If None (default), all available keys are
		loaded.
	@returns: None
	"""

	_check_dir()

	import os

	if not key:
		files = \
			[bsys.join(_CFG_DIR, f) for f in os.listdir(_CFG_DIR) if f.endswith(_EXT)]
	else:
		files = []
		fname = bsys.join(_CFG_DIR, "%s%s" % (key, _EXT))
		if bsys.exists(fname): files.append(fname)

	for p in files:
		try:
			f = file(p, 'r')
			lines = f.readlines()
			f.close()
			if lines: # Lines may be blank
				mainkey = lines[0].split('=')[0].strip()
				pysrc = "\n".join(lines)
				exec(pysrc)
				exec("Registry.SetKey('%s', %s)" % (str(mainkey), mainkey))
		except Exception, e:
			raise Warning(e) # Resend exception as warning


def RemoveConfigData (key = None):
	"""
	Remove this key's config file from the <(u)datadir>/config/ folder.
	@type key: string
	@param key: the name of the key to be removed.  If None (default) all
		available config files are deleted.
	"""

	_check_dir()

	if not key:
		files = \
			[bsys.join(_CFG_DIR, f) for f in os.listdir(_CFG_DIR) if f.endswith(_EXT)]
	else:
		files = []
		fname = bsys.join(_CFG_DIR, "%s%s" % (key, _EXT))
		if bsys.exists(fname): files.append(fname)

	import os

	try:
		for p in files:
			os.remove(p) # remove the file(s)
	except Exception, e:
		raise Warning(e) # Resend exception as warning


def SaveConfigData (key = None):
	"""
	Save Registry key(s) as file(s) in the <(u)datadir>/config/ folder.
	@type key: string
	@param key: the name of the key to be saved.  If None (default) all
		available keys are saved.
	"""

	global _KEYS, _CFG_DIR

	_check_dir()

	if key: keys = [key]
	else: keys = _KEYS

	for mainkey in keys:
		cfgdict = Registry.GetKey(mainkey).copy()
		for k in cfgdict: # .keys()
			if not k or k[0] == '_':
				del cfgdict[k]

		if not cfgdict: continue

		try:
			filename = bsys.join(_CFG_DIR, "%s%s" % (mainkey, _EXT))
			f = file(filename, 'w')
			output = _dict_to_str(mainkey, _sanitize(cfgdict))
			if output!='None':
				f.write(output)
				f.close()
		except Exception, e:
			raise Warning(e) # Resend exception as warning
