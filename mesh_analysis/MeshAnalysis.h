#pragma once

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>
#include <string>

class DirectedEdgeMesh;

// tasks II + III: manifold or not, and the genus if it is
class MeshAnalysis
{
public:
    explicit MeshAnalysis(const FaceIndexedMesh &mesh);

    bool manifold() const { return bad == "None"; }
    const std::string &why() const { return bad; }    // failure text, or "None"
    std::size_t genus() const { return g; }           // summed over all pieces

private:
    // both read the private connectivity of the mesh we build ourselves
    std::string check(const DirectedEdgeMesh &c) const;
    std::size_t euler(const DirectedEdgeMesh &c) const;

    std::string bad;
    std::size_t g;
};
