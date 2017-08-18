/*******************************************************************************
* Copyright 2015-2016 Juan Francisco Crespo Galán
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
******************************************************************************/

#include "fx/HRTFLoader.h"
#include "file/File.h"
#include "Exception.h"

#include <windows.h>
#include <exception>

AUD_NAMESPACE_BEGIN

std::shared_ptr<HRTF> HRTFLoader::loadLeftHRTFs(std::shared_ptr<FFTPlan> plan, const std::string& fileExtension, const std::string& path)
{
	std::shared_ptr<HRTF> hrtfs(std::make_shared<HRTF>(plan));
	loadHRTFs(hrtfs, 'L', fileExtension, path);
	return hrtfs;
}

std::shared_ptr<HRTF> HRTFLoader::loadRightHRTFs(std::shared_ptr<FFTPlan> plan, const std::string& fileExtension, const std::string& path)
{
	std::shared_ptr<HRTF> hrtfs(std::make_shared<HRTF>(plan));
	loadHRTFs(hrtfs, 'R', fileExtension, path);
	return hrtfs;
}

std::shared_ptr<HRTF> HRTFLoader::loadLeftHRTFs(const std::string& fileExtension, const std::string& path)
{
	std::shared_ptr<HRTF> hrtfs(std::make_shared<HRTF>());
	loadHRTFs(hrtfs, 'L', fileExtension, path);
	return hrtfs;
}

std::shared_ptr<HRTF> HRTFLoader::loadRightHRTFs(const std::string& fileExtension, const std::string& path)
{
	std::shared_ptr<HRTF> hrtfs(std::make_shared<HRTF>());
	loadHRTFs(hrtfs, 'R', fileExtension, path);
	return hrtfs;
}

void HRTFLoader::loadHRTFs(std::shared_ptr<HRTF> hrtfs, char ear, const std::string& fileExtension, const std::string& path)
{
	std::string readpath = path;
	if(path == "")
		readpath = ".";

	WIN32_FIND_DATA entry;
	bool found_file = true;
	std::string search = readpath + "\\*";
	HANDLE dir = FindFirstFile(search.c_str(), &entry);
	if(dir == INVALID_HANDLE_VALUE)
		return;

	float azim, elev;

	while(found_file)
	{
		std::string filename = entry.cFileName;
		if(filename.front() == ear && filename.length() >= fileExtension.length() && filename.substr(filename.length() - fileExtension.length()) == fileExtension)
		{
			try
			{
				elev = std::stof(filename.substr(1, filename.find("e") - 1));
				azim = std::stof(filename.substr(filename.find("e") + 1, filename.find("a") - filename.find("e") - 1));
				if(ear == 'L')
					azim = 360 - azim;
			}
			catch(std::exception& e)
			{
				AUD_THROW(FileException, "The HRTF name doesn't follow the naming scheme: " + filename);
			}
			hrtfs->addImpulseResponse(std::make_shared<StreamBuffer>(std::make_shared<File>(readpath + "/" + filename)), azim, elev);
		}	
		found_file = FindNextFile(dir, &entry);
	}
	FindClose(dir);
	return;
}

AUD_NAMESPACE_END