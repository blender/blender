import re
import ast
import sys

def isCodeValid(code):
    return getSyntaxError(code) is None

def getSyntaxError(code):
    try:
        ast.parse(code)
        return None
    except SyntaxError as e:
        return e

def containsStarImport(code):
    match = re.search("import\s*\*", code)
    return match is not None
