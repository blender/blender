#!BPY

"""
Name: 'Scripts Config Editor'
Blender: 236
Group: 'System'
Tooltip: 'View and edit available scripts configuration data'
"""

__author__ = "Willian P. Germano"
__version__ = "0.1 2005/04/14"
__email__ = ('scripts', 'Author, wgermano:ig*com*br')
__url__ = ('blender', 'blenderartists.org')

__bpydoc__ ="""\
This script can be used to view and edit configuration data stored
by other scripts.

Technical: this data is saved as dictionary keys with the
Blender.Registry module functions.  It is persistent while Blender is
running and, if the script's author chose to, is also saved to a file
in the scripts config data dir.

Usage:

- Start Screen:

To access any available key, select it from (one of) the menu(s).

Hotkeys:<br>
   ESC or Q: [Q]uit<br>
   H: [H]elp

- Keys Config Screen:

This screen exposes the configuration data for the chosen script key.  If the
buttons don't fit completely on the screen, you can scroll up or down with
arrow keys or a mouse wheel.  Leave the mouse pointer over any button to get
a tooltip about that option.

Any change can be reverted -- unless you have already applied it.

If the key is already stored in a config file, there will be a toggle button
(called 'file') that controls whether the changes will be written back to
the file or not.  If you just want to change the configuration for the current
session, simply unset that button.  Note, though, that data from files has
precedence over those keys already loaded in Blender, so if you re-run this
config editor, unsaved changes will not be seen.

Hotkeys:<br>
   ESC: back to Start Screen<br>
   Q: [Q]uit<br>
   U: [U]ndo changes<br>
   ENTER: apply changes (can't be reverted, then)<br>
   UP, DOWN Arrows and mouse wheel: scroll text up / down

Notes:

a) Available keys are determined by which scripts you use.  If the key you
expect isn't available (or maybe there are none or too few keys), either the
related script doesn't need or still doesn't support this feature or the key
has not been stored yet, in which case you just need to run that script once
to make its config data available.

b) There are two places where config data files can be saved: the
bpydata/config/ dir (1) inside the default scripts dir or (2) inside the user
defined Python scripts dir
(User Preferences window -> File Paths tab -> Python path).  If available,
(2) is the default and also the recommended option, because then fresh Blender
installations won't delete your config data.  To use this option, simply set a
dir for Python scripts at the User Preferences window and make sure this dir
has the subdirs bpydata/ and bpydata/config/ inside it.

c) The key called "General" in the "Other" menu has general config options.
All scripts where that data is relevant are recommended to access it and set
behaviors accordingly.
"""

# $Id$
#
# --------------------------------------------------------------------------
# config.py version 0.1 2005/04/08
# --------------------------------------------------------------------------
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

import Blender
from Blender import Draw, BGL, Registry, Window, sys as bsys
from Blender.Window import Theme
from BPyRegistry import LoadConfigData, SaveConfigData, HasConfigData,\
	BPY_KEY_IN_FILE

MAX_STR_LEN = 300 # max length for a string
MAX_ITEMS_NUM = 100 # max number for each type of button

# ---
# The "General" configure options key is managed from this script.
verbose = True
confirm_overwrite = True

tooltips = {
	'verbose': 'print script messages (info, warnings, errors) to the console',
	'confirm_overwrite': 'scripts should always confirm before overwriting files'
}

CFG_LIST = ['verbose', 'confirm_overwrite', 'tooltips']
KEY_NAME = 'General'

def update_registry():
	rd = {}
	for var in CFG_LIST:
		exec("rd['%s']=%s" % (var, var))
	Registry.SetKey(KEY_NAME, rd, True)

rd = Registry.GetKey('General', True)
if rd:
	try:
		for var in CFG_LIST[:-1]: # no need to update tooltips
			exec("%s=rd['%s']" % (var, var))
	except: update_registry()

else:
	update_registry()
# ---

# script globals:
CFGKEY = ''
LABELS = []
GD = {} # groups dict (includes "Other" for unmapped keys)
INDEX = 0 # to pass button indices to fs callbacks
FREEKEY_IDX = 0 # index of set of keys not mapped to a script name
KEYMENUS = []
ALL_SCRIPTS = {}
ALL_GROUPS = []
START_SCREEN  = 0
CONFIG_SCREEN = 1
DISK_UPDATE = True # write changed data to its config file

ACCEPTED_TYPES = [bool, int, float, str, unicode]

SCREEN = START_SCREEN

SCROLL_DOWN = 0

