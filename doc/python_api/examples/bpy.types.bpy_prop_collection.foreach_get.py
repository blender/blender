"""
Only works for 'basic type' properties (bool, int and float)!
Multi-dimensional arrays (like array of vectors) will be flattened into seq.
"""

collection.foreach_get(attr, some_seq)

# Python equivalent
for i in range(len(seq)):
    some_seq[i] = getattr(collection[i], attr)

