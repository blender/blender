/*
 * Copyright 2017 Blender Foundation
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

#include <stdio.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <OpenImageIO/argparse.h>
#include <OpenImageIO/filesystem.h>

#include "cuew.h"

#ifdef _MSC_VER
# include <Windows.h>
#endif

using std::string;
using std::vector;

namespace std {
	template<typename T>
	std::string to_string(const T &n) {
		std::ostringstream s;
		s << n;
		return s.str();
	}
}

class CompilationSettings
{
public:
	CompilationSettings()
	: target_arch(0),
	  bits(64),
	  verbose(false),
	  fast_math(false)
	{}

	string cuda_toolkit_dir;
	string input_file;
	string output_file;
	string ptx_file;
	vector<string> defines;
	vector<string> includes;
	int target_arch;
	int bits;
	bool verbose;
	bool fast_math;
};

bool compile_cuda(CompilationSettings &settings)
{
	const char* headers[] = {"stdlib.h" , "float.h", "math.h", "stdio.h"};
	const char* header_content[] = {"\n", "\n", "\n", "\n"};

	printf("Building %s\n", settings.input_file.c_str());

	string code;
	if(!OIIO::Filesystem::read_text_file(settings.input_file, code)) {
		fprintf(stderr, "Error: unable to read %s\n", settings.input_file.c_str());
		return false;
	}

	vector<string> options;
	for(size_t i = 0; i < settings.includes.size(); i++) {
		options.push_back("-I" + settings.includes[i]);
	}

	for(size_t i = 0; i < settings.defines.size(); i++) {
		options.push_back("-D" + settings.defines[i]);
	}
	options.push_back("-D__KERNEL_CUDA_VERSION__=" + std::to_string(cuewNvrtcVersion()));
	options.push_back("-arch=compute_" + std::to_string(settings.target_arch));
	options.push_back("--device-as-default-execution-space");
	if(settings.fast_math)
		options.push_back("--use_fast_math");

	nvrtcProgram prog;
	nvrtcResult result = nvrtcCreateProgram(&prog,
		code.c_str(),                    // buffer
		NULL,                            // name
		sizeof(headers) / sizeof(void*), // numHeaders
		header_content,                  // headers
		headers);                        // includeNames

	if(result != NVRTC_SUCCESS) {
		fprintf(stderr, "Error: nvrtcCreateProgram failed (%x)\n\n", result);
		return false;
	}

	/* Tranfer options to a classic C array. */
	vector<const char*> opts(options.size());
	for(size_t i = 0; i < options.size(); i++) {
		opts[i] = options[i].c_str();
	}

	result = nvrtcCompileProgram(prog, options.size(), &opts[0]);

	if(result != NVRTC_SUCCESS) {
		fprintf(stderr, "Error: nvrtcCompileProgram failed (%x)\n\n", result);

		size_t log_size;
		nvrtcGetProgramLogSize(prog, &log_size);

		vector<char> log(log_size);
		nvrtcGetProgramLog(prog, &log[0]);
		fprintf(stderr, "%s\n", &log[0]);

		return false;
	}

	/* Retrieve the ptx code. */
	size_t ptx_size;
	result = nvrtcGetPTXSize(prog, &ptx_size);
	if(result != NVRTC_SUCCESS) {
		fprintf(stderr, "Error: nvrtcGetPTXSize failed (%x)\n\n", result);
		return false;
	}

	vector<char> ptx_code(ptx_size);
	result = nvrtcGetPTX(prog, &ptx_code[0]);
	if(result != NVRTC_SUCCESS) {
		fprintf(stderr, "Error: nvrtcGetPTX failed (%x)\n\n", result);
		return false;
	}

	/* Write a file in the temp folder with the ptx code. */
	settings.ptx_file = OIIO::Filesystem::temp_directory_path() + "/" + OIIO::Filesystem::unique_path();
	FILE * f= fopen(settings.ptx_file.c_str(), "wb");
	fwrite(&ptx_code[0], 1, ptx_size, f);
	fclose(f);

	return true;
}

