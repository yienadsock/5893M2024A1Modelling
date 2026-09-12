// task IV + V driver. the style here is loose on purpose: the format readers,
// the writers and the mesh data all sit in the strict code it calls.

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

using namespace std;

namespace
{

void usage(ostream &out)
{
    out << "Usage: mesh_edit repair input.tri|input.face|input.diredge [output]\n"
           "       mesh_edit simplify input.tri|input.face|input.diredge [output] [ratio]\n"
           "repair closes every hole with a fan at its centre of gravity; simplify\n"
           "removes the flattest vertices until the keep ratio (default 0.5) is\n"
           "reached. Both write a new file in the input format.\n";
}

string extensionOf(const string &path)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == string::npos || (slash != string::npos && dot < slash)) return string();
    string ext = path.substr(dot + 1);
    for (char &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

string stem(const string &path)
{
    const size_t slash = path.find_last_of("/\\");
    const size_t start = slash == string::npos ? 0 : slash + 1;
    const size_t dot = path.find_last_of('.');
    if (dot != string::npos && dot > start) return path.substr(start, dot - start);
    return path.substr(start);
}

// keep the input extension so the original file is never touched
string defaultOutput(const string &path, const string &suffix)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == string::npos || (slash != string::npos && dot < slash)) return path + suffix;
    return path.substr(0, dot) + suffix + path.substr(dot);
}

// grumble if an edit left holes behind; the input is meant to be closed
void warnIfOpen(const FaceIndexedMesh &mesh)
{
    const DirectedEdgeMesh check(mesh);
    if (check.UnpairedEdgeCount() != 0)
        cerr << "mesh_edit: warning: " << check.UnpairedEdgeCount() << " unpaired edges remain\n";
}

FaceIndexedMesh readMesh(const string &path)
{
    ifstream in(path.c_str());
    if (!in) throw runtime_error("cannot open input file");
    if (extensionOf(path) == "tri") return FaceIndexedMesh::ReadTriangleSoup(in);

    // .face and .diredge share the geometry records, so keep those lines and
    // let ReadFace rebuild the Task I connectivity
    ostringstream kept;
    string line, kind;
    while (getline(in, line))
    {
        istringstream record(line);
        record >> kind;
        if (kind != "FirstDirectedEdge" && kind != "OtherHalf") kept << line << '\n';
    }
    istringstream rebuilt(kept.str());
    return FaceIndexedMesh::ReadFace(rebuilt);
}

void writeMesh(const FaceIndexedMesh &mesh, const string &path,
               const string &name, const string &format)
{
    if (ifstream(path.c_str()))
        throw runtime_error("output file already exists; choose a new path");
    ofstream out(path.c_str());
    if (!out) throw runtime_error("cannot open output file");
    if (format == "tri") WriteTriangleSoup(out, mesh);
    else if (format == "face") mesh.WriteFace(out, name);
    else if (format == "diredge") DirectedEdgeMesh(mesh).WriteDirectedEdge(out);
    else throw runtime_error("unsupported input extension");
    out.close();
    if (!out) throw runtime_error("cannot finish writing output file");
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h"))
    {
        usage(cout);
        return 0;
    }
    if (argc < 3 || argc > 5
        || (string(argv[1]) != "repair" && string(argv[1]) != "simplify"))
    {
        usage(cerr);
        return 1;
    }

    const string mode = argv[1];
    const bool repair = mode == "repair";
    if (repair && argc > 4) { usage(cerr); return 1; }   // repair takes no ratio
    const string inPath = argv[2];
    const string outPath = argc >= 4 ? argv[3]
        : defaultOutput(inPath, repair ? "_fixed" : "_simplified");
    double keep = 0.5;
    if (!repair && argc == 5)
    {
        keep = strtod(argv[4], 0);
        if (keep <= 0.0 || keep > 1.0)
        {
            cerr << "mesh_edit: the ratio must be between 0 and 1\n";
            return 1;
        }
    }
    if (outPath == inPath)
    {
        cerr << "mesh_edit: the output must differ from the input file\n";
        return 1;
    }

    try
    {
        const FaceIndexedMesh mesh = readMesh(inPath);
        if (repair)
        {
            const MeshRepair fix(mesh);
            warnIfOpen(fix.mesh());
            writeMesh(fix.mesh(), outPath, stem(inPath), extensionOf(inPath));
            cout << "Removed " << fix.dropped() << " face(s); filled " << fix.filled()
                 << " hole(s); wrote " << outPath << " (" << fix.mesh().VertexCount()
                 << " vertices, " << fix.mesh().FaceCount() << " faces).\n";
        }
        else
        {
            const MeshSimplifier thin(mesh, keep);
            warnIfOpen(thin.mesh());
            writeMesh(thin.mesh(), outPath, stem(inPath), extensionOf(inPath));
            cout << "Simplified from " << mesh.VertexCount() << "/" << mesh.FaceCount()
                 << " to " << thin.mesh().VertexCount() << "/" << thin.mesh().FaceCount()
                 << " vertices/faces (removed " << thin.gone()
                 << " vertices); wrote " << outPath << '\n';
        }
        return 0;
    }
    catch (const exception &err)
    {
        cerr << "mesh_edit: " << err.what() << '\n';
        return 1;
    }
}
