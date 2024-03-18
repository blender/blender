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

#pragma once

/**
 * \def AUD_NOEXCEPT
 * Compatibility macro for noexcept.
 */
#ifdef _MSC_VER
#define AUD_NOEXCEPT
#else
#define AUD_NOEXCEPT noexcept
#endif

/**
 * @file Exception.h
 * @ingroup general
 * Defines the Exception class as well as the AUD_THROW macro for easy throwing.
 */

#include "Audaspace.h"

#include <exception>
#include <string>

/// Throws a Exception with the provided error code.
#define AUD_THROW(exception, message) { throw exception(message, __FILE__, __LINE__); }

AUD_NAMESPACE_BEGIN

/**
 * The Exception class is the general exception base class.
 */
class AUD_API Exception : public std::exception
{
protected:
	/// A message describing the problem.
	const std::string m_message;

	/// The source code file in which the exception was thrown.
	const std::string m_file;

	/// The source code line from which the exception was thrown.
	const int m_line;

	/**
	 * Copy constructor.
	 * @param exception The exception to be copied.
	 */
	Exception(const Exception& exception);

	/**
	 * Creates a new Exception object.
	 * @param message A message describing the problem.
	 * @param file The source code file in which the exception was thrown.
	 * @param line The source code line from which the exception was thrown.
	 */
	Exception(std::string message, std::string file, int line);
public:
	/**
	 * Destroys the object.
	 */
	virtual ~Exception() AUD_NOEXCEPT;

	/**
	 * Returns the error message.
	 * @return A C string error message.
	 */
	virtual const char* what() const AUD_NOEXCEPT;

	/**
	 * Returns the error message plus file and line number for debugging purposes.
	 * @return The error message including debug information.
	 */
	virtual std::string getDebugMessage() const;

	/**
	 * Returns the error message.
	 * @return The error message as string.
	 */
	const std::string& getMessage() const;

	/**
	 * Returns the file in which the exception was thrown.
	 * @return The name of the file in which the exception was thrown.
	 */
	const std::string& getFile() const;

	/**
	 * Returns the line where the exception was originally thrown.
	 * @return The line of the source file where the exception was generated.
	 */
	int getLine() const;
};

/**
 * The FileException class is used for error cases in which files cannot
 * be read or written due to unknown containers or codecs.
 */
class AUD_API FileException : public Exception
{
public:
	/**
	 * Creates a new FileException object.
	 * @param message A message describing the problem.
	 * @param file The source code file in which the exception was thrown.
	 * @param line The source code line from which the exception was thrown.
	 */
	FileException(std::string message, std::string file, int line);

	/**
	 * Copy constructor.
	 * @param exception The exception to be copied.
	 */
	FileException(const FileException& exception);

	~FileException() AUD_NOEXCEPT;
};

/**
 * The DeviceException class is used for error cases in connection with
 * devices, which usually happens when specific features or requests
 * cannot be fulfilled by a device, for example when the device is opened.
 */
class AUD_API DeviceException : public Exception
{
public:
	/**
	 * Creates a new DeviceException object.
	 * @param message A message describing the problem.
	 * @param file The source code file in which the exception was thrown.
	 * @param line The source code line from which the exception was thrown.
	 */
	DeviceException(std::string message, std::string file, int line);

	/**
	 * Copy constructor.
	 * @param exception The exception to be copied.
	 */
	DeviceException(const DeviceException& exception);

	~DeviceException() AUD_NOEXCEPT;
};

/**
 * The StateException class is used for error cases of sounds or readers
 * with illegal states or requirements for states of dependent classes.
 * It is used for example when an effect reader needs a specific
 * specification from its input.
 */
class AUD_API StateException : public Exception
{
public:
	/**
	 * Creates a new StateException object.
	 * @param message A message describing the problem.
	 * @param file The source code file in which the exception was thrown.
	 * @param line The source code line from which the exception was thrown.
	 */
	StateException(std::string message, std::string file, int line);

	/**
	 * Copy constructor.
	 * @param exception The exception to be copied.
	 */
	StateException(const StateException& exception);

	~StateException() AUD_NOEXCEPT;
};

AUD_NAMESPACE_END
