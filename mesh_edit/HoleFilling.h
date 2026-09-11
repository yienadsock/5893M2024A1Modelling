#ifndef HOLE_FILLING_H
#define HOLE_FILLING_H

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>
#include <vector>

class DirectedEdgeMesh;

// Task IV: closes every boundary loop of a triangle mesh. Each hole gets one
// new vertex at the centre of gravity of its boundary vertices and a fan of
// triangles whose directed edges pair with the existing boundary edges.
class HoleFilling
{
public:
    explicit HoleFilling(const FaceIndexedMesh &mesh);

    std::size_t HoleCount() const { return holes; }
    std::size_t SkippedLoopCount() const { return skipped; }
    const FaceIndexedMesh &Mesh() const { return repaired; }

private:
    // Walks the directed edges of one hole, starting at an unpaired edge.
    std::vector<std::size_t> BoundaryLoop(const DirectedEdgeMesh &connectivity,
                                          std::vector<char> &used,
                                          std::size_t start) const;
    // A real hole is a loop of at least three edges, each from a different
    // face, whose boundary vertices are all distinct.
    bool FillableLoop(const DirectedEdgeMesh &connectivity,
                      const std::vector<std::size_t> &loop) const;
    // Adds the centre vertex and the fan of triangles that closes one loop.
    void FillHole(const DirectedEdgeMesh &connectivity,
                  const std::vector<std::size_t> &loop);

    FaceIndexedMesh repaired;
    std::size_t holes;
    std::size_t skipped;
};

#endif
