#include "FaceIndexedMesh.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>

namespace
{
std::string WithoutExtension(const std::string &path)
{
    const std::size_t separator = path.find_last_of("/\\");
    const std::size_t nameStart = separator == std::string::npos ? 0 : separator + 1;
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && dot > nameStart)
        return path.substr(0, dot);
    return path;
}

std::string ObjectName(const std::string &path)
{
    const std::string stem = WithoutExtension(path);
    const std::size_t separator = stem.find_last_of("/\\");
    return stem.substr(separator == std::string::npos ? 0 : separator + 1);
}
}

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help"
                     || std::string(argv[1]) == "-h"))
    {
        std::cout << "Usage: face2faceindex input.tri [output.face]\n";
        return 0;
    }
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: face2faceindex input.tri [output.face]\n";
        return 1;
    }

    try
    {
        const std::string inputPath = argv[1];
        const std::string outputPath = argc == 3 ? argv[2]
                                                : WithoutExtension(inputPath) + ".face";
        if (inputPath == outputPath)
            throw std::runtime_error("Input and output paths must be different.");

        std::ifstream input(inputPath.c_str());
        input.imbue(std::locale::classic());
        if (!input.is_open())
            throw std::runtime_error("Cannot open input file: " + inputPath);
        const FaceIndexedMesh mesh = FaceIndexedMesh::ReadTriangleSoup(input);
        input.close();

        // Refuse existing readable targets, including aliases of the input.
        // This keeps conversion from overwriting a source via e.g. ./input.tri.
        std::ifstream existingOutput(outputPath.c_str());
        if (existingOutput.is_open())
            throw std::runtime_error("Output file already exists; choose a new path: "
                                     + outputPath);

        // Do not create or truncate an output until the input is fully checked.
        std::ofstream output(outputPath.c_str());
        output.imbue(std::locale::classic());
        if (!output.is_open())
            throw std::runtime_error("Cannot open output file: " + outputPath);
        mesh.WriteFace(output, ObjectName(inputPath));
        output.close();
        if (!output)
            throw std::runtime_error("Cannot finish writing output file: " + outputPath);

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
