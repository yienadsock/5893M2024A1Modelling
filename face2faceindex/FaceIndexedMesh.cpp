#include "FaceIndexedMesh.h"

#include <cmath>
#include <functional>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
struct VertexHash
{
    std::size_t operator()(const FaceIndexedMesh::Vertex &vertex) const
    {
        std::size_t result = 0;
        for (std::size_t axis = 0; axis < vertex.size(); ++axis)
        {
            // std::hash<double> treats equal values, including +/-0, equally.
            const std::size_t component = std::hash<double>()(vertex[axis]);
            result ^= component + 0x9e3779b9U + (result << 6) + (result >> 2);
        }
        return result;
    }
};

std::size_t ReadTriangleCount(std::istream &input)
{
    std::string token;
    if (!(input >> token))
        throw std::runtime_error("Missing triangle count.");

    std::size_t count = 0;
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < token.size(); ++i)
    {
        if (token[i] < '0' || token[i] > '9')
            throw std::runtime_error("Triangle count must be a non-negative integer.");

        const std::size_t digit = static_cast<std::size_t>(token[i] - '0');
        if (count > (maximum - digit) / 10)
            throw std::runtime_error("Triangle count is too large.");
        count = count * 10 + digit;
    }
    return count;
}

bool ReadCoordinate(std::istream &input, double &coordinate)
{
    std::string token;
    if (!(input >> token))
        return false;

    // A coordinate must occupy one complete whitespace-delimited token.
    // Direct double extraction would incorrectly split "0-1" into two values.
    std::istringstream number(token);
    number.imbue(std::locale::classic());
    return (number >> coordinate)
        && number.peek() == std::char_traits<char>::eof()
        && std::isfinite(coordinate);
}
}

FaceIndexedMesh FaceIndexedMesh::ReadTriangleSoup(std::istream &input)
{
    FaceIndexedMesh mesh;
    const std::size_t triangleCount = ReadTriangleCount(input);
    if (triangleCount > mesh.faces.max_size()
        || triangleCount > mesh.vertices.max_size() / 3)
        throw std::runtime_error("Triangle count exceeds the supported mesh size.");

    std::unordered_map<Vertex, std::size_t, VertexHash> vertexIds;
    // Grow with actual input instead of allocating from an untrusted count.
    for (std::size_t faceId = 0; faceId < triangleCount; ++faceId)
    {
        Face face;
        for (std::size_t corner = 0; corner < face.size(); ++corner)
        {
            Vertex vertex;
            for (std::size_t axis = 0; axis < vertex.size(); ++axis)
            {
                if (!ReadCoordinate(input, vertex[axis]))
                    throw std::runtime_error(
                        "Missing, invalid or non-finite coordinate at face "
                        + std::to_string(faceId) + ", corner "
                        + std::to_string(corner) + ", axis "
                        + std::to_string(axis) + ".");
            }

            const auto entry = vertexIds.emplace(vertex, mesh.vertices.size());
            if (entry.second)
                mesh.vertices.push_back(vertex);
            face[corner] = entry.first->second;
        }
        // Preserve both face order and winding; topology repair is a later task.
        mesh.faces.push_back(face);
    }

    std::string extra;
    if (input >> extra)
        throw std::runtime_error("Unexpected data after the declared triangles.");
    if (input.bad())
        throw std::runtime_error("Failed while reading the input file.");
    return mesh;
}

void FaceIndexedMesh::WriteFace(std::ostream &output,
                                const std::string &objectName) const
{
    std::string headerName = objectName;
    for (std::size_t i = 0; i < headerName.size(); ++i)
        if (headerName[i] == '\r' || headerName[i] == '\n')
            headerName[i] = ' ';

    // Replace the two identity placeholders with your submission details.
    output << "# University of Leeds 2024-25\n"
           << "# COMP 5893M Assignment 1\n"
           << "# Your Name Goes Here\n"
           << "# Your Student Number Goes Here\n"
           << "#\n"
           << "# Object Name: " << headerName << '\n'
           << "# Vertices=" << vertices.size() << " Faces=" << faces.size()
           << "\n#\n";

    // Enough significant digits to recover the same double on the next read.
    output << std::defaultfloat
           << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t vertexId = 0; vertexId < vertices.size(); ++vertexId)
    {
        const Vertex &vertex = vertices[vertexId];
        output << "Vertex " << vertexId << ' ' << vertex[0] << ' '
               << vertex[1] << ' ' << vertex[2] << '\n';
    }
    for (std::size_t faceId = 0; faceId < faces.size(); ++faceId)
    {
        const Face &face = faces[faceId];
        output << "Face " << faceId << ' ' << face[0] << ' '
               << face[1] << ' ' << face[2] << '\n';
    }
    if (!output)
        throw std::runtime_error("Failed while writing the face file.");
}
