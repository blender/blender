# -*- coding: utf-8 -*-
#
# ***** BEGIN GPL LICENSE BLOCK *****
#
# --------------------------------------------------------------------------
# Blender 2.5 Extensions Framework
# --------------------------------------------------------------------------
#
# Authors:
# Doug Hammond
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#
# ***** END GPL LICENCE BLOCK *****
#
"""
Pure logic and validation class.

By using a Subject object, and a dict of described logic tests, it
is possible to arrive at a True or False result for various purposes:
1. Data validation
2. UI control visibility

A Subject can be any object whose members are readable with getattr() :
class Subject(object):
    a = 0
    b = 1
    c = 'foo'
    d = True
    e = False
    f = 8
    g = 'bar'


Tests are described thus:

Use the special list types Logic_AND and Logic_OR to describe
combinations of values and other members. Use Logic_Operator for
numerical comparison.

With regards to Subject, each of these evaluate to True:
TESTA = {
    'a': 0,
    'c': Logic_OR([ 'foo', 'bar' ]),
    'd': Logic_AND([True, True]),
    'f': Logic_AND([8, {'b': 1}]),
    'e': {'b': Logic_Operator({'gte':1, 'lt':3}) },
    'g': Logic_OR([ 'baz', Logic_AND([{'b': 1}, {'f': 8}]) ])
}

With regards to Subject, each of these evaluate to False:
TESTB = {
    'a': 'foo',
    'c': Logic_OR([ 'bar', 'baz' ]),
    'd': Logic_AND([ True, 'foo' ]),
    'f': Logic_AND([9, {'b': 1}]),
    'e': {'b': Logic_Operator({'gte':-10, 'lt': 1}) },
    'g': Logic_OR([ 'baz', Logic_AND([{'b':0}, {'f': 8}]) ])
}

With regards to Subject, this test is invalid
TESTC = {
    'n': 0
}

Tests are executed thus:
S = Subject()
L = Logician(S)
L.execute(TESTA)

"""

class Logic_AND(list):
    pass
class Logic_OR(list):
    pass
class Logic_Operator(dict):
    pass

class Logician(object):
    """Given a subject and a dict that describes tests to perform on
    its members, this class will evaluate True or False results for
    each member/test pair. See the examples below for test syntax.

    """

    subject = None
    def __init__(self, subject):
        self.subject = subject

    def get_member(self, member_name):
        """Get a member value from the subject object. Raise exception
        if subject is None or member not found.

        """
        if self.subject is None:
            raise Exception('Cannot run tests on a subject which is None')

        return getattr(self.subject, member_name)

    def test_logic(self, member, logic, operator='eq'):
        """Find the type of test to run on member, and perform that test"""

        if type(logic) is dict:
            return self.test_dict(member, logic)
        elif type(logic) is Logic_AND:
            return self.test_and(member, logic)
        elif type(logic) is Logic_OR:
            return self.test_or(member, logic)
        elif type(logic) is Logic_Operator:
            return self.test_operator(member, logic)
        else:
            # compare the value, I think using Logic_Operator() here
            # allows completeness in test_operator(), but I can't put
            # my finger on why for the minute
            return self.test_operator(member,
                Logic_Operator({operator: logic}))

    def test_operator(self, member, value):
        """Execute the operators contained within value and expect that
        ALL operators are True

        """

        # something in this method is incomplete, what if operand is
        # a dict, Logic_AND, Logic_OR or another Logic_Operator ?
        # Do those constructs even make any sense ?

        result = True
        for operator, operand in value.items():
            operator = operator.lower().strip()
            if operator in ['eq', '==']:
                result &= member==operand
            if operator in ['not', '!=']:
                result &= member!=operand
            if operator in ['lt', '<']:
                result &= member<operand
            if operator in ['lte', '<=']:
                result &= member<=operand
            if operator in ['gt', '>']:
                result &= member>operand
            if operator in ['gte', '>=']:
                result &= member>=operand
            if operator in ['and', '&']:
                result &= member&operand
            if operator in ['or', '|']:
                result &= member|operand
            if operator in ['len']:
                result &= len(member)==operand
            # I can think of some more, but they're probably not useful.

        return result

    def test_or(self, member, logic):
        """Member is a value, logic is a set of values, ANY of which
        can be True

        """
        result = False
        for test in logic:
            result |= self.test_logic(member, test)

        return result

    def test_and(self, member, logic):
        """Member is a value, logic is a list of values, ALL of which
        must be True

        """
        result = True
        for test in logic:
            result &= self.test_logic(member, test)

        return result

    def test_dict(self, member, logic):
        """Member is a value, logic is a dict of other members to
        compare to. All other member tests must be True

        """
        result = True
        for other_member, test in logic.items():
            result &= self.test_logic(self.get_member(other_member), test)

        return result

    def execute(self, test):
        """Subject is an object, test is a dict of {member: test} pairs
        to perform on subject's members. Wach key in test is a member
        of subject.

        """

        for member_name, logic in test.items():
            result = self.test_logic(self.get_member(member_name), logic)
            print('member %s is %s' % (member_name, result))

# A couple of name aliases
class Validation(Logician):
    pass
class Visibility(Logician):
    pass
