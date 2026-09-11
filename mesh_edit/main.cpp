#include "../converter/DirectedEdgeMesh.h"
#include "HoleFilling.h"
#include "TriangleSoupWriter.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{

void Usage(std::ostream &output)
{
    output << "Usage: mesh_edit repair input.tri|input.face|input.diredge [output]\n"
              "Closes every hole with a fan at the boundary centre of gravity and\n"
              "writes the repaired mesh to a new file in the input format.\n";
}

std::string Extension(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return std::string();
    std::string extension = path.substr(dot + 1);
    for (char &letter : extension)
        letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
    return extension;
}

std::string FileStem(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t start = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && dot > start) return path.substr(start, dot - start);
    return path.substr(start);
}

// Keeps the input extension so the original file is never overwritten.
std::string DefaultOutput(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + "_fixed";
    return path.substr(0, dot) + "_fixed" + path.substr(dot);
}

FaceIndexedMesh ReadMesh(const std::string &path)
{
    std::ifstream input(path.c_str());
    if (!input) throw std::runtime_error("cannot open input file");
    if (Extension(path) == "tri") return FaceIndexedMesh::ReadTriangleSoup(input);

    // .face and .diredge share their geometry records. Rebuild the Task I
    // connectivity so every supported input uses the same pairing convention.
    std::ostringstream geometry;
    std::string line, kind;
    while (std::getline(input, line))
    {
        std::istringstream record(line);
        record >> kind;
        if (kind != "FirstDirectedEdge" && kind != "OtherHalf")
            geometry << line << '\n';
    }
    std::istringstream indexed(geometry.str());
    return FaceIndexedMesh::ReadFace(indexed);
}

void WriteMesh(const FaceIndexedMesh &mesh, const std::string &path,
               const std::string &objectName, const std::string &format)
{
    if (std::ifstream(path.c_str()))
        throw std::runtime_error("output file already exists; choose a new path");
    std::ofstream output(path.c_str());
    if (!output) throw std::runtime_error("cannot open output file");
    if (format == "tri") WriteTriangleSoup(output, mesh);
    else if (format == "face") mesh.WriteFace(output, objectName);
    else if (format == "diredge") DirectedEdgeMesh(mesh).WriteDirectedEdge(output);
    else throw std::runtime_error("unsupported input extension");
    output.close();
    if (!output) throw std::runtime_error("cannot finish writing output file");
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        Usage(std::cout);
        return 0;
    }
    if (argc < 3 || argc > 4 || std::string(argv[1]) != "repair")
    {
        Usage(std::cerr);
        return 1;
    }

    const std::string inputPath = argv[2];
    const std::string outputPath = argc == 4 ? argv[3] : DefaultOutput(inputPath);
    if (outputPath == inputPath)
    {
        std::cerr << "mesh_edit: the output must differ from the input file\n";
        return 1;
    }

    try
    {
        const FaceIndexedMesh mesh = ReadMesh(inputPath);
        const HoleFilling repair(mesh);

        // The handout guarantees that only holes need repair, but report any
        // boundary edges that survive so a second repair can be attempted.
        const DirectedEdgeMesh check(repair.Mesh());
        if (check.UnpairedEdgeCount() != 0)
            std::cerr << "mesh_edit: warning: " << check.UnpairedEdgeCount()
                      << " unpaired edges remain\n";

        WriteMesh(repair.Mesh(), outputPath, FileStem(inputPath), Extension(inputPath));
        std::cout << "Filled " << repair.HoleCount() << " hole(s)";
        if (repair.SkippedLoopCount() != 0)
            std::cout << ", skipped " << repair.SkippedLoopCount()
                      << " non-simple boundary loop(s)";
        std::cout << "; wrote " << outputPath
                  << " (" << repair.Mesh().VertexCount() << " vertices, "
                  << repair.Mesh().FaceCount() << " faces).\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "mesh_edit: " << error.what() << '\n';
        return 1;
    }
}
