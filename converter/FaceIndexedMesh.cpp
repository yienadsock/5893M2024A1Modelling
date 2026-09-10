#include "FaceIndexedMesh.h"

#include <iomanip>
#include <limits>
#include <map>
#include <stdexcept>

FaceIndexedMesh FaceIndexedMesh::ReadTriangleSoup(std::istream &input)
{
    FaceIndexedMesh mesh;
    long long triangleCount;
    if (!(input >> triangleCount) || triangleCount < 0)
        throw std::runtime_error("Cannot read triangle count.");

    // The map finds shared coordinates; IDs follow first occurrence in the file.
    std::map<Vertex, std::size_t> vertexIds;
    for (long long f = 0; f < triangleCount; ++f)
    {
        Face face;
        for (std::size_t &index : face)
        {
            Vertex vertex;
            if (!(input >> vertex[0] >> vertex[1] >> vertex[2]))
                throw std::runtime_error("Incomplete triangle coordinates.");
            const auto entry = vertexIds.emplace(vertex, mesh.vertices.size());
            if (entry.second) mesh.vertices.push_back(vertex);
            index = entry.first->second;
        }
        mesh.faces.push_back(face);
    }
    // Keep this check: the supplied hamish.tri has an incorrect face count.
    input >> std::ws;
    if (!input.eof())
        throw std::runtime_error("Triangle count does not match the data.");
    return mesh;
}

void FaceIndexedMesh::WriteFace(std::ostream &output, const std::string &objectName) const
{
    output << "# University of Leeds 2024-25\n# COMP 5893M Assignment 1\n"
           << "# Your Name Goes Here\n# Your Student Number Goes Here\n#\n"
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