bool link_ptxas(CompilationSettings &settings)
{
	string cudapath = "";
	if(settings.cuda_toolkit_dir.size())
		cudapath = settings.cuda_toolkit_dir + "/bin/";

	string ptx = "\"" +cudapath + "ptxas\" " + settings.ptx_file +
					" -o " + settings.output_file +
					" --gpu-name sm_" + std::to_string(settings.target_arch) +
					" -m" + std::to_string(settings.bits);

	if (settings.verbose)
	{
		ptx += " --verbose";
		printf("%s\n", ptx.c_str());
	}

	int pxresult = system(ptx.c_str());
	if(pxresult) {
		fprintf(stderr, "Error: ptxas failed (%x)\n\n", pxresult);
		return false;
	}

	if(!OIIO::Filesystem::remove(settings.ptx_file)) {
		fprintf(stderr, "Error: removing %s\n\n", settings.ptx_file.c_str());
	}

	return true;
}

bool init(CompilationSettings &settings)
{
#ifdef _MSC_VER
	if(settings.cuda_toolkit_dir.size()) {
		SetDllDirectory((settings.cuda_toolkit_dir + "/bin").c_str());
	}
#endif

	int cuewresult = cuewInit(CUEW_INIT_NVRTC);
	if(cuewresult != CUEW_SUCCESS) {
		fprintf(stderr, "Error: cuew init fialed (0x%x)\n\n", cuewresult);
		return false;
	}

	if(cuewNvrtcVersion() < 80) {
		fprintf(stderr, "Error: only cuda 8 and higher is supported, %d\n\n", cuewCompilerVersion());
		return false;
	}

	if(!nvrtcCreateProgram) {
		fprintf(stderr, "Error: nvrtcCreateProgram not resolved\n");
		return false;
	}

	if(!nvrtcCompileProgram) {
		fprintf(stderr, "Error: nvrtcCompileProgram not resolved\n");
		return false;
	}

	if(!nvrtcGetProgramLogSize) {
		fprintf(stderr, "Error: nvrtcGetProgramLogSize not resolved\n");
		return false;
	}

	if(!nvrtcGetProgramLog) {
		fprintf(stderr, "Error: nvrtcGetProgramLog not resolved\n");
		return false;
	}

	if(!nvrtcGetPTXSize) {
		fprintf(stderr, "Error: nvrtcGetPTXSize not resolved\n");
		return false;
	}

	if(!nvrtcGetPTX) {
		fprintf(stderr, "Error: nvrtcGetPTX not resolved\n");
		return false;
	}

	return true;
}

bool parse_parameters(int argc, const char **argv, CompilationSettings &settings)
{
	OIIO::ArgParse ap;
	ap.options("Usage: cycles_cubin_cc [options]",
		"-target %d", &settings.target_arch, "target shader model",
		"-m %d", &settings.bits, "Cuda architecture bits",
		"-i %s", &settings.input_file, "Input source filename",
		"-o %s", &settings.output_file, "Output cubin filename",
		"-I %L", &settings.includes, "Add additional includepath",
		"-D %L", &settings.defines, "Add additional defines",
		"-v", &settings.verbose, "Use verbose logging",
		"--use_fast_math", &settings.fast_math, "Use fast math",
		"-cuda-toolkit-dir %s", &settings.cuda_toolkit_dir, "path to the cuda toolkit binary directory",
		NULL);

	if(ap.parse(argc, argv) < 0) {
		fprintf(stderr, "%s\n", ap.geterror().c_str());
		ap.usage();
		return false;
	}

	if(!settings.output_file.size()) {
		fprintf(stderr, "Error: Output file not set(-o), required\n\n");
		return false;
	}

	if(!settings.input_file.size()) {
		fprintf(stderr, "Error: Input file not set(-i, required\n\n");
		return false;
	}

	if(!settings.target_arch) {
		fprintf(stderr, "Error: target shader model not set (-target), required\n\n");
		return false;
	}

	return true;
}

int main(int argc, const char **argv)
{
	CompilationSettings settings;

	if(!parse_parameters(argc, argv, settings)) {
		fprintf(stderr, "Error: invalid parameters, exiting\n");
		exit(EXIT_FAILURE);
	}

	if(!init(settings)) {
		fprintf(stderr, "Error: initialization error, exiting\n");
		exit(EXIT_FAILURE);
	}

	if(!compile_cuda(settings)) {
		fprintf(stderr, "Error: compilation error, exiting\n");
		exit(EXIT_FAILURE);
	}

	if(!link_ptxas(settings)) {
		exit(EXIT_FAILURE);
	}

	return 0;
}
