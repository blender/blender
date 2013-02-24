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

from Freestyle import BinaryPredicate1D, GetZF1D, IntegrationType, Nature, SameShapeIdBP1D, ZDiscontinuityF1D
from Functions1D import pyViewMapGradientNormF1D

import random

class pyZBP1D(BinaryPredicate1D):
	def __call__(self, i1, i2):
		func = GetZF1D()
		return (func(i1) > func(i2))

class pyZDiscontinuityBP1D(BinaryPredicate1D):
	def __init__(self, iType = IntegrationType.MEAN):
		BinaryPredicate1D.__init__(self)
		self._GetZDiscontinuity = ZDiscontinuityF1D(iType)
	def __call__(self, i1, i2):
		return (self._GetZDiscontinuity(i1) > self._GetZDiscontinuity(i2))

class pyLengthBP1D(BinaryPredicate1D):
	def __call__(self, i1, i2):
		return (i1.length_2d > i2.length_2d)

class pySilhouetteFirstBP1D(BinaryPredicate1D):
	def __call__(self, inter1, inter2):
		bpred = SameShapeIdBP1D()
		if (bpred(inter1, inter2) != 1):
			return 0
		if (inter1.nature & Nature.SILHOUETTE):
			return (inter2.nature & Nature.SILHOUETTE) != 0
		return (inter1.nature == inter2.nature)

class pyNatureBP1D(BinaryPredicate1D):
	def __call__(self, inter1, inter2):
		return (inter1.nature & inter2.nature)

class pyViewMapGradientNormBP1D(BinaryPredicate1D):
	def __init__(self,l, sampling=2.0):
		BinaryPredicate1D.__init__(self)
		self._GetGradient = pyViewMapGradientNormF1D(l, IntegrationType.MEAN)
	def __call__(self, i1,i2):
		print("compare gradient")
		return (self._GetGradient(i1) > self._GetGradient(i2))

class pyShuffleBP1D(BinaryPredicate1D):
	def __init__(self):
		BinaryPredicate1D.__init__(self)
		random.seed(1)
	def __call__(self, inter1, inter2):
		r1 = random.uniform(0,1)
		r2 = random.uniform(0,1)
		return (r1<r2)
