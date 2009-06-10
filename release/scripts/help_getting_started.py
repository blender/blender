#!BPY
"""
Name: 'Getting Started'
Blender: 248
Group: 'Help'
Tooltip: 'Help for new users'
"""

__author__ = "Matt Ebb"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0.1"
__bpydoc__ = """\
This script opens the user's default web browser at www.blender.org's
"Getting Started" page.
"""

# $Id$
#
# --------------------------------------------------------------------------
# Getting Started Help Menu Item
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
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
try: import webbrowser
except: webbrowser = None

if webbrowser:
    webbrowser.open('http://www.blender.org/education-help/tutorials/getting-started/')
else:
    Blender.Draw.PupMenu("Error%t|This script requires a full python installation")


