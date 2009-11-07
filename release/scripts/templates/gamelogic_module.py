# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# This module can be accessed by a python controller with
# its execution method set to 'Module'
# * Set the module string to "gamelogic_module.main" (without quotes)
# * When renaming the script it MUST have a .py extension
# * External text modules are supported as long as they are at
#   the same location as the blendfile or one of its libraries.

import GameLogic

# variables defined here will only be set once when the
# module is first imported. Set object spesific vars
# inside the function if you intend to use the module
# with multiple objects.

def main(cont):
    own = cont.owner

    sens = cont.sensors['mySensor']
    actu = cont.actuators['myActuator']

    if sens.positive:
        cont.activate(actu)
    else:
        cont.deactivate(actu)

# dont call main(GameLogic.getCurrentController()), the py controller will
