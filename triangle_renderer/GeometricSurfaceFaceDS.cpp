///////////////////////////////////////////////////
//
//	Hamish Carr
//	January, 2018
//
//	------------------------
//	GeometricSurfaceFaceDS.cpp
//	------------------------
//	
//	Base code for geometric assignments.
//
//	This is the minimalistic Face-based D/S for storing
//	surfaces, to be used as the basis for fuller versions
//	
//	It will include object load / save code & render code
//	
///////////////////////////////////////////////////


#include "GeometricSurfaceFaceDS.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <locale>
#include <cctype>
#include <cmath>
#include <math.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

namespace
{
// Consume a whole whitespace-delimited value, so malformed tokens cannot
// silently become several coordinates or indices.
template <typename T>
bool ReadValue(std::istream &input, T &value)
{
    std::string token;
    if (!(input >> token)) return false;
    std::istringstream number(token);
    number.imbue(std::locale::classic());
    return (number >> value) && number.peek() == std::char_traits<char>::eof();
}
}

// constructor will initialise to safe values
GeometricSurfaceFaceDS::GeometricSurfaceFaceDS()
	{ // GeometricSurfaceFaceDS::GeometricSurfaceFaceDS()
	// force the size to nil (should not be necessary, but . . .)
	vertices.resize(0);

	// set this to something reasonable
	boundingSphereSize = 1.0;
	
	// set the midpoint to the origin
	midPoint = Cartesian3(0.0, 0.0, 0.0);
	} // GeometricSurfaceFaceDS::GeometricSurfaceFaceDS()

// Dispatch indexed formats to a small adapter; rendering still uses triangle soup.
bool GeometricSurfaceFaceDS::ReadFile(char *fileName)
	{
	std::string name(fileName);
	const std::size_t dot = name.find_last_of('.');
	std::string extension = dot == std::string::npos ? "" : name.substr(dot);
	for (std::size_t i = 0; i < extension.size(); ++i)
		extension[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(extension[i])));
	if (extension == ".face" || extension == ".diredge")
		return ReadFileIndexed(fileName);
	return ReadFileTriangleSoup(fileName);
	}

// read routine returns true on success, failure otherwise
bool GeometricSurfaceFaceDS::ReadFileTriangleSoup(char *fileName)
	{ // GeometricSurfaceFaceDS::ReadFileTriangleSoup()
	std::ifstream inFile(fileName);
	inFile.imbue(std::locale::classic());
	long nTriangles = 0;
	if (!inFile || !ReadValue(inFile, nTriangles) || nTriangles < 0)
		return false;
	std::vector<Cartesian3> triangles;
	if (static_cast<std::size_t>(nTriangles) > triangles.max_size() / 3)
		return false;
	for (std::size_t i = 0; i < static_cast<std::size_t>(nTriangles) * 3; ++i)
		{
		Cartesian3 vertex;
		if (!ReadValue(inFile, vertex.x) || !ReadValue(inFile, vertex.y)
			|| !ReadValue(inFile, vertex.z) || !std::isfinite(vertex.x)
			|| !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) return false;
		triangles.push_back(vertex);
		}
	vertices.swap(triangles);
	PrepareForViewing();
	return true;
	} // GeometricSurfaceFaceDS::ReadFileTriangleSoup()

// Only Vertex and Face records are needed for drawing. Directed-edge records
// are intentionally skipped: this adapter displays geometry, not connectivity.
bool GeometricSurfaceFaceDS::ReadFileIndexed(char *fileName)
	{
	std::ifstream inFile(fileName);
	if (!inFile) return false;
	std::vector<Cartesian3> indexedVertices, triangles;
	std::size_t faceCount = 0;
	std::string line;
	while (std::getline(inFile, line))
		{
		std::istringstream record(line);
		record.imbue(std::locale::classic());
		std::string kind, extra;
		if (!(record >> kind) || kind[0] == '#') continue;
		if (kind == "FirstDirectedEdge" || kind == "OtherHalf") continue;
		std::size_t id;
		if (!ReadValue(record, id)) return false;
		if (kind == "Vertex")
			{
			Cartesian3 vertex;
			if (faceCount != 0 || id != indexedVertices.size()
				|| !ReadValue(record, vertex.x) || !ReadValue(record, vertex.y)
				|| !ReadValue(record, vertex.z) || !std::isfinite(vertex.x)
				|| !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) return false;
			indexedVertices.push_back(vertex);
			}
		else if (kind == "Face")
			{
			std::size_t indices[3];
			if (id != faceCount || !ReadValue(record, indices[0])
				|| !ReadValue(record, indices[1]) || !ReadValue(record, indices[2])) return false;
			for (int corner = 0; corner < 3; ++corner)
				{
				if (indices[corner] >= indexedVertices.size()) return false;
				triangles.push_back(indexedVertices[indices[corner]]);
				}
			++faceCount;
			}
		else return false;
		if (record >> extra) return false;
		}
	if (inFile.bad()) return false;
	vertices.swap(triangles);
	PrepareForViewing();
	return true;
	}

// Use the same expanded triangle vertices for every format, so shared vertices
// have the same weighting when centering and the same viewing scale as .tri.
void GeometricSurfaceFaceDS::PrepareForViewing()
	{
	midPoint = Cartesian3(0.0, 0.0, 0.0);
	boundingSphereSize = 1.0;
	if (vertices.empty()) return;
	Cartesian3 minCoords = vertices[0], maxCoords = vertices[0];
	for (std::size_t i = 0; i < vertices.size(); ++i)
		{
		midPoint = midPoint + vertices[i];
		if (vertices[i].x < minCoords.x) minCoords.x = vertices[i].x;
		if (vertices[i].y < minCoords.y) minCoords.y = vertices[i].y;
		if (vertices[i].z < minCoords.z) minCoords.z = vertices[i].z;
		if (vertices[i].x > maxCoords.x) maxCoords.x = vertices[i].x;
		if (vertices[i].y > maxCoords.y) maxCoords.y = vertices[i].y;
		if (vertices[i].z > maxCoords.z) maxCoords.z = vertices[i].z;
		}
	midPoint = midPoint / vertices.size();
	for (std::size_t i = 0; i < vertices.size(); ++i)
		vertices[i] = vertices[i] - midPoint;
	boundingSphereSize = sqrt((maxCoords - minCoords).length());
	if (boundingSphereSize == 0.0) boundingSphereSize = 1.0;
	}

// routine to render
void GeometricSurfaceFaceDS::Render()
	{ // GeometricSurfaceFaceDS::Render()
	// walk through the faces rendering each one
	glBegin(GL_TRIANGLES);

	// we will loop in 3's, assuming CCW order
	for (unsigned int vertex = 0; vertex < vertices.size(); )
		{ // per triangle
		// use increment to step through them
		Cartesian3 *v0 = &(vertices[vertex++]);
		Cartesian3 *v1 = &(vertices[vertex++]);
		Cartesian3 *v2 = &(vertices[vertex++]);
		// now compute the normal vector
		Cartesian3 uVec = *v1 - *v0;
		Cartesian3 vVec = *v2 - *v0;
		Cartesian3 normal = uVec.cross(vVec).normalise();

		glNormal3fv(&normal.x);
		glVertex3fv(&v0->x);
		glVertex3fv(&v1->x);
		glVertex3fv(&v2->x);
		} // per triangle
	glEnd();
	} // GeometricSurfaceFaceDS::Render()
