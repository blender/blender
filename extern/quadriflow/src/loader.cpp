//
//  loader.cpp
//  Loop
//
//  Created by Jingwei on 10/22/17.
//  Copyright © 2017 Jingwei. All rights reserved.
//

#include "loader.hpp"

#include <fstream>
#include <unordered_map>

namespace qflow {

inline std::vector<std::string> &str_tokenize(const std::string &s, char delim, std::vector<std::string> &elems, bool include_empty = false) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
		if (!item.empty() || include_empty)
			elems.push_back(item);
	return elems;
}

inline std::vector<std::string> str_tokenize(const std::string &s, char delim, bool include_empty) {
	std::vector<std::string> elems;
	str_tokenize(s, delim, elems, include_empty);
	return elems;
}

inline uint32_t str_to_uint32_t(const std::string &str) {
	char *end_ptr = nullptr;
	uint32_t result = (uint32_t)strtoul(str.c_str(), &end_ptr, 10);
	if (*end_ptr != '\0')
		throw std::runtime_error("Could not parse unsigned integer \"" + str + "\"");
	return result;
}

void load(const char* filename, MatrixXd& V, MatrixXi& F)
{
	/// Vertex indices used by the OBJ format
	struct obj_vertex {
		uint32_t p = (uint32_t)-1;
		uint32_t n = (uint32_t)-1;
		uint32_t uv = (uint32_t)-1;

		inline obj_vertex() { }

		inline obj_vertex(const std::string &string) {
			std::vector<std::string> tokens = str_tokenize(string, '/', true);

			if (tokens.size() < 1 || tokens.size() > 3)
				throw std::runtime_error("Invalid vertex data: \"" + string + "\"");

			p = str_to_uint32_t(tokens[0]);

#if 0
			if (tokens.size() >= 2 && !tokens[1].empty())
				uv = str_to_uint32_t(tokens[1]);

			if (tokens.size() >= 3 && !tokens[2].empty())
				n = str_to_uint32_t(tokens[2]);
#endif
		}

		inline bool operator==(const obj_vertex &v) const {
			return v.p == p && v.n == n && v.uv == uv;
		}
	};

	/// Hash function for obj_vertex
	struct obj_vertexHash : std::unary_function<obj_vertex, size_t> {
		std::size_t operator()(const obj_vertex &v) const {
			size_t hash = std::hash<uint32_t>()(v.p);
			hash = hash * 37 + std::hash<uint32_t>()(v.uv);
			hash = hash * 37 + std::hash<uint32_t>()(v.n);
			return hash;
		}
	};

	typedef std::unordered_map<obj_vertex, uint32_t, obj_vertexHash> VertexMap;

	std::ifstream is(filename);

	std::vector<Vector3d>   positions;
	//std::vector<Vector2d>   texcoords;
	//std::vector<Vector3d>   normals;
	std::vector<uint32_t>   indices;
	std::vector<obj_vertex> vertices;
	VertexMap vertexMap;

	std::string line_str;
	while (std::getline(is, line_str)) {
		std::istringstream line(line_str);

		std::string prefix;
		line >> prefix;

		if (prefix == "v") {
			Vector3d p;
			line >> p.x() >> p.y() >> p.z();
			positions.push_back(p);
		}
		else if (prefix == "vt") {
			/*
			Vector2d tc;
			line >> tc.x() >> tc.y();
			texcoords.push_back(tc);
			*/
		}
		else if (prefix == "vn") {
			/*
			Vector3d n;
			line >> n.x() >> n.y() >> n.z();
			normals.push_back(n);
			*/
		}
		else if (prefix == "f") {
			std::string v1, v2, v3, v4;
			line >> v1 >> v2 >> v3 >> v4;
			obj_vertex tri[6];
			int nVertices = 3;

			tri[0] = obj_vertex(v1);
			tri[1] = obj_vertex(v2);
			tri[2] = obj_vertex(v3);

			if (!v4.empty()) {
				/* This is a quad, split into two triangles */
				tri[3] = obj_vertex(v4);
				tri[4] = tri[0];
				tri[5] = tri[2];
				nVertices = 6;
			}
			/* Convert to an indexed vertex list */
			for (int i = 0; i<nVertices; ++i) {
				const obj_vertex &v = tri[i];
				VertexMap::const_iterator it = vertexMap.find(v);
				if (it == vertexMap.end()) {
					vertexMap[v] = (uint32_t)vertices.size();
					indices.push_back((uint32_t)vertices.size());
					vertices.push_back(v);
				}
				else {
					indices.push_back(it->second);
				}
			}
		}
	}

	F.resize(3, indices.size() / 3);
	memcpy(F.data(), indices.data(), sizeof(uint32_t)*indices.size());

	V.resize(3, vertices.size());
	for (uint32_t i = 0; i<vertices.size(); ++i)
		V.col(i) = positions.at(vertices[i].p - 1);
}

} // namespace qflow
