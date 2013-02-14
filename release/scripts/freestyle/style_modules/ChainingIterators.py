#
#  Filename : ChainingIterators.py
#  Author   : Stephane Grabli
#  Date     : 04/08/2005
#  Purpose  : Chaining Iterators to be used with chaining operators
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

## the natural chaining iterator
## It follows the edges of same nature following the topology of
## objects with  preseance on silhouettes, then borders, 
## then suggestive contours, then everything else. It doesn't chain the same ViewEdge twice
## You can specify whether to stay in the selection or not.
class pyChainSilhouetteIterator(ChainingIterator):
	def __init__(self, stayInSelection=1):
		ChainingIterator.__init__(self, stayInSelection, 1,None,1)
	def getExactTypeName(self):
		return "pyChainSilhouetteIterator"
	def init(self):
		pass
	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for i in range(len(natures)):
				currentNature = self.getCurrentEdge().nature
				if (natures[i] & currentNature) != 0:
					count=0
					while not it.isEnd():
						visitNext = 0
						oNature = it.getObject().nature
						if (oNature & natures[i]) != 0:
							if natures[i] != oNature:
								for j in range(i):
									if (natures[j] & oNature) != 0:
										visitNext = 1
										break
								if visitNext != 0:
									break	 
							count = count+1
							winner = it.getObject()
						it.increment()
					if count != 1:
						winner = None
					break
		return winner

## the natural chaining iterator
## It follows the edges of same nature on the same
## objects with  preseance on silhouettes, then borders, 
## then suggestive contours, then everything else. It doesn't chain the same ViewEdge twice
## You can specify whether to stay in the selection or not.
## You can specify whether to chain iterate over edges that were 
## already visited or not.
class pyChainSilhouetteGenericIterator(ChainingIterator):
	def __init__(self, stayInSelection=1, stayInUnvisited=1):
		ChainingIterator.__init__(self, stayInSelection, stayInUnvisited,None,1)
	def getExactTypeName(self):
		return "pyChainSilhouetteGenericIterator"
	def init(self):
		pass
	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for i in range(len(natures)):
				currentNature = self.getCurrentEdge().nature
				if (natures[i] & currentNature) != 0:
					count=0
					while not it.isEnd():
						visitNext = 0
						oNature = it.getObject().nature
						ve = it.getObject()
						if ve.id == self.getCurrentEdge().id:
							it.increment()
							continue
						if (oNature & natures[i]) != 0:
							if natures[i] != oNature:
								for j in range(i):
									if (natures[j] & oNature) != 0:
										visitNext = 1
										break
								if visitNext != 0:
									break	 
							count = count+1
							winner = ve
						it.increment()
					if count != 1:
						winner = None
					break
		return winner
			
class pyExternalContourChainingIterator(ChainingIterator):
	def __init__(self):
		ChainingIterator.__init__(self, 0, 1,None,1)
		self._isExternalContour = ExternalContourUP1D()
		
	def getExactTypeName(self):
		return "pyExternalContourIterator"
	
	def init(self):
		self._nEdges = 0
		self._isInSelection = 1

	def checkViewEdge(self, ve, orientation):
		if orientation != 0:
			vertex = ve.second_svertex()
		else:
			vertex = ve.first_svertex()
		it = AdjacencyIterator(vertex,1,1)
		while not it.isEnd():
			ave = it.getObject()
			if self._isExternalContour(ave):
				return 1
			it.increment()
		print("pyExternlContourChainingIterator : didn't find next edge")
		return 0
	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		while not it.isEnd():
			ve = it.getObject()
			if self._isExternalContour(ve):
				if ve.time_stamp == GetTimeStampCF():
					winner = ve
			it.increment()
		
		self._nEdges = self._nEdges+1
		if winner is None:
			orient = 1
			it = AdjacencyIterator(iter)
			while not it.isEnd():
				ve = it.getObject()
				if it.isIncoming() != 0: # FIXME
					orient = 0
				good = self.checkViewEdge(ve,orient)
				if good != 0:
					winner = ve
				it.increment()
		return winner

