# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# ./blender.bin --background -noaudio --python tests/python/bl_pyapi_bpy_driver_secure_eval.py -- --verbose
import bpy
import unittest
import builtins


# -----------------------------------------------------------------------------
# Mock Environment


expect_unreachable_msg = "This function should _NEVER_ run!"
# Internal check, to ensure this actually runs as expected.
expect_unreachable_count = 0


def expect_os_unreachable():
    global expect_unreachable_count
    expect_unreachable_count += 1
    raise Exception(expect_unreachable_msg)


__import__("os").expect_os_unreachable = expect_os_unreachable


expect_open_unreachable_count = 0


def open_expect_unreachable(*args, **kwargs):
    global expect_open_unreachable_count
    expect_open_unreachable_count += 1
    raise Exception(expect_unreachable_msg)


mock_builtins = {**builtins.__dict__, **{"open": open_expect_unreachable}}


# -----------------------------------------------------------------------------
# Utility Functions


def is_expression_secure(expr_str, verbose):
    """
    Return (ok, code) where ok is true if expr_str is considered secure.
    """
    # Internal function only for testing (not part of the public API).
    from _bpy import _driver_secure_code_test
    expr_code = compile(expr_str, "<is_expression_secure>", 'eval')
    ok = _driver_secure_code_test(expr_code, verbose=verbose)
    return ok, expr_code


# -----------------------------------------------------------------------------
# Tests (Accept)


class _TestExprMixIn:
    """
    Sub-classes must define:
    - expressions_expect_secure: boolean, the expected secure state.
    - expressions: A sequence of expressions that must evaluate in the driver name-space.

    Optionally:
    - expressions_expect_unreachable:
      A boolean, when true, it's expected each expression should call
    ``expect_os_unreachable`` or ``expect_open_unreachable``.
    """

    # Sub-class may override.
    expressions_expect_unreachable = False

    def assertSecure(self, expect_secure, expr_str):
        is_secure, expr_code = is_expression_secure(
            expr_str,
            # Only verbose when secure as this is will result in an failure,
            # in that case it's useful to know which op-codes caused the test to unexpectedly fail.
            verbose=expect_secure,
        )
        if is_secure != expect_secure:
            raise self.failureException(
                "Expression \"%s\" was expected to be %s" %
                (expr_str, "secure" if expect_secure else "insecure"))
        # NOTE: executing is not essential, it's just better to ensure the expressions make sense.
        try:
            exec(
                expr_code,
                {"__builtins__": mock_builtins},
                {**bpy.app.driver_namespace, **{"__builtins__": mock_builtins}},
            )
            # exec(expr_code, {}, bpy.app.driver_namespace)
            ex = None
        except BaseException as ex_test:
            ex = ex_test

        if self.expressions_expect_unreachable:
            if ex and ex.args == (expect_unreachable_msg,):
                ex = None
            elif not ex:
                raise self.failureException("Expression \"%s\" failed to run `os.expect_os_unreachable`" % (expr_str,))
            else:
                # An unknown exception was raised, use the exception below.
                pass

        if ex:
            raise self.failureException("Expression \"%s\" failed to evaluate with error: %r" % (expr_str, ex))

    def test_expr(self):
        expect_secure = self.expressions_expect_secure
        for expr_str in self.expressions:
            self.assertSecure(expect_secure, expr_str)


class TestExprMixIn_Accept(_TestExprMixIn):
    expressions_expect_secure = True


class TestExprMixIn_Reject(_TestExprMixIn):
    expressions_expect_secure = False


class TestAcceptLiteralNumbers(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("1", "1_1", "1.1", "1j", "0x1", "0o1", "0b1")


class TestAcceptLiteralStrings(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("''", "'_'", "r''", "r'_'", "'''_'''")


class TestAcceptSequencesEmpty(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("()", "[]", "{}", "[[]]", "(())")


class TestAcceptSequencesSimple(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("('', '')", "['', '_']", "{'', '_'}", "{'': '_'}")


class TestAcceptSequencesExpand(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("(*('', '_'),)", "[*(), *[]]", "{*{1, 2}}")


class TestAcceptSequencesComplex(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("[1, 2, 3][-1:0:-1][0]", "1 in (1, 2)", "False if 1 in {1, 2} else True")


class TestAcceptMathOperators(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("4 / 4", "4 * 4", "4 // 4", "2 ** 2", "4 ^ -1", "4 & 1", "4 % 1")


class TestAcceptMathFunctionsSimple(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("sin(pi)", "degrees(pi / 2)", "clamp(4, 0, 1)")


class TestAcceptMathFunctionsComplex(unittest.TestCase, TestExprMixIn_Accept):
    expressions = ("-(sin(pi) ** 2) / 2", "floor(22 / 7)", "ceil(pi + 1)")


# -----------------------------------------------------------------------------
# Tests (Reject)

class TestRejectLiteralFStrings(unittest.TestCase, TestExprMixIn_Reject):
    # F-String's are not supported as `BUILD_STRING` op-code is disabled,
    # while it may be safe to enable that needs to be double-checked.
    # Further it doesn't seem useful for typical math expressions used in drivers.
    expressions = ("f''", "f'{1}'", "f'{\"_\"}'")


class TestRejectModuleAccess(unittest.TestCase, TestExprMixIn_Reject):
    # Each of these commands _must_ run `expect_os_unreachable`,
    # and must also be rejected as insecure - otherwise we have problems.
    expressions_expect_unreachable = True
    expressions = (
        "__import__('os').expect_os_unreachable()",
        "exec(\"__import__('os').expect_os_unreachable()\")",
        "(globals().update(__import__('os').__dict__), expect_os_unreachable())",
        "__builtins__['getattr'](__builtins__['__import__']('os'), 'expect_os_unreachable')()",
    )

    # Ensure the functions are actually called.
    def setUp(self):
        self._count = expect_unreachable_count

    def tearDown(self):
        count_actual = expect_unreachable_count - self._count
        count_expect = len(self.expressions)
        if count_actual != count_expect:
            raise Exception(
                "Expected 'os.expect_os_unreachable' to be called %d times but was called %d times" %
                (count_expect, count_actual),
            )


class TestRejectOpenAccess(unittest.TestCase, TestExprMixIn_Reject):
    # Each of these commands _must_ run `expect_open_unreachable`,
    # and must also be rejected as insecure - otherwise we have problems.
    expressions_expect_unreachable = True
    expressions = (
        "open('file.txt', 'r')",
        "exec(\"open('file.txt', 'r')\")",
        "(globals().update({'fake_open': __builtins__['open']}), fake_open())",
    )

    # Ensure the functions are actually called.
    def setUp(self):
        self._count = expect_open_unreachable_count

    def tearDown(self):
        count_actual = expect_open_unreachable_count - self._count
        count_expect = len(self.expressions)
        if count_actual != count_expect:
            raise Exception(
                "Expected 'open' to be called %d times but was called %d times" %
                (count_expect, count_actual),
            )


if __name__ == '__main__':
    import sys
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    unittest.main()
