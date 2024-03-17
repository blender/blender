/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
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

#include "devices/DeviceManager.h"
#include "devices/IDeviceFactory.h"
#include "devices/IDevice.h"
#include "devices/I3DDevice.h"

#include <limits>
#include <string>
#include <algorithm>

AUD_NAMESPACE_BEGIN

std::unordered_map<std::string, std::shared_ptr<IDeviceFactory>> DeviceManager::m_factories;
std::shared_ptr<IDevice> DeviceManager::m_device;

void DeviceManager::registerDevice(const std::string &name, std::shared_ptr<IDeviceFactory> factory)
{
	m_factories[name] = factory;
}

std::shared_ptr<IDeviceFactory> DeviceManager::getDeviceFactory(const std::string &name)
{
	auto it = m_factories.find(name);

	if(it == m_factories.end())
		return nullptr;

	return it->second;
}

std::shared_ptr<IDeviceFactory> DeviceManager::getDefaultDeviceFactory()
{
	int min = std::numeric_limits<int>::min();

	std::shared_ptr<IDeviceFactory> result;

	for(auto factory : m_factories)
	{
		if(factory.second->getPriority() >= min)
		{
			result = factory.second;
			min = result->getPriority();
		}
	}

	return result;
}

void DeviceManager::setDevice(std::shared_ptr<IDevice> device)
{
	m_device = device;
}

void DeviceManager::openDevice(const std::string &name)
{
	setDevice(getDeviceFactory(name)->openDevice());
}

void DeviceManager::openDefaultDevice()
{
	setDevice(getDefaultDeviceFactory()->openDevice());
}

void DeviceManager::releaseDevice()
{
	m_device = nullptr;
}

std::shared_ptr<IDevice> DeviceManager::getDevice()
{
	return m_device;
}

std::shared_ptr<I3DDevice> DeviceManager::get3DDevice()
{
	return std::dynamic_pointer_cast<I3DDevice>(m_device);
}

std::vector<std::string> DeviceManager::getAvailableDeviceNames()
{
	struct DeviceNamePriority {
		std::string name;
		int priority;
	};

	std::vector<DeviceNamePriority> devices;
	devices.reserve(m_factories.size());

	for(const auto& pair : m_factories)
		devices.push_back({pair.first, pair.second->getPriority()});

	auto sort = [](const DeviceNamePriority& lhs, const DeviceNamePriority& rhs){
		return lhs.priority > rhs.priority;
	};

	std::sort(devices.begin(), devices.end(), sort);

	std::vector<std::string> names;
	names.reserve(devices.size());

	for(const auto& device : devices)
		names.push_back(device.name);

	return names;
}

AUD_NAMESPACE_END
