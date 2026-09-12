#ifndef FACE_INDEXED_MESH_H
#define FACE_INDEXED_MESH_H

#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

// Shared vertices and ordered triangle indices, independent of the renderer.
class FaceIndexedMesh
{
public:
    typedef std::array<double, 3> Vertex;
    typedef std::array<std::size_t, 3> Face;

    // Throws std::runtime_error if the triangle soup is malformed.
    static FaceIndexedMesh ReadTriangleSoup(std::istream &input);
    // Implemented in DirectedEdgeMesh.cpp; reads the .face produced by step (a).
    static FaceIndexedMesh ReadFace(std::istream &input);
    void WriteFace(std::ostream &output, const std::string &objectName) const;

    std::size_t VertexCount() const { return vertices.size(); }
    std::size_t FaceCount() const { return faces.size(); }
    const std::vector<Vertex> &Vertices() const { return vertices; }
    const std::vector<Face> &Faces() const { return faces; }
    const std::vector<std::string> &HeaderLines() const { return headerLines; }

private:
    // Edited in place by MeshRepair (hole fans) and MeshSimplifier (re-triangulation).
    friend class MeshRepair;
    friend class MeshSimplifier;

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<std::string> headerLines;
};

#endif
