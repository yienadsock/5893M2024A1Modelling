#ifndef MESH_REPAIR_H
#define MESH_REPAIR_H

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>

class DirectedEdgeMesh;

// Task IV: removes debris (components, fins, flaps, pinches), then closes holes.
class MeshRepair
{
public:
    explicit MeshRepair(const FaceIndexedMesh &mesh);

    std::size_t RemovedFaceCount() const { return removedFaces; }
    std::size_t HoleCount() const { return holes; }
    const FaceIndexedMesh &Mesh() const { return repaired; }

private:
    // Deletes faces that stop the boundary being simple loops; true if any went.
    bool RemoveBadFaces();
    // Closes every simple hole; returns true when at least one was filled.
    bool FillHoles();

    FaceIndexedMesh repaired;
    std::size_t removedFaces;
    std::size_t holes;
};

#endif