# events:
BEVT_START = 50
BEVT_EXIT = 0 + BEVT_START
BEVT_BACK = 1 + BEVT_START
BEVT_DISK = 2 + BEVT_START
BEVT_CANCEL = 3 + BEVT_START
BEVT_APPLY = 4 + BEVT_START
BEVT_HELP = 5 + BEVT_START
BEVT_DEL = 6 + BEVT_START
BEVT_KEYMENU = []
BUT_KEYMENU = []
BEVT_BOOL = 100
BEVT_INT = BEVT_BOOL + MAX_ITEMS_NUM
BEVT_FLOAT = BEVT_BOOL + 2*MAX_ITEMS_NUM
BEVT_STR = BEVT_BOOL + 3*MAX_ITEMS_NUM
BEVT_BROWSEDIR = BEVT_BOOL + 4*MAX_ITEMS_NUM
BEVT_BROWSEFILE = BEVT_BOOL + 5*MAX_ITEMS_NUM
BUT_TYPES = {
	bool: 0,
	int: 0,
	float: 0,
	str: 0
}

# Function definitions:

def get_keys():
	LoadConfigData() # loads all data from files in (u)scripts/bpydata/config/
	return [k for k in Registry.Keys() if k[0] != "_"]


def show_help(script = 'config.py'):
	Blender.ShowHelp(script)


def fs_dir_callback(pathname):
	global CFGKEY, INDEX

	pathname = bsys.dirname(pathname)
	datatypes = CFGKEY.sorteddata
	datatypes[str][INDEX][1] = pathname


def fs_file_callback(pathname):
	global CFGKEY, INDEX

	datatypes = CFGKEY.sorteddata
	datatypes[str][INDEX][1] = pathname


# parse Bpymenus file to get all script filenames
# (used to show help for a given key)
def fill_scripts_dict():
	global ALL_SCRIPTS, ALL_GROUPS

	group = ''
	group_len = 0
	sep = bsys.sep
	home = Blender.Get('homedir')
	if not home:
		errmsg = """
Can't find Blender's home dir and so can't find the
Bpymenus file automatically stored inside it, which
is needed by this script.  Please run the
Help -> System -> System Information script to get
information about how to fix this.
"""
		raise SystemError, errmsg
	fname = bsys.join(home, 'Bpymenus')
	if not bsys.exists(fname): return False
	f = file(fname, 'r')
	lines = f.readlines()
	f.close()
	for l in lines:
		if l.rfind('{') > 0:
			group = l.split()[0]
			ALL_GROUPS.append(group)
			group_len += 1
			continue
		elif l[0] != "'": continue
		fields = l.split("'")
		if len(fields) > 2:
			menuname = fields[1].replace('...','')
			fields = fields[2].split()
			if len(fields) > 1:
				fname = fields[1].split(sep)[-1]
				ALL_SCRIPTS[fname] = (menuname, group_len - 1)
	return True


def map_to_registered_script(name):
	global ALL_SCRIPTS

	if not name.endswith('.py'):
		name = "%s.py" % name
	if ALL_SCRIPTS.has_key(name):
		return ALL_SCRIPTS[name] # == (menuname, group index)
	return None


def reset():
	global LABELS, GD, KEYMENUS, KEYS

	# init_data is recalled when a key is deleted, so:
	LABELS = []
	GD = {}
	KEYMENUS = []
	KEYS = get_keys()


# gather all script info, fill gui menus
def init_data():
	global KEYS, GD, ALL_GROUPS, ALL_SCRIPTS, KEYMENUS, LABELS
	global BUT_KEYMENU, BEVT_KEYMENU, FREEKEY_IDX

	for k in ALL_GROUPS:
		GD[k] = []
	GD[None] = []

	for k in KEYS:
		res = map_to_registered_script(k)
		if res:
			GD[ALL_GROUPS[res[1]]].append((k, res[0]))
		else: GD[None].append((k, k))

	for k in GD.keys():
		if not GD[k]: GD.pop(k)

	if GD.has_key(None):
		GD['Other'] = GD[None]
		GD.pop(None)
		FREEKEY_IDX = -1

	BUT_KEYMENU = range(len(GD))

	for k in GD.keys():
		kmenu = ['Configuration Keys: %s%%t' % k]
		for j in GD[k]:
			kmenu.append(j[1])
		kmenu = "|".join(kmenu)
		KEYMENUS.append(kmenu)
		LABELS.append(k)

	if FREEKEY_IDX < 0:
		FREEKEY_IDX = LABELS.index('Other')

	length = len(KEYMENUS)
	BEVT_KEYMENU = range(1, length + 1)
	BUT_KEYMENU = range(length)


