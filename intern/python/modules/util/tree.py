# Basisklasse fuer Baumstruktur
# Object-orientiertes Programmieren Wi/97
#
# (c) Martin Strubel, Fakultaet fuer Physik, Universitaet Konstanz
# (strubi@gandalf.physik.uni-konstanz.de)

# updated 08.2001

"""Simple binary tree module

	This module demonstrates a binary tree class.

	Example::

		a = [5, 8, 8, 3, 7, 9]
		t1 = Tree()
		t1.fromList(a)

	Operations on tree nodes are done by writing a simple operator class::
		
		class myOp:
			def __init__(self):
				...
			def operate(self, node):
				do_something(node)

	and calling the recursive application::

		op = MyOp()
		t1.recurse(op)

	Objects inserted into the tree can be of any kind, as long as they define a
	comparison operation.
"""	

def recurse(node, do):
	if node == None:
		return
	recurse(node.left, do)
	do(node)
	recurse(node.right, do)

class Nullnode:
	def __init__(self):
		self.left = None
		self.right = None
		self.depth = 0

	def recurse(self, do):
		if self == Nil:
			return
		self.left.recurse(do)
		do(self)
		self.right.recurse(do)

Nil = Nullnode()

def nothing(x):
	return x
			
class Node(Nullnode):
	def __init__(self, data = None):
		self.left = Nil
		self.right = Nil
		self.data = data
		self.depth = 0

	def __repr__(self):
		return "Node: %s" % self.data
		
	def insert(self, node):
		if node.data < self.data:
			if self.left != Nil: 
				return self.left.insert(node)
			else:
				node.depth = self.depth + 1
				self.left = node
			#	print "inserted left"
				return self

		elif node.data > self.data:
			if self.right != Nil:
				return self.right.insert(node)
			else:
				node.depth = self.depth + 1
				self.right = node
			#	print "inserted right"
				return self
		else: 
			return self.insert_equal(node)

	def find(self, node, do = nothing):
		if node.data < self.data:
			if self.left != Nil: 
				return self.left.find(node, do)
			else:
				return self
		elif node.data > self.data:
			if self.right != Nil:
				return self.right.find(node, do)
			else:
				return self
		else: 
			return do(self)

	def remove(self, node):
		newpar 
		return self	
	def insert_equal(self, node):
		#print "insert:",
		self.equal(node)
		return self
	def found_equal(self, node):
		self.equal(node)
	def equal(self, node):
		# handle special
		print "node (%s) is equal self (%s)" % (node, self)
	def copy(self):
		n = Node(self.data)
		return n

	def recursecopy(self):
		n = Node()
		n.data = self.data
		n.flag = self.flag
		if self.left != Nil:
			n.left = self.left.recursecopy()
		if self.right != Nil:
			n.right = self.right.recursecopy()

		return n

class NodeOp:
	def __init__(self):
		self.list = []
	def copy(self, node):
		self.list.append(node.data)

class Tree:
	def __init__(self, root = None):
		self.root = root
		self.n = 0
	def __radd__(self, other):
		print other
		t = self.copy()
		t.merge(other)
		return t
	def __repr__(self):
		return "Tree with %d elements" % self.n
	def insert(self, node):
		if self.root == None:
			self.root = node
		else:
			self.root.insert(node)
		self.n += 1	
	def recurse(self, do):
		if self.root == None:
			return
		self.root.recurse(do)
	def find(self, node):
		return self.root.find(node)
	def remove(self, node):
		self.root.remove(node)
	def copy(self):
		"make true copy of self"
		t = newTree()
		c = NodeOp()
		self.recurse(c.copy)
		t.fromList(c.list)
		return t
	def asList(self):
		c = NodeOp()
		self.recurse(c.copy)
		return c.list
	def fromList(self, list):
		for item in list:
			n = Node(item)
			self.insert(n)
	def insertcopy(self, node):
		n = node.copy()
		self.insert(n)
	def merge(self, other):
		other.recurse(self.insertcopy)
# EXAMPLE:

newTree = Tree

def printnode(x):
	print "Element: %s, depth: %s" % (x, x.depth)

def test():
	a = [5, 8, 8, 3, 7, 9]
	t1 = Tree()
	t1.fromList(a)

	b = [12, 4, 56, 7, 34]
	t2 = Tree()
	t2.fromList(b)

	print "tree1:"
	print t1.asList()
	print "tree2:"
	print t2.asList()
	print '-----'
	print "Trees can be added:"

	
	t3 = t1 + t2
	print t3.asList()
	print "..or alternatively merged:"
	t1.merge(t2)
	print t1.asList()

if __name__ == '__main__':
	test()
