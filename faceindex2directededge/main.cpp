#include "DirectedEdgeMesh.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

// Task I(b): build the Appendix 2 records from a .face file.
int main(int argc, char **argv)
{
    const bool help = argc == 2 && (std::string(argv[1]) == "--help"
                                    || std::string(argv[1]) == "-h");
    if (help || argc < 2 || argc > 3)
    {
        (help ? std::cout : std::cerr)
            << "Usage: faceindex2directededge input.face [output.diredge]\n";
        return help ? 0 : 1;
    }
    try
    {
        std::string stem = argv[1];
        const std::size_t slash = stem.find_last_of("/\\");
        const std::size_t dot = stem.find_last_of('.');
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash + 1))
            stem.resize(dot);
        const std::string outputPath = argc == 3 ? argv[2] : stem + ".diredge";

        std::ifstream input(argv[1]);
        if (!input) throw std::runtime_error("Cannot open input file.");
        const DirectedEdgeMesh mesh(FaceIndexedMesh::ReadFace(input));
        if (std::ifstream(outputPath.c_str()))
            throw std::runtime_error("Output file already exists; choose a new path.");
        std::ofstream output(outputPath.c_str());
        if (!output) throw std::runtime_error("Cannot open output file.");
        mesh.WriteDirectedEdge(output);
        output.close();
        if (!output) throw std::runtime_error("Cannot finish writing output file.");
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
