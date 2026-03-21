#pragma once

/**
 * @file ICaptureDeviceFactory.h
 * @ingroup devices
 * The ICaptureDeviceFactory interface.
 */

#include "Audaspace.h"
#include "respec/Specification.h"

#include <memory>
#include <string>

AUD_NAMESPACE_BEGIN

class IReader;

/**
 * @interface ICaptureDeviceFactory
 * The ICaptureDeviceFactory interface opens an input capture device.
 */
class AUD_API ICaptureDeviceFactory
{
public:
	/**
	 * Destroys the capture device factory.
	 */
	virtual ~ICaptureDeviceFactory() {}

	/**
	 * Opens an audio capture device.
	 * \param specs The desired specification.
	 * \param buffersize The capture buffer size in samples.
	 * \exception Exception Thrown if the audio device cannot be opened.
	 */
	virtual std::shared_ptr<IReader> openDevice(Specs specs, int buffersize)=0;

	/**
	 * Sets a name for the capture device.
	 * \param name The internal name for the capture device.
	 */
	virtual void setName(const std::string &name)=0;
};

AUD_NAMESPACE_END