## the natural chaining iterator
## with a sketchy multiple touch
class pySketchyChainSilhouetteIterator(ChainingIterator):
	def __init__(self, nRounds=3,stayInSelection=1):
		ChainingIterator.__init__(self, stayInSelection, 0,None,1)
		self._timeStamp = GetTimeStampCF()+nRounds
		self._nRounds = nRounds
	def getExactTypeName(self):
		return "pySketchyChainSilhouetteIterator"
	def init(self):
		self._timeStamp = GetTimeStampCF()+self._nRounds
	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for i in range(len(natures)):
				currentNature = self.getCurrentEdge().nature
				if (natures[i] & currentNature) != 0:
					count=0
					while not it.isEnd():
						visitNext = 0
						oNature = it.getObject().nature
						ve = it.getObject()
						if ve.id == self.getCurrentEdge().id:
							it.increment()
							continue
						if (oNature & natures[i]) != 0:
							if (natures[i] != oNature) != 0:
								for j in range(i):
									if (natures[j] & oNature) != 0:
										visitNext = 1
										break
								if visitNext != 0:
									break	 
							count = count+1
							winner = ve
						it.increment()
					if count != 1:
						winner = None
					break
		if winner is None:
			winner = self.getCurrentEdge()
		if winner.chaining_time_stamp == self._timeStamp:
			winner = None
		return winner


# Chaining iterator designed for sketchy style.
# can chain several times the same ViewEdge
# in order to produce multiple strokes per ViewEdge.
class pySketchyChainingIterator(ChainingIterator):
	def __init__(self, nRounds=3, stayInSelection=1):
		ChainingIterator.__init__(self, stayInSelection, 0,None,1)
		self._timeStamp = GetTimeStampCF()+nRounds
		self._nRounds = nRounds
	def getExactTypeName(self):
		return "pySketchyChainingIterator"

	def init(self):
		self._timeStamp = GetTimeStampCF()+self._nRounds

	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		while not it.isEnd():
			ve = it.getObject()
			if ve.id == self.getCurrentEdge().id:
				it.increment()
				continue
			winner = ve
			it.increment()
		if winner is None:
			winner = self.getCurrentEdge()
		if winner.chaining_time_stamp == self._timeStamp:
			return None
		return winner


## Chaining iterator that fills small occlusions
## 	percent
##		The max length of the occluded part 
##		expressed in % of the total chain length
class pyFillOcclusionsRelativeChainingIterator(ChainingIterator):
	def __init__(self, percent):
		ChainingIterator.__init__(self, 0, 1,None,1)
		self._length = 0
		self._percent = float(percent)
	def getExactTypeName(self):
		return "pyFillOcclusionsChainingIterator"
	def init(self):
		# each time we're evaluating a chain length 
		# we try to do it once. Thus we reinit 
		# the chain length here:
		self._length = 0
	def traverse(self, iter):
		winner = None
		winnerOrientation = 0
		print(self.getCurrentEdge().id.first, self.getCurrentEdge().id.second)
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					if it.isIncoming() == 0: # FIXME
						winnerOrientation = 1
					else:
						winnerOrientation = 0
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for nat in natures:
				if (self.getCurrentEdge().nature & nat) != 0:
					count=0
					while not it.isEnd():
						ve = it.getObject()
						if (ve.nature & nat) != 0:
							count = count+1
							winner = ve
							if it.isIncoming() == 0: # FIXME
								winnerOrientation = 1
							else:
								winnerOrientation = 0
						it.increment()
					if count != 1:
						winner = None
					break
		if winner is not None:
			# check whether this edge was part of the selection
			if winner.time_stamp != GetTimeStampCF():
				#print("---", winner.id.first, winner.id.second)
				# if not, let's check whether it's short enough with
				# respect to the chain made without staying in the selection
				#------------------------------------------------------------
				# Did we compute the prospective chain length already ?
				if self._length == 0:
					#if not, let's do it
					_it = pyChainSilhouetteGenericIterator(0,0)
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					_it.init()
					while not _it.isEnd():
						ve = _it.getObject()
						#print("--------", ve.id.first, ve.id.second)
						self._length = self._length + ve.length_2d
						_it.increment()
						if _it.isBegin():
							break;
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					if not _it.isBegin():
						_it.decrement()
						while (not _it.isEnd()) and (not _it.isBegin()):
							ve = _it.getObject()
							#print("--------", ve.id.first, ve.id.second)
							self._length = self._length + ve.length_2d
							_it.decrement()

				# let's do the comparison:
				# nw let's compute the length of this connex non selected part:
				connexl = 0
				_cit = pyChainSilhouetteGenericIterator(0,0)
				_cit.setBegin(winner)
				_cit.setCurrentEdge(winner)
				_cit.setOrientation(winnerOrientation)
				_cit.init()
				while _cit.isEnd() == 0 and _cit.getObject().time_stamp != GetTimeStampCF():
					ve = _cit.getObject()
					#print("-------- --------", ve.id.first, ve.id.second)
					connexl = connexl + ve.length_2d
					_cit.increment()
				if connexl > self._percent * self._length:
					winner = None
		return winner

