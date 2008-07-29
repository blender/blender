from freestyle_init import *

class AndUP1D(ContourUP1D):
    def __init__(self, pred1, pred2):
        ContourUP1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2

    def getName(self):
        return "AndUP1D"

    def __call__(self, inter):
        return self.__pred1(inter) and self.__pred2(inter)
    
class OrUP1D(ContourUP1D):
    def __init__(self, pred1, pred2):
        ContourUP1D.__init__(self)
        self.__pred1 = pred1
        self.__pred2 = pred2

    def getName(self):
        return "OrUP1D"

    def __call__(self, inter):
        return self.__pred1(inter) or self.__pred2(inter)

class NotUP1D(ContourUP1D):
	def __init__(self, pred):
		ContourUP1D.__init__(self)
		self.__pred = pred

	def getName(self):
 		return "NotUP1D"

	def __call__(self, inter):
		return self.__pred(inter) == 0
