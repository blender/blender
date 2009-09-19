#
#  Filename : apriori_and_causal_density.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Selects the lines with high a priori density and 
#             subjects them to the causal density so as to avoid 
#             cluttering
#
#############################################################################  
#
#  Copyright (C) : Please refer to the COPYRIGHT file distributed 
#  with this source distribution. 
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
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
#############################################################################


from freestyle_init import *
from logical_operators import *
from PredicatesB1D import *
from PredicatesU1D import *
from shaders import *

upred = AndUP1D(QuantitativeInvisibilityUP1D(0), pyHighViewMapDensityUP1D(0.3, IntegrationType.LAST))
Operators.select(upred)
bpred = TrueBP1D()
Operators.bidirectionalChain(ChainPredicateIterator(upred, bpred), NotUP1D(QuantitativeInvisibilityUP1D(0)))
shaders_list = 	[
		ConstantThicknessShader(2), 
		ConstantColorShader(0.0, 0.0, 0.0,1)
		]
Operators.create(pyDensityUP1D(1,0.1, IntegrationType.MEAN), shaders_list)
