#ifndef DIRECTED_EDGE_MESH_H
#define DIRECTED_EDGE_MESH_H

#include "../face2faceindex/FaceIndexedMesh.h"

#include <array>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

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
    std::size_t UnpairedEdgeCount() const { return unpairedEdgeCount; }

private:
    typedef std::ptrdiff_t EdgeId;
    typedef std::array<std::size_t, 2> EdgeKey;

    struct UnpairedGroup
    {
        EdgeKey vertices;
        std::vector<std::size_t> edges;
        std::string reason;
    };

    FaceIndexedMesh mesh;
    std::vector<EdgeId> firstDirectedEdges;
    std::vector<EdgeId> otherHalves;
    std::vector<UnpairedGroup> unpairedGroups;
    std::size_t unpairedEdgeCount;
    std::size_t isolatedVertexCount;

    // Edge 3f+i goes from face[(i+2)%3] to face[i], as in the appendix.
    EdgeKey Endpoints(std::size_t edgeId) const;
    void BuildConnectivity();
};

#endif
