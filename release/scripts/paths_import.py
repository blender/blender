#!BPY

"""
Name: 'Paths (.svg, .ps, .eps, .ai, Gimp)'
Blender: 233
Group: 'Import'
Submenu: 'Gimp 1.0 - 1.2.5' Gimp_1_0
Submenu: 'Gimp 2.0' Gimp_2_0
Submenu: 'Illustrator (.ai) PS-Adobe-2.0' AI
Submenu: 'InkScape (.svg)' SVG
Submenu: 'Postscript (.eps/.ps) PS-Adobe-2.0' EPS
Tip: 'Import a path from any of a set of formats (still experimental)'
"""

import Blender

argv=__script__['arg']

if argv=='SVG':
  from mod_svg2obj import *

elif argv=='AI':
  from mod_ai2obj import *

elif argv=='EPS':
  from mod_eps2obj import *

elif argv=='Gimp_1_0':
  from mod_gimp2obj import *

elif argv=='Gimp_2_0':
  from mod_svg2obj import *

text = 'Import %s' % argv
Blender.Window.FileSelector (fonctionSELECT, text)
