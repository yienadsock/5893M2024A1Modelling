#include "../face2faceindex/FaceIndexedMesh.h"

#include <cmath>
#include <istream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace
{
std::size_t ParseIndex(const std::string &token)
{
    if (token.empty())
        throw std::runtime_error("Missing count or index.");

    std::size_t value = 0;
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < token.size(); ++i)
    {
        if (token[i] < '0' || token[i] > '9')
            throw std::runtime_error("Counts and indices must be non-negative integers.");
        const std::size_t digit = static_cast<std::size_t>(token[i] - '0');
        if (value > (maximum - digit) / 10)
            throw std::runtime_error("Count or index is too large.");
        value = value * 10 + digit;
    }
    return value;
}

std::size_t ReadIndex(std::istream &input)
{
    std::string token;
    input >> token;
    return ParseIndex(token);
}

double ReadCoordinate(std::istream &input)
{
    std::string token;
    input >> token;
    std::istringstream number(token);
    number.imbue(std::locale::classic());
    double coordinate = 0;
    if (!(number >> coordinate)
        || number.peek() != std::char_traits<char>::eof()
        || !std::isfinite(coordinate))
        throw std::runtime_error("Missing, invalid or non-finite vertex coordinate.");
    return coordinate;
}

void RequireEndOfLine(std::istream &input)
{
    std::string extra;
    if (input >> extra)
        throw std::runtime_error("Unexpected extra field in face file.");
}
}

FaceIndexedMesh FaceIndexedMesh::ReadFace(std::istream &input)
{
    FaceIndexedMesh mesh;
    std::size_t vertexCount = 0;
    std::size_t faceCount = 0;
    bool haveCounts = false;
    bool dataStarted = false;
    std::string line;
    while (std::getline(input, line))
    {
        // Normalize Windows line endings while retaining the header text.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const std::size_t start = line.find_first_not_of(" \t\r\n\f\v");
        if (start == std::string::npos)
            continue;

        if (line[start] == '#')
        {
            if (dataStarted)
                throw std::runtime_error("Header comments must precede the vertex block.");
            mesh.headerLines.push_back(line);
            std::istringstream header(line.substr(start + 1));
            header.imbue(std::locale::classic());
            std::string verticesField;
            header >> verticesField;
            if (verticesField.compare(0, 9, "Vertices=") == 0)
            {
                if (haveCounts)
                    throw std::runtime_error("Duplicate vertex/face count header.");
                vertexCount = ParseIndex(verticesField.substr(9));
                std::string facesField;
                if (!(header >> facesField)
                    || facesField.compare(0, 6, "Faces=") != 0)
                    throw std::runtime_error("Missing Faces= count in header.");
                faceCount = ParseIndex(facesField.substr(6));
                RequireEndOfLine(header);
                if (vertexCount > mesh.vertices.max_size()
                    || faceCount > mesh.faces.max_size())
                    throw std::runtime_error("Counts exceed the supported mesh size.");
                haveCounts = true;
            }
            continue;
        }

        if (!haveCounts)
            throw std::runtime_error("Missing vertex/face count header.");
        dataStarted = true;
        std::istringstream record(line);
        record.imbue(std::locale::classic());
        std::string kind;
        record >> kind;
        if (kind == "Vertex")
        {
            if (!mesh.faces.empty() || mesh.vertices.size() >= vertexCount)
                throw std::runtime_error("Unexpected vertex outside the vertex block.");
            if (ReadIndex(record) != mesh.vertices.size())
                throw std::runtime_error("Vertex IDs must be consecutive, starting at zero.");
            Vertex vertex;
            for (std::size_t axis = 0; axis < vertex.size(); ++axis)
                vertex[axis] = ReadCoordinate(record);
            RequireEndOfLine(record);
            // Keep duplicate positions: .face IDs already define the topology.
            mesh.vertices.push_back(vertex);
        }
        else if (kind == "Face")
        {
            if (mesh.vertices.size() != vertexCount || mesh.faces.size() >= faceCount)
                throw std::runtime_error("Unexpected face or incomplete vertex block.");
            if (ReadIndex(record) != mesh.faces.size())
                throw std::runtime_error("Face IDs must be consecutive, starting at zero.");
            Face face;
            for (std::size_t corner = 0; corner < face.size(); ++corner)
            {
                face[corner] = ReadIndex(record);
                if (face[corner] >= vertexCount)
                    throw std::runtime_error("Face vertex index is out of range.");
            }
            RequireEndOfLine(record);
            mesh.faces.push_back(face);
        }
        else
            throw std::runtime_error("Unknown face-file record: " + kind);
    }

    if (input.bad() || (!input.eof() && input.fail()))
        throw std::runtime_error("Failed while reading the face file.");
    if (!haveCounts)
        throw std::runtime_error("Missing vertex/face count header.");
    if (mesh.vertices.size() != vertexCount || mesh.faces.size() != faceCount)
        throw std::runtime_error("Vertex/face counts do not match the header.");
    return mesh;
}