# for theme colors:
def float_colors(cols):
	return map(lambda x: x / 255.0, cols)



class Config:

	def __init__(self, key, has_group = True):
		global DISK_UPDATE

		self.key = key
		self.has_group = has_group
		self.name = key
		self.fromdisk = HasConfigData(key) & BPY_KEY_IN_FILE
		if not self.fromdisk: DISK_UPDATE = False
		else: DISK_UPDATE = True

		self.origdata = Registry.GetKey(key, True)
		data = self.data = self.origdata.copy()

		if not data:
			Draw.PupMenu('ERROR: couldn\'t find requested data')
			self.data = None
			return

		keys = data.keys()
		nd = {}
		for k in keys:
			nd[k.lower()] = k

		if nd.has_key('tooltips'):
			ndval = nd['tooltips']
			self.tips = data[ndval]
			data.pop(ndval)
		else: self.tips = 0

		if nd.has_key('limits'):
			ndval = nd['limits']
			self.limits = data[ndval]
			data.pop(ndval)
		else: self.limits = 0

		if self.has_group:
			scriptname = key
			if not scriptname.endswith('.py'):
				scriptname = "%s.py" % scriptname
		elif nd.has_key('script'):
				ndval = nd['script']
				scriptname = data[ndval]
				data.pop(ndval)
				if not scriptname.endswith('.py'):
					scriptname = "%s.py" % scriptname
		else: scriptname = None

		self.scriptname = scriptname

		self.sort()


	def needs_update(self): # check if user changed data
		data = self.data
		new = self.sorteddata

		for vartype in new.keys():
			for i in new[vartype]:
				if data[i[0]] != i[1]: return 1

		return 0 # no changes


	def update(self): # update original key
		global DISK_UPDATE

		data = self.data
		odata = self.origdata
		new = self.sorteddata
		for vartype in new.keys():
			for i in new[vartype]:
				if data[i[0]] != i[1]: data[i[0]] = i[1]
				if odata[i[0]] != i[1]: odata[i[0]] = i[1]

		if DISK_UPDATE: Registry.SetKey(self.key, odata, True)

	def delete(self):
		global DISK_UPDATE

		delmsg = 'OK?%t|Delete key from memory'
		if DISK_UPDATE:
			delmsg = "%s and from disk" % delmsg
		if Draw.PupMenu(delmsg) == 1:
			Registry.RemoveKey(self.key, DISK_UPDATE)
			return True

		return False


	def revert(self): # revert to original key
		data = self.data
		new = self.sorteddata
		for vartype in new.keys():
			for i in new[vartype]:
				if data[i[0]] != i[1]: i[1] = data[i[0]]


	def sort(self): # create a new dict with types as keys
		global ACCEPTED_TYPES, BUT_TYPES

		data = self.data
		datatypes = {}
		keys = [k for k in data.keys() if k[0] != '_']
		for k in keys:
			val = data[k]
			tval = type(val)
			if tval not in ACCEPTED_TYPES: continue
			if not datatypes.has_key(tval):
				datatypes[tval] = []
			datatypes[type(val)].append([k, val])
		if datatypes.has_key(unicode):
			if not datatypes.has_key(str): datatypes[str] = datatypes[unicode]
			else:
				for i in datatypes[unicode]: datatypes[str].append(i)
			datatypes.pop(unicode)
		for k in datatypes.keys():
			dk = datatypes[k]
			dk.sort()
			dk.reverse()
			BUT_TYPES[k] = range(len(dk))
		self.sorteddata = datatypes


# GUI:

# gui callbacks:

