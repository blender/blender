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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

#  Filename : logical_operators.py
#  Authors  : Fredo Durand, Stephane Grabli, Francois Sillion, Emmanuel Turquin 
#  Date     : 08/04/2005
#  Purpose  : Logical unary predicates (functors) for 1D elements

from Freestyle import UnaryPredicate1D

class AndUP1D(UnaryPredicate1D):
    def __init__(self, pred1, pred2):
        UnaryPredicate1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2
    def __call__(self, inter):
        return self.__pred1(inter) and self.__pred2(inter)
    
class OrUP1D(UnaryPredicate1D):
    def __init__(self, pred1, pred2):
        UnaryPredicate1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2
    def __call__(self, inter):
        return self.__pred1(inter) or self.__pred2(inter)

class NotUP1D(UnaryPredicate1D):
    def __init__(self, pred):
        UnaryPredicate1D.__init__(self)
        self.__pred = pred
    def __call__(self, inter):
        return not self.__pred(inter)
