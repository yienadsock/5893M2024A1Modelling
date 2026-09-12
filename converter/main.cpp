// Task I: the two little format converters. This file is deliberately loose;
// the file-format code it leans on (FaceIndexedMesh, DirectedEdgeMesh) is strict.

#include "DirectedEdgeMesh.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace
{

void usage(ostream &out)
{
    out << "Usage: converter face2faceindex|tri2face input.tri [output.face]\n"
           "       converter faceindex2directededge|face2diredge input.face [output.diredge]\n";
}

// "models/cube.tri" -> "cube", which is what goes in the .face header
string stem(const string &path)
{
    const size_t slash = path.find_last_of("/\\");
    const size_t start = slash == string::npos ? 0 : slash + 1;
    const size_t dot = path.find_last_of('.');
    if (dot != string::npos && dot > start) return path.substr(start, dot - start);
    return path.substr(start);
}

// models/cube.tri + ".face" -> models/cube.face
string swapped(const string &path, const string &suffix)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot != string::npos && (slash == string::npos || dot > slash))
        return path.substr(0, dot) + suffix;
    return path + suffix;
}

// don't clobber whatever is already sitting there
void mustBeNew(const string &path)
{
    if (ifstream(path.c_str())) throw runtime_error("Output file already exists; choose a new path.");
}

// task I(a): triangle soup in, face index out
void triToFace(const string &inPath, const string &outName)
{
    ifstream in(inPath.c_str());
    if (!in) throw runtime_error("Cannot open input file.");
    const auto mesh = FaceIndexedMesh::ReadTriangleSoup(in);

    mustBeNew(outName);
    ofstream out(outName.c_str());
    if (!out) throw runtime_error("Cannot open output file.");
    mesh.WriteFace(out, stem(inPath));
    out.close();
    if (!out) throw runtime_error("Cannot finish writing output file.");
    cout << "Wrote " << outName << " (" << mesh.VertexCount()
         << " vertices, " << mesh.FaceCount() << " faces).\n";
}

// task I(b): face index in, appendix 2 directed edges out
void faceToEdge(const string &inPath, const string &outName)
{
    ifstream in(inPath.c_str());
    if (!in) throw runtime_error("Cannot open input file.");
    const DirectedEdgeMesh mesh(FaceIndexedMesh::ReadFace(in));

    mustBeNew(outName);
    ofstream out(outName.c_str());
    if (!out) throw runtime_error("Cannot open output file.");
    mesh.WriteDirectedEdge(out);
    out.close();
    if (!out) throw runtime_error("Cannot finish writing output file.");
    mesh.WriteDiagnostics(cerr);
    cout << "Wrote " << outName << " (" << mesh.VertexCount()
         << " vertices, " << mesh.FaceCount() << " faces, "
         << mesh.DirectedEdgeCount() << " directed edges, "
         << mesh.UnpairedEdgeCount() << " unpaired).\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h"))
    {
        usage(cout);
        return 0;
    }
    if (argc < 3 || argc > 4)
    {
        usage(cerr);
        return 1;
    }

    const string mode = argv[1];
    const string inPath = argv[2];
    const bool toFace = mode == "face2faceindex" || mode == "tri2face";
    const bool toEdge = mode == "faceindex2directededge" || mode == "face2diredge";
    if (!toFace && !toEdge) { usage(cerr); return 1; }   // not a mode we know

    const string outName = argc == 4 ? argv[3]
        : swapped(inPath, toFace ? ".face" : ".diredge");
    try
    {
        if (toFace) triToFace(inPath, outName);
        else faceToEdge(inPath, outName);
        return 0;
    }
    catch (const exception &err)
    {
        cerr << "converter: " << err.what() << '\n';
        return 1;
    }
}
