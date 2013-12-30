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

#  Filename : apriori_and_causal_density.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Selects the lines with high a priori density and 
#             subjects them to the causal density so as to avoid 
#             cluttering

from freestyle.chainingiterators import ChainPredicateIterator
from freestyle.predicates import (
    AndUP1D,
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    TrueBP1D,
    pyDensityUP1D,
    pyHighViewMapDensityUP1D,
    )
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    )
from freestyle.types import IntegrationType, Operators

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyHighViewMapDensityUP1D(0.3, IntegrationType.LAST))
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectional_chain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = [
    ConstantThicknessShader(2),
    ConstantColorShader(0, 0, 0, 1),
    ]
Operators.create(pyDensityUP1D(1, 0.1, IntegrationType.MEAN), shaders_list)
