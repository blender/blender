# This is called by cmake as an external process from
# ./source/creator/CMakeLists.txt to write ./source/creator/buildinfo.h
# Caller must define:
#   SOURCE_DIR
# Optional overrides:
#   BUILD_DATE
#   BUILD_TIME

# Extract working copy information for SOURCE_DIR into MY_XXX variables
# with a default in case anything fails, for example when using git-svn
set(MY_WC_HASH "unknown")
set(MY_WC_BRANCH "unknown")
set(MY_WC_COMMIT_TIMESTAMP 0)

# Guess if this is a git working copy and then look up the revision
if(EXISTS ${SOURCE_DIR}/.git)
	execute_process(COMMAND git rev-parse --abbrev-ref HEAD
	                WORKING_DIRECTORY ${SOURCE_DIR}
	                OUTPUT_VARIABLE MY_WC_BRANCH
	                OUTPUT_STRIP_TRAILING_WHITESPACE)

	if(MY_WC_BRANCH STREQUAL "HEAD")
		# Detached HEAD, check whether commit hash is reachable
		# in the master branch
		execute_process(COMMAND git rev-parse --short HEAD
		                WORKING_DIRECTORY ${SOURCE_DIR}
		                OUTPUT_VARIABLE MY_WC_HASH
		                OUTPUT_STRIP_TRAILING_WHITESPACE)

		execute_process(COMMAND git branch --list master blender-v* --contains ${MY_WC_HASH}
		                WORKING_DIRECTORY ${SOURCE_DIR}
		                OUTPUT_VARIABLE _git_contains_check
		                OUTPUT_STRIP_TRAILING_WHITESPACE)

		if(NOT _git_contains_check STREQUAL "")
			set(MY_WC_BRANCH "master")
		else()
			execute_process(COMMAND git show-ref --tags -d
			                WORKING_DIRECTORY ${SOURCE_DIR}
			                OUTPUT_VARIABLE _git_tag_hashes
			                OUTPUT_STRIP_TRAILING_WHITESPACE)

			execute_process(COMMAND git rev-parse HEAD
			                WORKING_DIRECTORY ${SOURCE_DIR}
			                OUTPUT_VARIABLE _git_head_hash
			                OUTPUT_STRIP_TRAILING_WHITESPACE)

			if(_git_tag_hashes MATCHES "${_git_head_hash}")
				set(MY_WC_BRANCH "master")
			else()
				execute_process(COMMAND git branch --contains ${MY_WC_HASH}
				                WORKING_DIRECTORY ${SOURCE_DIR}
				                OUTPUT_VARIABLE _git_contains_branches
				                OUTPUT_STRIP_TRAILING_WHITESPACE)
				string(REGEX REPLACE "^\\*[ \t]+" "" _git_contains_branches "${_git_contains_branches}")
				string(REGEX REPLACE "[\r\n]+" ";" _git_contains_branches "${_git_contains_branches}")
				string(REGEX REPLACE ";[ \t]+" ";" _git_contains_branches "${_git_contains_branches}")
				foreach(_branch ${_git_contains_branches})
					if(NOT "${_branch}" MATCHES "\\(HEAD.*")
						set(MY_WC_BRANCH "${_branch}")
						break()
					endif()
				endforeach()
				unset(_branch)
				unset(_git_contains_branches)
			endif()

			unset(_git_tag_hashes)
			unset(_git_head_hashs)
		endif()


		unset(_git_contains_check)
	else()
		execute_process(COMMAND git log HEAD..@{u}
		                WORKING_DIRECTORY ${SOURCE_DIR}
		                OUTPUT_VARIABLE _git_below_check
		                OUTPUT_STRIP_TRAILING_WHITESPACE
		                ERROR_QUIET)
		if(NOT _git_below_check STREQUAL "")
			# If there're commits between HEAD and upstream this means
			# that we're reset-ed to older revision. Use it's hash then.
			execute_process(COMMAND git rev-parse --short HEAD
			                WORKING_DIRECTORY ${SOURCE_DIR}
			                OUTPUT_VARIABLE MY_WC_HASH
			                OUTPUT_STRIP_TRAILING_WHITESPACE)
		else()
			execute_process(COMMAND git rev-parse --short @{u}
			                WORKING_DIRECTORY ${SOURCE_DIR}
			                OUTPUT_VARIABLE MY_WC_HASH
			                OUTPUT_STRIP_TRAILING_WHITESPACE
			                ERROR_QUIET)

			if(MY_WC_HASH STREQUAL "")
				# Local branch, not set to upstream.
				# Well, let's use HEAD for now
				execute_process(COMMAND git rev-parse --short HEAD
				                WORKING_DIRECTORY ${SOURCE_DIR}
				                OUTPUT_VARIABLE MY_WC_HASH
				                OUTPUT_STRIP_TRAILING_WHITESPACE)
			endif()
		endif()

		if(MY_WC_BRANCH MATCHES "^blender-v")
			set(MY_WC_BRANCH "master")
		endif()

		unset(_git_below_check)
	endif()

	execute_process(COMMAND git log -1 --format=%ct
	                WORKING_DIRECTORY ${SOURCE_DIR}
	                OUTPUT_VARIABLE MY_WC_COMMIT_TIMESTAMP
	                OUTPUT_STRIP_TRAILING_WHITESPACE)
	# May fail in rare cases
	if(MY_WC_COMMIT_TIMESTAMP STREQUAL "")
		set(MY_WC_COMMIT_TIMESTAMP 0)
	endif()

	# Update GIT index before getting dirty files
	execute_process(COMMAND git update-index -q --refresh
	                WORKING_DIRECTORY ${SOURCE_DIR}
	                OUTPUT_STRIP_TRAILING_WHITESPACE)

	execute_process(COMMAND git diff-index --name-only HEAD --
	                WORKING_DIRECTORY ${SOURCE_DIR}
	                OUTPUT_VARIABLE _git_changed_files
	                OUTPUT_STRIP_TRAILING_WHITESPACE)

	if(NOT _git_changed_files STREQUAL "")
		set(MY_WC_BRANCH "${MY_WC_BRANCH} (modified)")
	else()
		# Unpushed commits are also considered local modifications
		execute_process(COMMAND git log @{u}..
		                WORKING_DIRECTORY ${SOURCE_DIR}
		                OUTPUT_VARIABLE _git_unpushed_log
		                OUTPUT_STRIP_TRAILING_WHITESPACE
		                ERROR_QUIET)
		if(NOT _git_unpushed_log STREQUAL "")
			set(MY_WC_BRANCH "${MY_WC_BRANCH} (modified)")
		endif()
		unset(_git_unpushed_log)
	endif()

	unset(_git_changed_files)
