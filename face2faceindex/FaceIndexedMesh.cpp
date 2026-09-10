#include "FaceIndexedMesh.h"

#include <fstream>
#include <iomanip>
#include <iostream>
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

#ifndef TASK1_LIBRARY
int main(int argc, char **argv)
{
    const bool help = argc == 2 && (std::string(argv[1]) == "--help"
                                    || std::string(argv[1]) == "-h");
    if (help || argc < 2 || argc > 3)
    {
        (help ? std::cout : std::cerr) << "Usage: face2faceindex input.tri [output.face]\n";
        return help ? 0 : 1;
    }
    try
    {
        std::string stem = argv[1];
        const std::size_t slash = stem.find_last_of("/\\");
        const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
        const std::size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos && dot > start) stem.resize(dot);
        const std::string outputPath = argc == 3 ? argv[2] : stem + ".face";

        std::ifstream input(argv[1]);
        if (!input) throw std::runtime_error("Cannot open input file.");
        const FaceIndexedMesh mesh = FaceIndexedMesh::ReadTriangleSoup(input);
        if (std::ifstream(outputPath.c_str()))
            throw std::runtime_error("Output file already exists; choose a new path.");
        std::ofstream output(outputPath.c_str());
        if (!output) throw std::runtime_error("Cannot open output file.");
        mesh.WriteFace(output, stem.substr(start));
        output.close();
        if (!output) throw std::runtime_error("Cannot finish writing output file.");
        std::cout << "Wrote " << outputPath << " (" << mesh.VertexCount()
                  << " vertices, " << mesh.FaceCount() << " faces).\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "face2faceindex: " << error.what() << '\n';
        return 1;
    }
}
#endif
