#ifndef DIRECTED_EDGE_MESH_H
#define DIRECTED_EDGE_MESH_H

#include "FaceIndexedMesh.h"

#include <algorithm>

// Appendix 2 connectivity, retaining the input header, vertices and faces.
class DirectedEdgeMesh
{
public:
    explicit DirectedEdgeMesh(FaceIndexedMesh inputMesh);

    void WriteDirectedEdge(std::ostream &output) const;
    void WriteDiagnostics(std::ostream &output) const;

    std::size_t VertexCount() const { return mesh.VertexCount(); }
    std::size_t FaceCount() const { return mesh.FaceCount(); }
    std::size_t DirectedEdgeCount() const { return otherHalves.size(); }
    std::size_t UnpairedEdgeCount() const
    { return std::count(otherHalves.begin(), otherHalves.end(), -1); }

private:
    // Tasks II and III analyse this connectivity in mesh_analysis/MeshAnalysis.cpp.
    friend class MeshAnalysis;
    // Task IV walks the hole boundary loops in mesh_edit/HoleFilling.cpp.
    friend class HoleFilling;

    typedef std::ptrdiff_t EdgeId;
    typedef std::array<std::size_t, 2> EdgeKey;

    FaceIndexedMesh mesh;
    std::vector<EdgeId> firstDirectedEdges;
    std::vector<EdgeId> otherHalves;

    // Edge 3f+i goes from face[(i+2)%3] to face[i], as in the appendix.
    EdgeKey Endpoints(std::size_t edgeId) const;
    void BuildConnectivity();
};

#endif
