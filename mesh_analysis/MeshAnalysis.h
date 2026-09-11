#ifndef MESH_ANALYSIS_H
#define MESH_ANALYSIS_H

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>
#include <string>

class DirectedEdgeMesh;

// Tasks II and III of the assignment: decides whether a mesh is a closed
// manifold surface and, if it is, finds the genus from the Euler formula.
class MeshAnalysis
{
public:
    explicit MeshAnalysis(const FaceIndexedMesh &mesh);

    bool IsManifold() const { return failureText == "None"; }
    // "None" for a manifold mesh, otherwise the failing edge and vertex IDs.
    const std::string &Failure() const { return failureText; }
    // Sum of the component genera; only meaningful for a manifold mesh.
    std::size_t Genus() const { return surfaceGenus; }

private:
    // Both helpers read the private connectivity of DirectedEdgeMesh, which
    // therefore declares MeshAnalysis as a friend.
    std::string FindFailure(const DirectedEdgeMesh &connectivity) const;
    std::size_t ComputeGenus(const DirectedEdgeMesh &connectivity) const;

    std::string failureText;
    std::size_t surfaceGenus;
};

#endif