## Chaining iterator that fills small occlusions
## 	size
##		The max length of the occluded part 
##		expressed in pixels
class pyFillOcclusionsAbsoluteChainingIterator(ChainingIterator):
	def __init__(self, length):
		ChainingIterator.__init__(self, 0, 1,None,1)
		self._length = float(length)
	def getExactTypeName(self):
		return "pySmallFillOcclusionsChainingIterator"
	def init(self):
		pass
	def traverse(self, iter):
		winner = None
		winnerOrientation = 0
		#print(self.getCurrentEdge().id.first, self.getCurrentEdge().id.second)
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					if it.isIncoming() == 0: # FIXME
						winnerOrientation = 1
					else:
						winnerOrientation = 0
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for nat in natures:
				if (self.getCurrentEdge().nature & nat) != 0:
					count=0
					while not it.isEnd():
						ve = it.getObject()
						if (ve.nature & nat) != 0:
							count = count+1
							winner = ve
							if it.isIncoming() == 0: # FIXME
								winnerOrientation = 1
							else:
								winnerOrientation = 0
						it.increment()
					if count != 1:
						winner = None
					break
		if winner is not None:
			# check whether this edge was part of the selection
			if winner.time_stamp != GetTimeStampCF():
				#print("---", winner.id.first, winner.id.second)
				# nw let's compute the length of this connex non selected part:
				connexl = 0
				_cit = pyChainSilhouetteGenericIterator(0,0)
				_cit.setBegin(winner)
				_cit.setCurrentEdge(winner)
				_cit.setOrientation(winnerOrientation)
				_cit.init()
				while _cit.isEnd() == 0 and _cit.getObject().time_stamp != GetTimeStampCF():
					ve = _cit.getObject()
					#print("-------- --------", ve.id.first, ve.id.second)
					connexl = connexl + ve.length_2d
					_cit.increment()
				if connexl > self._length:
					winner = None
		return winner


