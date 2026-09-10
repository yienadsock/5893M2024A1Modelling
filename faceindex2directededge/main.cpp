#include "DirectedEdgeMesh.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>

namespace
{
std::string DefaultOutputPath(const std::string &inputPath)
{
    const std::size_t separator = inputPath.find_last_of("/\\");
    const std::size_t nameStart = separator == std::string::npos ? 0 : separator + 1;
    const std::size_t dot = inputPath.find_last_of('.');
    if (dot != std::string::npos && dot > nameStart)
        return inputPath.substr(0, dot) + ".diredge";
    return inputPath + ".diredge";
}
}

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help"
                     || std::string(argv[1]) == "-h"))
    {
        std::cout << "Usage: faceindex2directededge input.face [output.diredge]\n";
        return 0;
    }
    if (argc < 2 || argc > 3)
    {
        std::cerr << "Usage: faceindex2directededge input.face [output.diredge]\n";
        return 1;
    }

    try
    {
        const std::string inputPath = argv[1];
        const std::string outputPath = argc == 3 ? argv[2] : DefaultOutputPath(inputPath);
        if (inputPath == outputPath)
            throw std::runtime_error("Input and output paths must be different.");

        std::ifstream input(inputPath.c_str());
        input.imbue(std::locale::classic());
        if (!input.is_open())
            throw std::runtime_error("Cannot open input file: " + inputPath);
        const DirectedEdgeMesh mesh(FaceIndexedMesh::ReadFace(input));
        input.close();

        // Also catches readable aliases of the input, e.g. ./input.face.
        std::ifstream existingOutput(outputPath.c_str());
        if (existingOutput.is_open())
            throw std::runtime_error("Output file already exists; choose a new path: "
                                     + outputPath);

        // Parse and construct connectivity before creating the output file.
        std::ofstream output(outputPath.c_str());
        output.imbue(std::locale::classic());
        if (!output.is_open())
            throw std::runtime_error("Cannot open output file: " + outputPath);
        mesh.WriteDirectedEdge(output);
        output.close();
        if (!output)
            throw std::runtime_error("Cannot finish writing output file: " + outputPath);

        mesh.WriteDiagnostics(std::cerr);
        std::cout << "Wrote " << outputPath << " (" << mesh.VertexCount()
                  << " vertices, " << mesh.FaceCount() << " faces, "
                  << mesh.DirectedEdgeCount() << " directed edges, "
                  << mesh.UnpairedEdgeCount() << " unpaired).\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "faceindex2directededge: " << error.what() << '\n';
        return 1;
    }
}
