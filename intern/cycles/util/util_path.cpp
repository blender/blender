/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util/util_md5.h"
#include "util/util_path.h"
#include "util/util_string.h"

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/strutil.h>
#include <OpenImageIO/sysutil.h>

OIIO_NAMESPACE_USING

#include <stdio.h>

#include <sys/stat.h>

#if defined(_WIN32)
#  define DIR_SEP '\\'
#  define DIR_SEP_ALT '/'
#  include <direct.h>
#else
#  define DIR_SEP '/'
#  include <dirent.h>
#  include <pwd.h>
#  include <unistd.h>
#  include <sys/types.h>
#endif

#ifdef HAVE_SHLWAPI_H
#  include <shlwapi.h>
#endif

#include "util/util_map.h"
#include "util/util_windows.h"

CCL_NAMESPACE_BEGIN

#ifdef _WIN32
#  if defined(_MSC_VER) || defined(__MINGW64__)
typedef struct _stat64 path_stat_t;
#  elif defined(__MINGW32__)
typedef struct _stati64 path_stat_t;
#  else
typedef struct _stat path_stat_t;
#  endif
#  ifndef S_ISDIR
#    define S_ISDIR(x) (((x) & _S_IFDIR) == _S_IFDIR)
#  endif
#else
typedef struct stat path_stat_t;
#endif

static string cached_path = "";
static string cached_user_path = "";
static string cached_xdg_cache_path = "";

namespace {

#ifdef _WIN32
class directory_iterator {
public:
	class path_info {
	public:
		path_info(const string& path,
		          const WIN32_FIND_DATAW& find_data)
		: path_(path),
		  find_data_(find_data)
		{
		}

		string path() {
			return path_join(path_, string_from_wstring(find_data_.cFileName));
		}
	protected:
		const string& path_;
		const WIN32_FIND_DATAW& find_data_;
	};

	directory_iterator()
	: path_info_("", find_data_),
	  h_find_(INVALID_HANDLE_VALUE)
	{
	}

	explicit directory_iterator(const string& path)
	: path_(path),
	  path_info_(path, find_data_)
	{
		string wildcard = path;
		if(wildcard[wildcard.size() - 1] != DIR_SEP) {
			wildcard += DIR_SEP;
		}
		wildcard += "*";
		h_find_ = FindFirstFileW(string_to_wstring(wildcard).c_str(),
		                         &find_data_);
		if(h_find_ != INVALID_HANDLE_VALUE) {
			skip_dots();
		}
	}

	~directory_iterator()
	{
		if(h_find_ != INVALID_HANDLE_VALUE) {
			FindClose(h_find_);
		}
	}

	directory_iterator& operator++()
	{
		step();
		return *this;
	}

	path_info* operator-> ()
	{
		return &path_info_;
	}

	bool operator!=(const directory_iterator& other)
	{
		return h_find_ != other.h_find_;
	}

protected:
	bool step()
	{
		if(do_step()) {
			return skip_dots();
		}
		return false;
	}

	bool do_step()
	{
		if(h_find_ != INVALID_HANDLE_VALUE) {
			bool result = FindNextFileW(h_find_, &find_data_) == TRUE;
			if(!result) {
				FindClose(h_find_);
				h_find_ = INVALID_HANDLE_VALUE;
			}
			return result;
		}
		return false;
	}

	bool skip_dots()
	{
		while(wcscmp(find_data_.cFileName, L".") == 0 ||
		      wcscmp(find_data_.cFileName, L"..") == 0)
		{
			if(!do_step()) {
				return false;
			}
		}
		return true;
	}

	string path_;
	path_info path_info_;
	WIN32_FIND_DATAW find_data_;
	HANDLE h_find_;
};
#else  /* _WIN32 */

class directory_iterator {
public:
	class path_info {
	public:
		explicit path_info(const string& path)
		: path_(path),
		  entry_(NULL)
		{
		}

		string path() {
			return path_join(path_, entry_->d_name);
		}

		void current_entry_set(const struct dirent *entry)
		{
			entry_ = entry;
		}
	protected:
		const string& path_;
		const struct dirent *entry_;
	};

