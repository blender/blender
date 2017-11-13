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
