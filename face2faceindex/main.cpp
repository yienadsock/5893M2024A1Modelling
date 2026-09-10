#include "FaceIndexedMesh.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

// Task I(a): convert a triangle soup (.tri) into the face index format.
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
