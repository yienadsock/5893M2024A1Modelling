#include "FaceIndexedMesh.h"

#include <iomanip>
#include <istream>
#include <limits>
#include <map>
#include <ostream>
#include <stdexcept>

FaceIndexedMesh FaceIndexedMesh::ReadTriangleSoup(std::istream &input)
{
    FaceIndexedMesh mesh;
    long long declaredTriangles;
    if (!(input >> declaredTriangles) || declaredTriangles < 0)
        throw std::runtime_error("Cannot read triangle count.");

    // Stored triangles are authoritative: hamish.tri holds more than its header claims.
    std::map<Vertex, std::size_t> vertexIds;
    for (;;)
    {
        Face face;
        for (std::size_t corner = 0; corner < face.size(); ++corner)
        {
            Vertex vertex;
            if (corner == 0)
            {
                // A clean end of file is only valid before a new triangle starts.
                if (!(input >> vertex[0]))
                {
                    if (input.eof()) return mesh;
                    throw std::runtime_error("Cannot read triangle coordinates.");
                }
                if (!(input >> vertex[1] >> vertex[2]))
                    throw std::runtime_error("Incomplete triangle coordinates.");
            }
            else if (!(input >> vertex[0] >> vertex[1] >> vertex[2]))
                throw std::runtime_error("Incomplete triangle coordinates.");

            const auto entry = vertexIds.emplace(vertex, mesh.vertices.size());
            if (entry.second) mesh.vertices.push_back(vertex);
            face[corner] = entry.first->second;
        }
        mesh.faces.push_back(face);
    }
}

void FaceIndexedMesh::WriteFace(std::ostream &output, const std::string &objectName) const
{
    output << "# University of Leeds 2024-25\n# COMP 5893M Assignment 1\n"
           << "# Nansong Yue\n# 201466811\n#\n"
           << "# Object Name: " << objectName << '\n'
           << "# Vertices=" << vertices.size() << " Faces=" << faces.size() << "\n#\n";
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t i = 0; i < vertices.size(); ++i)
        output << "Vertex " << i << ' ' << vertices[i][0] << ' '
               << vertices[i][1] << ' ' << vertices[i][2] << '\n';
    for (std::size_t i = 0; i < faces.size(); ++i)
        output << "Face " << i << ' ' << faces[i][0] << ' '
               << faces[i][1] << ' ' << faces[i][2] << '\n';
}

