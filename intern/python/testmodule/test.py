#! /usr/bin/env python

# CVS
# $Author$
# $Date$
# $RCSfile$
# $Revision$

import testmodule

tc = testmodule.TestClass()

print tc.memberVariable
tc.memberVariable = 1
print tc.memberVariable
tc.memberFunction()