	directory_iterator()
	: path_info_(""),
	  name_list_(NULL),
	  num_entries_(-1),
	  cur_entry_(-1)
	{
	}

	explicit directory_iterator(const string& path)
	: path_(path),
	  path_info_(path_),
	  cur_entry_(0)
	{
		num_entries_ = scandir(path.c_str(),
		                       &name_list_,
		                       NULL,
		                       alphasort);
		if(num_entries_ < 0) {
			perror("scandir");
		}
		else {
			skip_dots();
		}
	}

	~directory_iterator()
	{
		destroy_name_list();
	}

	directory_iterator& operator++()
	{
		step();
		return *this;
	}

	path_info* operator-> ()
	{
		path_info_.current_entry_set(name_list_[cur_entry_]);
		return &path_info_;
	}

	bool operator!=(const directory_iterator& other)
	{
		return name_list_ != other.name_list_;
	}

protected:
	bool step()
	{
		if(do_step()) {
			return skip_dots();
		}
		return false;
	}

	bool do_step()
	{
		++cur_entry_;
		if(cur_entry_ >= num_entries_) {
			destroy_name_list();
			return false;
		}
		return true;
	}

	/* Skip . and .. folders. */
	bool skip_dots()
	{
		while(strcmp(name_list_[cur_entry_]->d_name, ".") == 0 ||
		      strcmp(name_list_[cur_entry_]->d_name, "..") == 0)
		{
			if(!step()) {
				return false;
			}
		}
		return true;
	}

	void destroy_name_list()
	{
		if(name_list_ == NULL) {
			return;
		}
		for(int i = 0; i < num_entries_; ++i) {
			free(name_list_[i]);
		}
		free(name_list_);
		name_list_ = NULL;
	}

	string path_;
	path_info path_info_;
	struct dirent **name_list_;
	int num_entries_, cur_entry_;
};

#endif  /* _WIN32 */

size_t find_last_slash(const string& path)
{
	for(size_t i = 0; i < path.size(); ++i) {
		size_t index = path.size() - 1 - i;
#ifdef _WIN32
		if(path[index] == DIR_SEP || path[index] == DIR_SEP_ALT)
#else
		if(path[index] == DIR_SEP)
#endif
		{
			return index;
		}
	}
	return string::npos;
}

}  /* namespace */

static char *path_specials(const string& sub)
{
	static bool env_init = false;
	static char *env_shader_path;
	static char *env_source_path;
	if(!env_init) {
		env_shader_path = getenv("CYCLES_SHADER_PATH");
		/* NOTE: It is KERNEL in env variable for compatibility reasons. */
		env_source_path = getenv("CYCLES_KERNEL_PATH");
		env_init = true;
	}
	if(env_shader_path != NULL && sub == "shader") {
		return env_shader_path;
	}
	else if(env_shader_path != NULL && sub == "source") {
		return env_source_path;
	}
	return NULL;
}

#if defined(__linux__) || defined(__APPLE__)
static string path_xdg_cache_get()
{
	const char *home = getenv("XDG_CACHE_HOME");
	if(home) {
		return string(home);
	}
	else {
		home = getenv("HOME");
		if(home == NULL) {
			home = getpwuid(getuid())->pw_dir;
		}
		return path_join(string(home), ".cache");
	}
}
#endif

void path_init(const string& path, const string& user_path)
{
	cached_path = path;
	cached_user_path = user_path;

#ifdef _MSC_VER
	// workaround for https://svn.boost.org/trac/boost/ticket/6320
	// indirectly init boost codec here since it's not thread safe, and can
	// cause crashes when it happens in multithreaded image load
	OIIO::Filesystem::exists(path);
#endif
}

string path_get(const string& sub)
{
	char *special = path_specials(sub);
	if(special != NULL)
		return special;

	if(cached_path == "")
		cached_path = path_dirname(Sysutil::this_program_path());

	return path_join(cached_path, sub);
}

string path_user_get(const string& sub)
{
	if(cached_user_path == "")
		cached_user_path = path_dirname(Sysutil::this_program_path());

	return path_join(cached_user_path, sub);
}