def gui(): # drawing the screen

	global SCREEN, START_SCREEN, CONFIG_SCREEN, KEYMENUS, LABELS
	global BEVT_KEYMENU, BUT_KEYMENU, CFGKEY
	global BUT_TYPES, SCROLL_DOWN, VARS_NUM

	WIDTH, HEIGHT = Window.GetAreaSize()

	theme = Theme.Get()[0]
	tui = theme.get('ui')
	ttxt = theme.get('text')

	COL_BG = float_colors(ttxt.back)
	COL_TXT = ttxt.text
	COL_TXTHI = ttxt.text_hi

	BGL.glClearColor(COL_BG[0],COL_BG[1],COL_BG[2],COL_BG[3])
	BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)
	BGL.glColor3ub(COL_TXT[0],COL_TXT[1], COL_TXT[2])

	if SCREEN == START_SCREEN:
		x = 10
		y = 10
		h = 20
		w = 90
		BGL.glRasterPos2i(x, y)
		Draw.Text('Select a configuration key to access it.  Press Q or ESC to leave.')
		km_len = len(KEYMENUS)
		km_columns = (WIDTH - x) / w
		if km_columns == 0: km_rows = km_len
		else:
			km_rows = km_len / km_columns
			if (km_len % km_columns): km_rows += 1
		if km_rows == 0: km_rows = 1
		ystart = y + 2*h*km_rows
		if ystart > (HEIGHT - 70): ystart = HEIGHT - 70
		y = ystart
		column = 1
		for i, km in enumerate(KEYMENUS):
			column += 1
			BGL.glRasterPos2i(x + 2, y + h + 5)
			Draw.Text(LABELS[i])
			BUT_KEYMENU[i] = Draw.Menu(km, BEVT_KEYMENU[i],
				x, y, w - 10, h, 0, 'Choose a key to access its configuration data')
			if column > km_columns:
				column = 1
				y -= 2*h
				if y < 35: break
				x = 10
			else: x += w
		x = 10
		y = 50 + ystart
		BGL.glColor3ub(COL_TXTHI[0], COL_TXTHI[1], COL_TXTHI[2])
		BGL.glRasterPos2i(x, y)
		Draw.Text('Scripts Configuration Editor')
		Draw.PushButton('help', BEVT_HELP, x, 22, 45, 16,
			'View help information about this script (hotkey: H)')

	elif SCREEN == CONFIG_SCREEN:
		x = y = 10
		h = 18
		data = CFGKEY.sorteddata
		tips = CFGKEY.tips
		fromdisk = CFGKEY.fromdisk
		limits = CFGKEY.limits
		VARS_NUM = 0
		for k in data.keys():
			VARS_NUM += len(data[k])
		lines = VARS_NUM + 5 # to account for header and footer
		y = lines*h
		if y > HEIGHT - 20: y = HEIGHT - 20
		BGL.glColor3ub(COL_TXTHI[0],COL_TXTHI[1], COL_TXTHI[2])
		BGL.glRasterPos2i(x, y)
		Draw.Text('Scripts Configuration Editor')
		y -= 20
		BGL.glColor3ub(COL_TXT[0],COL_TXT[1], COL_TXT[2])
		txtsize = 10
		if HEIGHT < lines*h:
			BGL.glRasterPos2i(10, 5)
			txtsize += Draw.Text('Arrow keys or mouse wheel to scroll, ')
		BGL.glRasterPos2i(txtsize, 5)
		Draw.Text('Q or ESC to return.')
		BGL.glRasterPos2i(x, y)
		Draw.Text('Key: "%s"' % CFGKEY.name)
		bh = 16
		bw = 45
		by = 16
		i = -1
		if CFGKEY.scriptname:
			i = 0
			Draw.PushButton('help', BEVT_HELP, x, by, bw, bh,
				'Show documentation for the script that owns this key (hotkey: H)')
		Draw.PushButton('back', BEVT_BACK, x + (1+i)*bw, by, bw, bh,
			'Back to config keys selection screen (hotkey: ESC)')
		Draw.PushButton('exit', BEVT_EXIT, x + (2+i)*bw, by, bw, bh,
			'Exit from Scripts Config Editor (hotkey: Q)')
		Draw.PushButton('revert', BEVT_CANCEL, x + (3+i)*bw, by, bw, bh,
			'Revert data to original values (hotkey: U)')
		Draw.PushButton('apply', BEVT_APPLY, x + (4+i)*bw, by, bw, bh,
			'Apply changes, if any (hotkey: ENTER)')
		delmsg = 'Delete this data key from memory'
		if fromdisk: delmsg = "%s and from disk" % delmsg
		Draw.PushButton('delete', BEVT_DEL, x + (5+i)*bw, by, bw, bh,
			'%s (hotkey: DELETE)' % delmsg)
		if fromdisk:
			Draw.Toggle("file", BEVT_DISK, x + 3 + (6+i)*bw, by, bw, bh, DISK_UPDATE,
				'Update also the file where this config key is stored')
		i = -1
		top = -1
		y -= 20
		yend = 30
		if data.has_key(bool) and y > 0:
			lst = data[bool]
			for l in lst:
				top += 1
				i += 1
				if top < SCROLL_DOWN: continue
				y -= h
				if y < yend: break
				w = 20
				tog = data[bool][i][1]
				if tips and tips.has_key(l[0]): tooltip = tips[l[0]]
				else: tooltip = "click to toggle"
				BUT_TYPES[bool][i] = Draw.Toggle("", BEVT_BOOL + i,
					x, y, w, h, tog, tooltip)
				BGL.glRasterPos2i(x + w + 3, y + 5)
				Draw.Text(l[0].lower().replace('_', ' '))
			i = -1
			y -= 5
		if data.has_key(int) and y > 0:
			lst = data[int]
			for l in lst:
				w = 70
				top += 1
				i += 1
				if top < SCROLL_DOWN: continue
				y -= h
				if y < yend: break
				val = data[int][i][1]
				if limits: min, max = limits[l[0]]
				else: min, max = 0, 10
				if tips and tips.has_key(l[0]): tooltip = tips[l[0]]
				else: tooltip = "click / drag to change"
				BUT_TYPES[int][i] = Draw.Number("", BEVT_INT + i,
					x, y, w, h, val, min, max, tooltip)
				BGL.glRasterPos2i(x + w + 3, y + 3)
				Draw.Text(l[0].lower().replace('_', ' '))
			i = -1
			y -= 5
		if data.has_key(float) and y > 0:
			lst = data[float]
			for l in lst:
				w = 70
				top += 1
				i += 1
				if top < SCROLL_DOWN: continue
				y -= h
				if y < yend: break
				val = data[float][i][1]
				if limits: min, max = limits[l[0]]
				else: min, max = 0.0, 1.0
				if tips and tips.has_key(l[0]): tooltip = tips[l[0]]
				else: tooltip = "click and drag to change"
				BUT_TYPES[float][i] = Draw.Number("", BEVT_FLOAT + i,
					x, y, w, h, val, min, max, tooltip)
				BGL.glRasterPos2i(x + w + 3, y + 3)
				Draw.Text(l[0].lower().replace('_', ' '))
			i = -1
			y -= 5
		if data.has_key(str) and y > 0:
			lst = data[str]
			for l in lst:
				top += 1
				i += 1
				if top < SCROLL_DOWN: continue
				y -= h
				if y < yend: break
				name = l[0].lower()
				is_dir = is_file = False
				if name.find('_dir', -4) > 0:	is_dir = True
				elif name.find('_file', -5) > 0: is_file = True
				w = WIDTH - 20
				wbrowse = 50
				if is_dir and w > wbrowse: w -= wbrowse
				if tips and tips.has_key(l[0]): tooltip = tips[l[0]]
				else: tooltip = "click to write a new string"
				name = name.replace('_',' ') + ': '
				if len(l[1]) > MAX_STR_LEN:
					l[1] = l[1][:MAX_STR_LEN]
				BUT_TYPES[str][i] = Draw.String(name, BEVT_STR + i,
					x, y, w, h, l[1], MAX_STR_LEN, tooltip)
				if is_dir:
					Draw.PushButton('browse', BEVT_BROWSEDIR + i, x+w+1, y, wbrowse, h,
						'click to open a file selector (pick any file in the desired dir)')
				elif is_file:
					Draw.PushButton('browse', BEVT_BROWSEFILE + i, x + w + 1, y, 50, h,
						'click to open a file selector')