endif()

# BUILD_PLATFORM and BUILD_PLATFORM are taken from CMake
# but BUILD_DATE and BUILD_TIME are platform dependent
if(UNIX)
	if(NOT BUILD_DATE)
		execute_process(COMMAND date "+%Y-%m-%d" OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
	if(NOT BUILD_TIME)
		execute_process(COMMAND date "+%H:%M:%S" OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
elseif(WIN32)
	if(NOT BUILD_DATE)
		execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
	if(NOT BUILD_TIME)
		execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
endif()

# Write a file with the BUILD_HASH define
file(WRITE buildinfo.h.txt
	"#define BUILD_HASH \"${MY_WC_HASH}\"\n"
	"#define BUILD_COMMIT_TIMESTAMP ${MY_WC_COMMIT_TIMESTAMP}\n"
	"#define BUILD_BRANCH \"${MY_WC_BRANCH}\"\n"
	"#define BUILD_DATE \"${BUILD_DATE}\"\n"
	"#define BUILD_TIME \"${BUILD_TIME}\"\n"
)

# cleanup
unset(MY_WC_HASH)
unset(MY_WC_COMMIT_TIMESTAMP)
unset(MY_WC_BRANCH)
unset(BUILD_DATE)
unset(BUILD_TIME)


# Copy the file to the final header only if the version changes
# and avoid needless rebuilds
# TODO: verify this comment is true, as BUILD_TIME probably changes
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        buildinfo.h.txt buildinfo.h)