string path_cache_get(const string& sub)
{
#if defined(__linux__) || defined(__APPLE__)
	if(cached_xdg_cache_path == "") {
		cached_xdg_cache_path = path_xdg_cache_get();
	}
	string result = path_join(cached_xdg_cache_path, "cycles");
	return path_join(result, sub);
#else
	/* TODO(sergey): What that should be on Windows? */
	return path_user_get(path_join("cache", sub));
#endif
}

#if defined(__linux__) || defined(__APPLE__)
string path_xdg_home_get(const string& sub = "");
#endif

string path_filename(const string& path)
{
	size_t index = find_last_slash(path);
	if(index != string::npos) {
		/* Corner cases to match boost behavior. */
#ifndef _WIN32
		if(index == 0 && path.size() == 1) {
			return path;
		}
#endif
		if(index == path.size() - 1) {
#ifdef _WIN32
			if(index == 2) {
				return string(1, DIR_SEP);
			}
#endif
			return ".";
		}
		return path.substr(index + 1, path.size() - index - 1);
	}
	return path;
}

string path_dirname(const string& path)
{
	size_t index = find_last_slash(path);
	if(index != string::npos) {
#ifndef _WIN32
		if(index == 0 && path.size() > 1) {
			return string(1, DIR_SEP);
		}
#endif
		return path.substr(0, index);
	}
	return "";
}

string path_join(const string& dir, const string& file)
{
	if(dir.size() == 0) {
		return file;
	}
	if(file.size() == 0) {
		return dir;
	}
	string result = dir;
#ifndef _WIN32
	if(result[result.size() - 1] != DIR_SEP &&
	   file[0] != DIR_SEP)
#else
	if(result[result.size() - 1] != DIR_SEP &&
	   result[result.size() - 1] != DIR_SEP_ALT &&
	   file[0] != DIR_SEP &&
	   file[0] != DIR_SEP_ALT)
#endif
	{
		result += DIR_SEP;
	}
	result += file;
	return result;
}

string path_escape(const string& path)
{
	string result = path;
	string_replace(result, " ", "\\ ");
	return result;
}

bool path_is_relative(const string& path)
{
#ifdef _WIN32
#  ifdef HAVE_SHLWAPI_H
	return PathIsRelative(path.c_str());
#  else  /* HAVE_SHLWAPI_H */
	if(path.size() >= 3) {
		return !(((path[0] >= 'a' && path[0] <= 'z') ||
		         (path[0] >= 'A' && path[0] <= 'Z')) &&
		         path[1] == ':' && path[2] == DIR_SEP);
	}
	return true;
#  endif  /* HAVE_SHLWAPI_H */
#else  /* _WIN32 */
	if(path.size() == 0) {
		return 1;
	}
	return path[0] != DIR_SEP;
#endif  /* _WIN32 */
}

#ifdef _WIN32
/* Add a slash if the UNC path points to a share. */
static string path_unc_add_slash_to_share(const string& path)
{
	size_t slash_after_server = path.find(DIR_SEP, 2);
	if(slash_after_server != string::npos) {
		size_t slash_after_share = path.find(DIR_SEP,
		                                     slash_after_server + 1);
		if(slash_after_share == string::npos) {
			return path + DIR_SEP;
		}
	}
	return path;
}

/* Convert:
 *    \\?\UNC\server\share\folder\... to \\server\share\folder\...
 *    \\?\C:\ to C:\ and \\?\C:\folder\... to C:\folder\...
 */
static string path_unc_to_short(const string& path)
{
	size_t len = path.size();
	if((len > 3) &&
	   (path[0] ==  DIR_SEP) &&
	   (path[1] ==  DIR_SEP) &&
	   (path[2] ==  '?') &&
	   ((path[3] ==  DIR_SEP) || (path[3] ==  DIR_SEP_ALT)))
	{
		if((len > 5) && (path[5] ==  ':')) {
			return path.substr(4, len - 4);
		}
		else if((len > 7) &&
		        (path.substr(4, 3) == "UNC") &&
		        ((path[7] ==  DIR_SEP) || (path[7] ==  DIR_SEP_ALT)))
		{
			return "\\\\" + path.substr(8, len - 8);
		}
	}
	return path;
}