def fit_scroll():
	global SCROLL_DOWN, VARS_NUM
	max = VARS_NUM - 1 # so last item is always visible
	if SCROLL_DOWN > max:
		SCROLL_DOWN = max
	elif SCROLL_DOWN < 0:
		SCROLL_DOWN = 0


def event(evt, val): # input events

	global SCREEN, START_SCREEN, CONFIG_SCREEN
	global SCROLL_DOWN, CFGKEY

	if not val: return

	if evt == Draw.ESCKEY:
		if SCREEN == START_SCREEN: Draw.Exit()
		else:
			if CFGKEY.needs_update():
				if Draw.PupMenu('UPDATE?%t|Data was changed') == 1:
					CFGKEY.update()
			SCREEN = START_SCREEN
			SCROLL_DOWN = 0
			Draw.Redraw()
		return
	elif evt == Draw.QKEY:
		if SCREEN == CONFIG_SCREEN and CFGKEY.needs_update():
			if Draw.PupMenu('UPDATE?%t|Data was changed') == 1:
				CFGKEY.update()
		Draw.Exit()
		return
	elif evt == Draw.HKEY:
		if SCREEN == START_SCREEN: show_help()
		elif CFGKEY.scriptname: show_help(CFGKEY.scriptname)
		return

	elif SCREEN == CONFIG_SCREEN:
		if evt in [Draw.DOWNARROWKEY, Draw.WHEELDOWNMOUSE]:
			SCROLL_DOWN += 1
			fit_scroll()
		elif evt in [Draw.UPARROWKEY, Draw.WHEELUPMOUSE]:
			SCROLL_DOWN -= 1
			fit_scroll()
		elif evt == Draw.UKEY:
			if CFGKEY.needs_update():
				CFGKEY.revert()
		elif evt == Draw.RETKEY or evt == Draw.PADENTER:
			if CFGKEY.needs_update():
				CFGKEY.update()
		elif evt == Draw.DELKEY:
			if CFGKEY.delete():
				reset()
				init_data()
				SCREEN = START_SCREEN
				SCROLL_DOWN = 0
		else: return
		Draw.Redraw()


