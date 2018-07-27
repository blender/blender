/*
 * Copyright 2011-2018 Blender Foundation
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

#include "render/stats.h"
#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_string.h"

CCL_NAMESPACE_BEGIN

static int kIndentNumSpaces = 2;

/* Named size entry. */

namespace {

bool namedSizeEntryComparator(const NamedSizeEntry& a, const NamedSizeEntry& b)
{
	/* We sort in descending order. */
	return a.size > b.size;
}

}  // namespace

NamedSizeEntry::NamedSizeEntry()
    : name(""),
      size(0) {
}

NamedSizeEntry::NamedSizeEntry(const string& name, size_t size)
    : name(name),
      size(size) {
}

/* Named size statistics. */

NamedSizeStats::NamedSizeStats()
    : total_size(0) {
}

void NamedSizeStats::add_entry(const NamedSizeEntry& entry) {
	total_size += entry.size;
	entries.push_back(entry);
}

string NamedSizeStats::full_report(int indent_level)
{
	const string indent(indent_level * kIndentNumSpaces, ' ');
	const string double_indent = indent + indent;
	string result = "";
	result += string_printf("%sTotal memory: %s (%s)\n",
	                        indent.c_str(),
	                        string_human_readable_size(total_size).c_str(),
	                        string_human_readable_number(total_size).c_str());
	sort(entries.begin(), entries.end(), namedSizeEntryComparator);
	foreach(const NamedSizeEntry& entry, entries) {
		result += string_printf(
		        "%s%-32s %s (%s)\n",
		        double_indent.c_str(),
		        entry.name.c_str(),
		        string_human_readable_size(entry.size).c_str(),
		        string_human_readable_number(entry.size).c_str());
	}
	return result;
}

/* Mesh statistics. */

MeshStats::MeshStats() {
}

string MeshStats::full_report(int indent_level)
{
	const string indent(indent_level * kIndentNumSpaces, ' ');
	string result = "";
	result += indent + "Geometry:\n" + geometry.full_report(indent_level + 1);
	return result;
}

/* Image statistics. */

ImageStats::ImageStats() {
}

string ImageStats::full_report(int indent_level)
{
	const string indent(indent_level * kIndentNumSpaces, ' ');
	string result = "";
	result += indent + "Textures:\n" + textures.full_report(indent_level + 1);
	return result;
}

/* Overall statistics. */

RenderStats::RenderStats() {
}

string RenderStats::full_report()
{
	string result = "";
	result += "Mesh statistics:\n" + mesh.full_report(1);
	result += "Image statistics:\n" + image.full_report(1);
	return result;
}

CCL_NAMESPACE_END
