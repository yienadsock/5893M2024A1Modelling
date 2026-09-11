#ifndef MESH_REPAIR_H
#define MESH_REPAIR_H

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>

class DirectedEdgeMesh;

// Task IV: removes debris (detached components, fin slivers, flaps and
// pinches) and then closes every hole with a fan at its centre of gravity.
class MeshRepair
{
public:
    explicit MeshRepair(const FaceIndexedMesh &mesh);

    std::size_t RemovedFaceCount() const { return removedFaces; }
    std::size_t HoleCount() const { return holes; }
    const FaceIndexedMesh &Mesh() const { return repaired; }

private:
    // Deletes every face that stops the boundary from being a set of simple
    // loops; returns true when at least one face was removed.
    bool RemoveBadFaces();
    // Closes every simple hole; returns true when at least one was filled.
    bool FillHoles();

    FaceIndexedMesh repaired;
    std::size_t removedFaces;
    std::size_t holes;
};

#endif