def button_event(evt): # gui button events

	global SCREEN, START_SCREEN, CONFIG_SCREEN, CFGKEY, DISK_UPDATE
	global BEVT_KEYMENU, BUT_KEYMENU, BUT_TYPES, SCROLL_DOWN, GD, INDEX
	global BEVT_EXIT, BEVT_BACK, BEVT_APPLY, BEVT_CANCEL, BEVT_HELP, FREEKEY_IDX

	if SCREEN == START_SCREEN:
		for e in BEVT_KEYMENU:
			if evt == e:
				index = e - 1
				k = BUT_KEYMENU[index].val - 1
				CFGKEY = Config(GD[LABELS[index]][k][0], index != FREEKEY_IDX)
				if CFGKEY.data:
					SCREEN = CONFIG_SCREEN
					Draw.Redraw()
					return
		if evt == BEVT_EXIT:
			Draw.Exit()
		elif evt == BEVT_HELP:
			show_help()
		return

	elif SCREEN == CONFIG_SCREEN:
		datatypes = CFGKEY.sorteddata
		if evt >= BEVT_BROWSEFILE:
			INDEX = evt - BEVT_BROWSEFILE
			Window.FileSelector(fs_file_callback, 'Choose file')
		elif evt >= BEVT_BROWSEDIR:
			INDEX = evt - BEVT_BROWSEDIR
			Window.FileSelector(fs_dir_callback, 'Choose any file')
		elif evt >= BEVT_STR:
			var = BUT_TYPES[str][evt - BEVT_STR].val
			datatypes[str][evt - BEVT_STR][1] = var
		elif evt >= BEVT_FLOAT:
			var = BUT_TYPES[float][evt - BEVT_FLOAT].val
			datatypes[float][evt - BEVT_FLOAT][1] = var
		elif evt >= BEVT_INT:
			var = BUT_TYPES[int][evt - BEVT_INT].val
			datatypes[int][evt - BEVT_INT][1] = var
		elif evt >= BEVT_BOOL:
			var = datatypes[bool][evt - BEVT_BOOL][1]
			if var == True: var = False
			else: var = True
			datatypes[bool][evt - BEVT_BOOL][1] = var

		elif evt == BEVT_BACK:
			if SCREEN == CONFIG_SCREEN:
				SCREEN = START_SCREEN
				SCROLL_DOWN = 0
				Draw.Redraw()
		elif evt == BEVT_EXIT:
			if CFGKEY.needs_update():
				if Draw.PupMenu('UPDATE?%t|Data was changed') == 1:
					CFGKEY.update()
			Draw.Exit()
			return
		elif evt == BEVT_APPLY:
			if CFGKEY.needs_update():
				CFGKEY.update()
		elif evt == BEVT_CANCEL:
			if CFGKEY.needs_update():
				CFGKEY.revert()
		elif evt == BEVT_DEL:
			if CFGKEY.delete():
				reset()
				init_data()
				SCREEN = START_SCREEN
				SCROLL_DOWN = 0
		elif evt == BEVT_DISK:
			if DISK_UPDATE: DISK_UPDATE = False
			else: DISK_UPDATE = True
		elif evt == BEVT_HELP:
			show_help(CFGKEY.scriptname)
			return
		else:
			return
	Draw.Redraw()

# End of definitions


KEYS = get_keys()

if not KEYS:
	Draw.PupMenu("NO DATA: please read this help screen")
	Blender.ShowHelp('config.py')
else:
	fill_scripts_dict()
	init_data()
	Draw.Register(gui, event, button_event)
