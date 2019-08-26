INCLUDE(FindPackageHandleStandardArgs)

FIND_PROGRAM(GHOSTSCRIPT_EXECUTABLE
  NAMES gs gswin32c
  PATHS "$ENV{ProgramFiles}/gs"
  PATH_SUFFIXES gs8.61/bin gs8.62/bin gs8.63/bin gs8.64/bin gs8.65/bin
  DOC "Ghostscript: PostScript and PDF language interpreter and previewer."
)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Ghostscript DEFAULT_MSG GHOSTSCRIPT_EXECUTABLE)
