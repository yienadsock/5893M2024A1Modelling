#include "../converter/DirectedEdgeMesh.h"
#include "MeshRepair.h"
#include "MeshSimplifier.h"
#include "TriangleSoupWriter.h"

#include <cctype>
#include <cstdlib>
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
              "       mesh_edit simplify input.tri|input.face|input.diredge [output] [ratio]\n"
              "repair closes every hole with a fan at its centre of gravity; simplify\n"
              "removes the flattest vertices until the keep ratio (default 0.5) is\n"
              "reached. Both write a new file in the input format.\n";
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
std::string DefaultOutput(const std::string &path, const std::string &suffix)
{
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + suffix;
    return path.substr(0, dot) + suffix + path.substr(dot);
}

// Reports boundary edges that survive an edit; the input is expected closed.
void WarnIfOpen(const FaceIndexedMesh &mesh)
{
    const DirectedEdgeMesh check(mesh);
    if (check.UnpairedEdgeCount() != 0)
        std::cerr << "mesh_edit: warning: " << check.UnpairedEdgeCount()
                  << " unpaired edges remain\n";
}

FaceIndexedMesh ReadMesh(const std::string &path)
{
    std::ifstream input(path.c_str());
    if (!input) throw std::runtime_error("cannot open input file");
    if (Extension(path) == "tri") return FaceIndexedMesh::ReadTriangleSoup(input);

    // .face and .diredge share their geometry records; rebuild the Task I connectivity.
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
    if (argc < 3 || argc > 5
        || (std::string(argv[1]) != "repair" && std::string(argv[1]) != "simplify"))
    {
        Usage(std::cerr);
        return 1;
    }

    const std::string mode = argv[1];
    const bool repair = mode == "repair";
    if (repair && argc > 4)
    {
        Usage(std::cerr);
        return 1;
    }
    const std::string inputPath = argv[2];
    const std::string outputPath = argc >= 4 ? argv[3]
        : DefaultOutput(inputPath, repair ? "_fixed" : "_simplified");
    double keepRatio = 0.5;
    if (!repair && argc == 5)
    {
        keepRatio = std::strtod(argv[4], 0);
        if (keepRatio <= 0.0 || keepRatio > 1.0)
        {
            std::cerr << "mesh_edit: the ratio must be between 0 and 1\n";
            return 1;
        }
    }
    if (outputPath == inputPath)
    {
        std::cerr << "mesh_edit: the output must differ from the input file\n";
        return 1;
    }

    try
    {
        const FaceIndexedMesh mesh = ReadMesh(inputPath);
        if (repair)
        {
            const MeshRepair repair(mesh);
            WarnIfOpen(repair.Mesh());
            WriteMesh(repair.Mesh(), outputPath, FileStem(inputPath), Extension(inputPath));
            std::cout << "Removed " << repair.RemovedFaceCount()
                      << " face(s); filled " << repair.HoleCount() << " hole(s)";
            std::cout << "; wrote " << outputPath
                      << " (" << repair.Mesh().VertexCount() << " vertices, "
                      << repair.Mesh().FaceCount() << " faces).\n";
        }
        else
        {
            const MeshSimplifier simplify(mesh, keepRatio);
            WarnIfOpen(simplify.Mesh());
            WriteMesh(simplify.Mesh(), outputPath, FileStem(inputPath), Extension(inputPath));
            std::cout << "Simplified from " << mesh.VertexCount() << "/"
                      << mesh.FaceCount() << " to " << simplify.Mesh().VertexCount()
                      << "/" << simplify.Mesh().FaceCount()
                      << " vertices/faces (removed " << simplify.RemovedVertexCount()
                      << " vertices); wrote " << outputPath << '\n';
        }
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "mesh_edit: " << error.what() << '\n';
        return 1;
    }
}