static string path_cleanup_unc(const string& path)
{
	string result = path_unc_to_short(path);
	if(path.size() > 2) {
		/* It's possible path is now a non-UNC. */
		if(result[0] == DIR_SEP && result[1] == DIR_SEP) {
			return path_unc_add_slash_to_share(result);
		}
	}
	return result;
}

/* Make path compatible for stat() functions. */
static string path_make_compatible(const string& path)
{
	string result = path;
	/* In Windows stat() doesn't recognize dir ending on a slash. */
	if(result.size() > 3 && result[result.size() - 1] == DIR_SEP) {
		result.resize(result.size() - 1);
	}
	/* Clean up UNC path. */
	if((path.size() >= 3) && (path[0] == DIR_SEP) && (path[1] == DIR_SEP)) {
		result = path_cleanup_unc(result);
	}
	/* Make sure volume-only path ends up wit ha directory separator. */
	if(result.size() == 2 && result[1] == ':') {
		result += DIR_SEP;
	}
	return result;
}

static int path_wstat(const wstring& path_wc, path_stat_t *st)
{
#if defined(_MSC_VER) || defined(__MINGW64__)
	return _wstat64(path_wc.c_str(), st);
#elif defined(__MINGW32__)
	return _wstati64(path_wc.c_str(), st);
#else
	return _wstat(path_wc.c_str(), st);
#endif
}

static int path_stat(const string& path, path_stat_t *st)
{
	wstring path_wc = string_to_wstring(path);
	return path_wstat(path_wc, st);
}
#else  /* _WIN32 */
static int path_stat(const string& path, path_stat_t *st)
{
	return stat(path.c_str(), st);
}
#endif  /* _WIN32 */

size_t path_file_size(const string& path)
{
	path_stat_t st;
	if(path_stat(path, &st) != 0) {
		return -1;
	}
	return st.st_size;
}

bool path_exists(const string& path)
{
#ifdef _WIN32
	string fixed_path = path_make_compatible(path);
	wstring path_wc = string_to_wstring(fixed_path);
	path_stat_t st;
	if(path_wstat(path_wc, &st) != 0) {
		return false;
	}
	return st.st_mode != 0;
#else  /* _WIN32 */
	struct stat st;
	if(stat(path.c_str(), &st) != 0) {
		return 0;
	}
	return st.st_mode != 0;
#endif /* _WIN32 */
}

bool path_is_directory(const string& path)
{
	path_stat_t st;
	if(path_stat(path, &st) != 0) {
		return false;
	}
	return S_ISDIR(st.st_mode);
}

static void path_files_md5_hash_recursive(MD5Hash& hash, const string& dir)
{
	if(path_exists(dir)) {
		directory_iterator it(dir), it_end;

		for(; it != it_end; ++it) {
			if(path_is_directory(it->path())) {
				path_files_md5_hash_recursive(hash, it->path());
			}
			else {
				string filepath = it->path();

				hash.append((const uint8_t*)filepath.c_str(), filepath.size());
				hash.append_file(filepath);
			}
		}
	}
}

string path_files_md5_hash(const string& dir)
{
	/* computes md5 hash of all files in the directory */
	MD5Hash hash;

	path_files_md5_hash_recursive(hash, dir);

	return hash.get_hex();
}

static bool create_directories_recursivey(const string& path)
{
	if(path_is_directory(path)) {
		/* Directory already exists, nothing to do. */
		return true;
	}
	if(path_exists(path)) {
		/* File exists and it's not a directory. */
		return false;
	}

	string parent = path_dirname(path);
	if(parent.size() > 0 && parent != path) {
		if(!create_directories_recursivey(parent)) {
			return false;
		}
	}

#ifdef _WIN32
	wstring path_wc = string_to_wstring(path);
	return _wmkdir(path_wc.c_str()) == 0;
#else
	return mkdir(path.c_str(), 0777) == 0;
#endif
}

void path_create_directories(const string& filepath)
{
	string path = path_dirname(filepath);
	create_directories_recursivey(path);
}

