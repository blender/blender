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

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "blenderartists.org",
"AI importer's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_import_ai.htm",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "0.1.1"

__bpydoc__ = """\
Paths Import imports paths from a selection of different formats:

- Gimp 1.0 -> 1.2.5;<br>
- Gimp 2.0;<br>
- AI PS-Adobe 2.0;<br>
- Inkscape (svg);<br>
- Postscript (ps/eps)

Usage:
    Run the script from "File->Import", select the desired format from the
pop-up menu and select the file to open.

Notes:<br>
    If the imported curve looks "messy", you may need to enter edit mode with the imported curve selected and toggle cyclic mode for it, by selecting all its points and pressing "c" or using the Curve menu in the 3d view header.
"""

#----------------------------------------------
# (c) jm soler juillet 2004, released under GPL
#    for the Blender 2.45 Python Scripts Bundle.
#----------------------------------------------
"""
Ce programme est libre, vous pouvez le redistribuer et/ou
le modifier selon les termes de la Licence Publique Générale GNU
publiée par la Free Software Foundation (version 2 ou bien toute
autre version ultérieure choisie par vous).

Ce programme est distribué car potentiellement utile, mais SANS
AUCUNE GARANTIE, ni explicite ni implicite, y compris les garanties
de commercialisation ou d'adaptation dans un but spécifique.
Reportez-vous à la Licence Publique Générale GNU pour plus de détails.

Vous devez avoir reçu une copie de la Licence Publique Générale GNU
en même temps que ce programme ; si ce n'est pas le cas, écrivez à la
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
MA 02111-1307, États-Unis.


This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA    
"""
import Blender

argv=__script__['arg']

if argv=='SVG':
  from paths_svg2obj import *
  fonctionSELECT = functionSELECT # can they all be called function?

elif argv=='AI':
  from paths_ai2obj import *

elif argv=='EPS':
  from paths_eps2obj import *

elif argv=='Gimp_1_0':
  from paths_gimp2obj import *

elif argv=='Gimp_2_0':
  from paths_svg2obj import *
  fonctionSELECT = functionSELECT # can they all be called function?

text = 'Import %s' % argv
Blender.Window.FileSelector (fonctionSELECT, text)