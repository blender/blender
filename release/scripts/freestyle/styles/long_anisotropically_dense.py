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

#  Filename : long_anisotropically_dense.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Selects the lines that are long and have a high anisotropic 
#             a priori density and uses causal density 
#             to draw without cluttering. Ideally, half of the
#             selected lines are culled using the causal density.
#
#             ********************* WARNING *************************************
#             ******** The Directional a priori density maps must          ****** 
#             ******** have been computed prior to using this style module ******

from freestyle.chainingiterators import ChainSilhouetteIterator
from freestyle.functions import DensityF1D
from freestyle.predicates import (
    NotUP1D,
    QuantitativeInvisibilityUP1D,
    UnaryPredicate1D,
    pyHighDensityAnisotropyUP1D,
    pyHigherLengthUP1D,
    pyLengthBP1D,
    )
from freestyle.shaders import (
    ConstantColorShader,
    ConstantThicknessShader,
    SamplingShader,
    )
from freestyle.types import IntegrationType, Operators


## custom density predicate
class pyDensityUP1D(UnaryPredicate1D):
    def __init__(self, wsize, threshold, integration=IntegrationType.MEAN, sampling=2.0):
        UnaryPredicate1D.__init__(self)
        self._wsize = wsize
        self._threshold = threshold
        self._integration = integration
        self._func = DensityF1D(self._wsize, self._integration, sampling)
        self._func2 = DensityF1D(self._wsize, IntegrationType.MAX, sampling)

    def __call__(self, inter):
        c = self._func(inter)
        m = self._func2(inter)
        if c < self._threshold:
            return 1
        if m > 4*c:
            if c < 1.5*self._threshold:
                return 1
        return 0

Operators.select(QuantitativeInvisibilityUP1D(0))
Operators.bidirectional_chain(ChainSilhouetteIterator(),NotUP1D(QuantitativeInvisibilityUP1D(0)))
Operators.select(pyHigherLengthUP1D(40))
## selects lines having a high anisotropic a priori density
Operators.select(pyHighDensityAnisotropyUP1D(0.3,4))
Operators.sort(pyLengthBP1D())
shaders_list = [
    SamplingShader(2.0),
    ConstantThicknessShader(2),
    ConstantColorShader(0.2,0.2,0.25,1), 
    ]
## uniform culling
Operators.create(pyDensityUP1D(3.0,2.0e-2, IntegrationType.MEAN, 0.1), shaders_list)