bool path_write_binary(const string& path, const vector<uint8_t>& binary)
{
	path_create_directories(path);

	/* write binary file from memory */
	FILE *f = path_fopen(path, "wb");

	if(!f)
		return false;

	if(binary.size() > 0)
		fwrite(&binary[0], sizeof(uint8_t), binary.size(), f);

	fclose(f);

	return true;
}

bool path_write_text(const string& path, string& text)
{
	vector<uint8_t> binary(text.length(), 0);
	std::copy(text.begin(), text.end(), binary.begin());

	return path_write_binary(path, binary);
}

bool path_read_binary(const string& path, vector<uint8_t>& binary)
{
	/* read binary file into memory */
	FILE *f = path_fopen(path, "rb");

	if(!f) {
		binary.resize(0);
		return false;
	}

	binary.resize(path_file_size(path));

	if(binary.size() == 0) {
		fclose(f);
		return false;
	}

	if(fread(&binary[0], sizeof(uint8_t), binary.size(), f) != binary.size()) {
		fclose(f);
		return false;
	}

	fclose(f);

	return true;
}

bool path_read_text(const string& path, string& text)
{
	vector<uint8_t> binary;

	if(!path_exists(path) || !path_read_binary(path, binary))
		return false;

	const char *str = (const char*)&binary[0];
	size_t size = binary.size();
	text = string(str, size);

	return true;
}

uint64_t path_modified_time(const string& path)
{
	path_stat_t st;
	if(path_stat(path, &st) != 0) {
		return 0;
	}
	return st.st_mtime;
}

bool path_remove(const string& path)
{
	return remove(path.c_str()) == 0;
}

struct SourceReplaceState {
	typedef map<string, string> ProcessedMapping;
	/* Base director for all relative include headers. */
	string base;
	/* Result of processed files. */
	ProcessedMapping processed_files;
	/* Set of files which are considered "precompiled" and which are replaced
	 * with and empty string on a subsequent occurrence in include statement.
	 */
	set<string> precompiled_headers;
};

static string path_source_replace_includes_recursive(
        const string& source,
        const string& source_filepath,
        SourceReplaceState *state);

static string line_directive(const SourceReplaceState& state,
                             const string& path,
                             const int line)
{
	string unescaped_path = path;
	/* First we make path relative. */
	if(string_startswith(unescaped_path, state.base.c_str())) {
		const string base_file = path_filename(state.base);
		const size_t base_len = state.base.length();
		unescaped_path = base_file +
		        unescaped_path.substr(base_len,
		                            unescaped_path.length() - base_len);
	}
	/* Second, we replace all unsafe characters. */
	const size_t length = unescaped_path.length();
	string escaped_path = "";
	for(size_t i = 0; i < length; ++i) {
		const char ch = unescaped_path[i];
		if(strchr("\"\'\?\\", ch) != NULL) {
			escaped_path += "\\";
		}
		escaped_path += ch;
	}
	/* TODO(sergey): Check whether using std::to_string combined with several
	 * concatenation operations is any faster.
	 */
	return string_printf("#line %d \"%s\"", line, escaped_path.c_str());
}

static string path_source_handle_preprocessor(
        const string& preprocessor_line,
        const string& source_filepath,
        const size_t line_number,
        SourceReplaceState *state)
{
	string result = preprocessor_line;
	string token = string_strip(
	        preprocessor_line.substr(1, preprocessor_line.size() - 1));
	if(string_startswith(token, "include")) {
		token = string_strip(token.substr(7, token.size() - 7));
		if(token[0] == '"') {
			const size_t n_start = 1;
			const size_t n_end = token.find("\"", n_start);
			const string filename = token.substr(n_start, n_end - n_start);
			const bool is_precompiled = string_endswith(token, "// PRECOMPILED");
			string filepath = path_join(state->base, filename);
			if(!path_exists(filepath)) {
				filepath = path_join(path_dirname(source_filepath),
				                     filename);
			}
			if(is_precompiled) {
				state->precompiled_headers.insert(filepath);
			}
			string text;
			if(path_read_text(filepath, text)) {
				text = path_source_replace_includes_recursive(
				        text, filepath, state);
				/* Use line directives for better error messages. */
				result = line_directive(*state, filepath, 1) + "\n"
				     + text + "\n"
				     + line_directive(*state, source_filepath, line_number + 1);
			}
		}
	}
	return result;
}

