/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Singleton to manage exceptions
 */

#include "Exception.h"

namespace Freestyle {

Exception::exception_type Exception::_exception = Exception::NO_EXCEPTION;

} /* namespace Freestyle */
