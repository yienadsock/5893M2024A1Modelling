#include "DirectedEdgeMesh.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void Usage(std::ostream &output)
{
    output << "Usage: converter face2faceindex|tri2face input.tri [output.face]\n"
              "       converter faceindex2directededge|face2diredge input.face [output.diredge]\n";
}

// File name without directory or extension, used as the .face object name.
std::string FileStem(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && dot > start) return path.substr(start, dot - start);
    return path.substr(start);
}

// Replaces the input extension, e.g. models/cube.tri -> models/cube.face.
std::string OutputPath(const std::string &path, const std::string &suffix)
{
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        return path.substr(0, dot) + suffix;
    return path + suffix;
}

void CheckNewOutput(const std::string &path)
{
    if (std::ifstream(path.c_str()))
        throw std::runtime_error("Output file already exists; choose a new path.");
}

// Task I(a): triangle soup (.tri) -> face index (.face).
void ConvertTriangleSoup(const std::string &inputPath, const std::string &outputPath)
{
    std::ifstream input(inputPath.c_str());
    if (!input) throw std::runtime_error("Cannot open input file.");
    const FaceIndexedMesh mesh = FaceIndexedMesh::ReadTriangleSoup(input);

    CheckNewOutput(outputPath);
    std::ofstream output(outputPath.c_str());
    if (!output) throw std::runtime_error("Cannot open output file.");
    mesh.WriteFace(output, FileStem(inputPath));
    output.close();
    if (!output) throw std::runtime_error("Cannot finish writing output file.");
    std::cout << "Wrote " << outputPath << " (" << mesh.VertexCount()
              << " vertices, " << mesh.FaceCount() << " faces).\n";
}

// Task I(b): face index (.face) -> Appendix 2 directed edges (.diredge).
void ConvertFaceIndex(const std::string &inputPath, const std::string &outputPath)
{
    std::ifstream input(inputPath.c_str());
    if (!input) throw std::runtime_error("Cannot open input file.");
    const DirectedEdgeMesh mesh(FaceIndexedMesh::ReadFace(input));

    CheckNewOutput(outputPath);
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
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        Usage(std::cout);
        return 0;
    }
    if (argc < 3 || argc > 4)
    {
        Usage(std::cerr);
        return 1;
    }

    const std::string mode = argv[1];
    const std::string inputPath = argv[2];
    const bool toFace = mode == "face2faceindex" || mode == "tri2face";
    const bool toDirectedEdge = mode == "faceindex2directededge" || mode == "face2diredge";
    if (!toFace && !toDirectedEdge)
    {
        Usage(std::cerr);
        return 1;
    }

    const std::string outputPath = argc == 4 ? argv[3]
        : OutputPath(inputPath, toFace ? ".face" : ".diredge");
    try
    {
        if (toFace) ConvertTriangleSoup(inputPath, outputPath);
        else ConvertFaceIndex(inputPath, outputPath);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "converter: " << error.what() << '\n';
        return 1;
    }
}