## Chaining iterator that fills small occlusions
## 	percent
##		The max length of the occluded part 
##		expressed in % of the total chain length
class pyFillOcclusionsAbsoluteAndRelativeChainingIterator(ChainingIterator):
	def __init__(self, percent, l):
		ChainingIterator.__init__(self, 0, 1,None,1)
		self._length = 0
		self._absLength = l
		self._percent = float(percent)
	def getExactTypeName(self):
		return "pyFillOcclusionsChainingIterator"
	def init(self):
		# each time we're evaluating a chain length 
		# we try to do it once. Thus we reinit 
		# the chain length here:
		self._length = 0
	def traverse(self, iter):
		winner = None
		winnerOrientation = 0
		print(self.getCurrentEdge().id.first, self.getCurrentEdge().id.second)
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					if it.isIncoming() == 0: # FIXME
						winnerOrientation = 1
					else:
						winnerOrientation = 0
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for nat in natures:
				if (self.getCurrentEdge().nature & nat) != 0:
					count=0
					while not it.isEnd():
						ve = it.getObject()
						if (ve.nature & nat) != 0:
							count = count+1
							winner = ve
							if it.isIncoming() == 0: # FIXME
								winnerOrientation = 1
							else:
								winnerOrientation = 0
						it.increment()
					if count != 1:
						winner = None
					break
		if winner is not None:
			# check whether this edge was part of the selection
			if winner.time_stamp != GetTimeStampCF():
				#print("---", winner.id.first, winner.id.second)
				# if not, let's check whether it's short enough with
				# respect to the chain made without staying in the selection
				#------------------------------------------------------------
				# Did we compute the prospective chain length already ?
				if self._length == 0:
					#if not, let's do it
					_it = pyChainSilhouetteGenericIterator(0,0)
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					_it.init()
					while not _it.isEnd():
						ve = _it.getObject()
						#print("--------", ve.id.first, ve.id.second)
						self._length = self._length + ve.length_2d
						_it.increment()
						if _it.isBegin():
							break;
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					if not _it.isBegin():
						_it.decrement()
						while (not _it.isEnd()) and (not _it.isBegin()):
							ve = _it.getObject()
							#print("--------", ve.id.first, ve.id.second)
							self._length = self._length + ve.length_2d
							_it.decrement()

				# let's do the comparison:
				# nw let's compute the length of this connex non selected part:
				connexl = 0
				_cit = pyChainSilhouetteGenericIterator(0,0)
				_cit.setBegin(winner)
				_cit.setCurrentEdge(winner)
				_cit.setOrientation(winnerOrientation)
				_cit.init()
				while _cit.isEnd() == 0 and _cit.getObject().time_stamp != GetTimeStampCF():
					ve = _cit.getObject()
					#print("-------- --------", ve.id.first, ve.id.second)
					connexl = connexl + ve.length_2d
					_cit.increment()
				if (connexl > self._percent * self._length) or (connexl > self._absLength):
					winner = None
		return winner

## Chaining iterator that fills small occlusions without caring about the 
## actual selection
## 	percent
##		The max length of the occluded part 
##		expressed in % of the total chain length
class pyFillQi0AbsoluteAndRelativeChainingIterator(ChainingIterator):
	def __init__(self, percent, l):
		ChainingIterator.__init__(self, 0, 1,None,1)
		self._length = 0
		self._absLength = l
		self._percent = float(percent)
	def getExactTypeName(self):
		return "pyFillOcclusionsChainingIterator"
	def init(self):
		# each time we're evaluating a chain length 
		# we try to do it once. Thus we reinit 
		# the chain length here:
		self._length = 0
	def traverse(self, iter):
		winner = None
		winnerOrientation = 0
		print(self.getCurrentEdge().id.first, self.getCurrentEdge().id.second)
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				if ve.id == mateVE.id:
					winner = ve
					if it.isIncoming() == 0: # FIXME
						winnerOrientation = 1
					else:
						winnerOrientation = 0
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for nat in natures:
				if (self.getCurrentEdge().nature & nat) != 0:
					count=0
					while not it.isEnd():
						ve = it.getObject()
						if (ve.nature & nat) != 0:
							count = count+1
							winner = ve
							if it.isIncoming() == 0: # FIXME
								winnerOrientation = 1
							else:
								winnerOrientation = 0
						it.increment()
					if count != 1:
						winner = None
					break
		if winner is not None:
			# check whether this edge was part of the selection
			if winner.qi != 0:
				#print("---", winner.id.first, winner.id.second)
				# if not, let's check whether it's short enough with
				# respect to the chain made without staying in the selection
				#------------------------------------------------------------
				# Did we compute the prospective chain length already ?
				if self._length == 0:
					#if not, let's do it
					_it = pyChainSilhouetteGenericIterator(0,0)
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					_it.init()
					while not _it.isEnd():
						ve = _it.getObject()
						#print("--------", ve.id.first, ve.id.second)
						self._length = self._length + ve.length_2d
						_it.increment()
						if _it.isBegin():
							break;
					_it.setBegin(winner)
					_it.setCurrentEdge(winner)
					_it.setOrientation(winnerOrientation)
					if not _it.isBegin():
						_it.decrement()
						while (not _it.isEnd()) and (not _it.isBegin()):
							ve = _it.getObject()
							#print("--------", ve.id.first, ve.id.second)
							self._length = self._length + ve.length_2d
							_it.decrement()

				# let's do the comparison:
				# nw let's compute the length of this connex non selected part:
				connexl = 0
				_cit = pyChainSilhouetteGenericIterator(0,0)
				_cit.setBegin(winner)
				_cit.setCurrentEdge(winner)
				_cit.setOrientation(winnerOrientation)
				_cit.init()
				while not _cit.isEnd() and _cit.getObject().qi != 0:
					ve = _cit.getObject()
					#print("-------- --------", ve.id.first, ve.id.second)
					connexl = connexl + ve.length_2d
					_cit.increment()
				if (connexl > self._percent * self._length) or (connexl > self._absLength):
					winner = None
		return winner