/* Our own little c preprocessor that replaces #includes with the file
 * contents, to work around issue of OpenCL drivers not supporting
 * include paths with spaces in them.
 */
static string path_source_replace_includes_recursive(
        const string& source,
        const string& source_filepath,
        SourceReplaceState *state)
{
	/* Try to re-use processed file without spending time on replacing all
	 * include directives again.
	 */
	SourceReplaceState::ProcessedMapping::iterator replaced_file =
	        state->processed_files.find(source_filepath);
	if(replaced_file != state->processed_files.end()) {
		if(state->precompiled_headers.find(source_filepath) !=
		        state->precompiled_headers.end()) {
			return "";
		}
		return replaced_file->second;
	}
	/* Perform full file processing. */
	string result = "";
	const size_t source_length = source.length();
	size_t index = 0;
	/* Information about where we are in the source. */
	size_t line_number = 0, column_number = 1;
	/* Currently gathered non-preprocessor token.
	 * Store as start/length rather than token itself to avoid overhead of
	 * memory re-allocations on each character concatenation.
	 */
	size_t token_start = 0, token_length = 0;
	/* Denotes whether we're inside of preprocessor line, together with
	 * preprocessor line itself.
	 *
	 * TODO(sergey): Investigate whether using token start/end position
	 * gives measurable speedup.
	 */
	bool inside_preprocessor = false;
	string preprocessor_line = "";
	/* Actual loop over the whole source. */
	while(index < source_length) {
		const char ch = source[index];
		if(ch == '\n') {
			if(inside_preprocessor) {
				result += path_source_handle_preprocessor(preprocessor_line,
				                                          source_filepath,
				                                          line_number,
				                                          state);
				/* Start gathering net part of the token. */
				token_start = index;
				token_length = 0;
			}
			inside_preprocessor = false;
			preprocessor_line = "";
			column_number = 0;
			++line_number;
		}
		else if(ch == '#' && column_number == 1 && !inside_preprocessor) {
			/* Append all possible non-preprocessor token to the result. */
			if(token_length != 0) {
				result.append(source, token_start, token_length);
				token_start = index;
				token_length = 0;
			}
			inside_preprocessor = true;
		}
		if(inside_preprocessor) {
			preprocessor_line += ch;
		}
		else {
			++token_length;
		}
		++index;
		++column_number;
	}
	/* Append possible tokens which happened before special events handled
	 * above.
	 */
	if(token_length != 0) {
		result.append(source, token_start, token_length);
	}
	if(inside_preprocessor) {
		result += path_source_handle_preprocessor(preprocessor_line,
		                                          source_filepath,
		                                          line_number,
		                                          state);
	}
	/* Store result for further reuse. */
	state->processed_files[source_filepath] = result;
	return result;
}

string path_source_replace_includes(const string& source,
                                    const string& path,
                                    const string& source_filename)
{
	SourceReplaceState state;
	state.base = path;
	return path_source_replace_includes_recursive(
	        source,
	        path_join(path, source_filename),
	        &state);
}

FILE *path_fopen(const string& path, const string& mode)
{
#ifdef _WIN32
	wstring path_wc = string_to_wstring(path);
	wstring mode_wc = string_to_wstring(mode);
	return _wfopen(path_wc.c_str(), mode_wc.c_str());
#else
	return fopen(path.c_str(), mode.c_str());
#endif
}

void path_cache_clear_except(const string& name, const set<string>& except)
{
	string dir = path_user_get("cache");

	if(path_exists(dir)) {
		directory_iterator it(dir), it_end;

		for(; it != it_end; ++it) {
			string filename = path_filename(it->path());

			if(string_startswith(filename, name.c_str()))
				if(except.find(filename) == except.end())
					path_remove(it->path());
		}
	}

}

CCL_NAMESPACE_END
