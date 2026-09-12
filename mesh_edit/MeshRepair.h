#pragma once

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>

class DirectedEdgeMesh;

// task IV: sweep out the junk (components, fins, flaps, pinches), then patch holes
class MeshRepair
{
public:
    explicit MeshRepair(const FaceIndexedMesh &mesh);

    std::size_t dropped() const { return droppedFaces; }
    std::size_t filled() const { return filledHoles; }
    const FaceIndexedMesh &mesh() const { return fixed; }

private:
    // marks faces that would break the boundary loops; true if any went
    bool trim();
    // fans off every simple hole it can find; true if any got patched
    bool patch();

    FaceIndexedMesh fixed;
    std::size_t droppedFaces;
    std::size_t filledHoles;
};
