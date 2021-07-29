import sys
from .. problems import InvalidSyntax

cache = {}

def compileScript(script, name = "<string>"):
    # clear the cache once in a while
    if len(cache) == 500:
        cache.clear()

    try:
        scriptHash = hash(script)
        if scriptHash not in cache:
            compiledCode = compile(script, name, "exec")
            cache[scriptHash] = compiledCode

        return cache[scriptHash]

    except SyntaxError:
        lines = script.split("\n")
        lineNumber = sys.exc_info()[1].lineno
        lineNumberWidth = len(str(len(lines)))

        print("\n"*5)
        for i, line in enumerate(lines):
            linePrefix = str(i + 1).rjust(lineNumberWidth) + ". "
            linesSuffix = "        <-------------- Error happens here" if lineNumber == i + 1 else ""
            print(linePrefix + line + linesSuffix)
        print("\n"*5)

        InvalidSyntax().report()
