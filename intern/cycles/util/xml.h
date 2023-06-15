/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_XML_H__
#define __UTIL_XML_H__

/* PugiXML is used for XML parsing. */

#include <pugixml.hpp>

CCL_NAMESPACE_BEGIN

OIIO_NAMESPACE_USING

#ifdef WITH_SYSTEM_PUGIXML
#  define PUGIXML_NAMESPACE pugi
#else
#  define PUGIXML_NAMESPACE OIIO_NAMESPACE::pugi
#endif

using PUGIXML_NAMESPACE::xml_attribute;
using PUGIXML_NAMESPACE::xml_document;
using PUGIXML_NAMESPACE::xml_node;
using PUGIXML_NAMESPACE::xml_parse_result;

CCL_NAMESPACE_END

#endif /* __UTIL_XML_H__ */