## the natural chaining iterator
## It follows the edges of same nature on the same
## objects with  preseance on silhouettes, then borders, 
## then suggestive contours, then everything else. It doesn't chain the same ViewEdge twice
## You can specify whether to stay in the selection or not.
class pyNoIdChainSilhouetteIterator(ChainingIterator):
	def __init__(self, stayInSelection=1):
		ChainingIterator.__init__(self, stayInSelection, 1,None,1)
	def getExactTypeName(self):
		return "pyChainSilhouetteIterator"
	def init(self):
		pass
	def traverse(self, iter):
		winner = None
		it = AdjacencyIterator(iter)
		tvertex = self.getVertex()
		if type(tvertex) is TVertex:
			mateVE = tvertex.get_mate(self.getCurrentEdge())
			while not it.isEnd():
				ve = it.getObject()
				feB = self.getCurrentEdge().last_fedge
				feA = ve.first_fedge
				vB = feB.second_svertex
				vA = feA.first_svertex
				if vA.id.first == vB.id.first:
					winner = ve
					break
				feA = self.getCurrentEdge().first_fedge
				feB = ve.last_fedge
				vB = feB.second_svertex
				vA = feA.first_svertex
				if vA.id.first == vB.id.first:
					winner = ve
					break
				feA = self.getCurrentEdge().last_fedge
				feB = ve.last_fedge
				vB = feB.second_svertex
				vA = feA.second_svertex
				if vA.id.first == vB.id.first:
					winner = ve
					break
				feA = self.getCurrentEdge().first_fedge
				feB = ve.first_fedge
				vB = feB.first_svertex
				vA = feA.first_svertex
				if vA.id.first == vB.id.first:
					winner = ve
					break
				it.increment()
		else:
			## case of NonTVertex
			natures = [Nature.SILHOUETTE,Nature.BORDER,Nature.CREASE,Nature.SUGGESTIVE_CONTOUR,Nature.VALLEY,Nature.RIDGE]
			for i in range(len(natures)):
				currentNature = self.getCurrentEdge().nature
				if (natures[i] & currentNature) != 0:
					count=0
					while not it.isEnd():
						visitNext = 0
						oNature = it.getObject().nature
						if (oNature & natures[i]) != 0:
							if natures[i] != oNature:
								for j in range(i):
									if (natures[j] & oNature) != 0:
										visitNext = 1
										break
								if visitNext != 0:
									break	 
							count = count+1
							winner = it.getObject()
						it.increment()
					if count != 1:
						winner = None
					break
		return winner

