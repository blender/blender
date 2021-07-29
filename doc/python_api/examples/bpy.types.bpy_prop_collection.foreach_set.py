"""
Only works for 'basic type' properties (bool, int and float)!
seq must be uni-dimensional, multi-dimensional arrays (like array of vectors) will be re-created from it.
"""

collection.foreach_set(attr, some_seq)

# Python equivalent
for i in range(len(some_seq)):
    setattr(collection[i], attr, some_seq[i])
