#ifndef MESH_SIMPLIFIER_H
#define MESH_SIMPLIFIER_H

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

// Task V: greedy vertex decimation ordered by the smallest Gaussian
// curvature. Every removal re-triangulates the 1-ring and checks the
// Eulerian condition so the surface stays a closed manifold. The mean
// curvature is computed alongside for completeness.
class MeshSimplifier
{
public:
    explicit MeshSimplifier(const FaceIndexedMesh &mesh, double keepRatio = 0.5);

    std::size_t RemovedVertexCount() const { return removedVertices; }
    const FaceIndexedMesh &Mesh() const { return simplified; }

private:
    typedef std::pair<std::size_t, std::size_t> Edge;

    double FaceArea(std::size_t face) const;
    double Angle(std::size_t vertex, const FaceIndexedMesh::Face &face) const;
    double Cotangent(std::size_t at, std::size_t first, std::size_t second) const;
    // The 1-ring neighbours of an interior vertex, in face order.
    std::vector<std::size_t> Ring(std::size_t vertex) const;
    // Mean and Gaussian curvature from the current incident faces.
    void UpdateCurvature(std::size_t vertex);
    // Tries to remove a vertex; on success mutates the mesh and returns the
    // old 1-ring so the caller can refresh the curvatures.
    bool TryRemove(std::size_t vertex, std::vector<std::size_t> &ring);

    FaceIndexedMesh simplified;
    std::vector<char> aliveFace;
    std::vector<char> aliveVertex;
    std::map<Edge, std::pair<std::size_t, std::size_t> > edgeFaces;
    std::vector<std::vector<std::size_t> > incident;
    std::vector<double> gaussian;
    std::vector<double> mean;
    std::size_t removedVertices;
};

#endif
