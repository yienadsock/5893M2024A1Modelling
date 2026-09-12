#pragma once

#include "../converter/FaceIndexedMesh.h"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

// task V: greedy vertex decimation by Gaussian curvature, keeping V - E + F
class MeshSimplifier
{
public:
    explicit MeshSimplifier(const FaceIndexedMesh &mesh, double keep = 0.5);

    std::size_t gone() const { return deadVerts; }
    const FaceIndexedMesh &mesh() const { return thin; }

private:
    typedef std::pair<std::size_t, std::size_t> Edge;

    double triArea(std::size_t f) const;
    double angleAt(std::size_t v, const FaceIndexedMesh::Face &tri) const;
    double cotan(std::size_t at, std::size_t a, std::size_t b) const;
    // the 1-ring neighbours of an interior vertex, in face order
    std::vector<std::size_t> ringOf(std::size_t v) const;
    // fills in K and H from the faces currently around the vertex
    void curvature(std::size_t v);
    // removes a vertex; reports its old ring so the caller can refresh
    bool chop(std::size_t v, std::vector<std::size_t> &ring);

    FaceIndexedMesh thin;
    std::vector<char> liveFace;
    std::vector<char> liveVert;
    std::map<Edge, std::pair<std::size_t, std::size_t> > edgeFaces;
    std::vector<std::vector<std::size_t> > around;
    std::vector<double> K;
    std::vector<double> H;
    std::size_t deadVerts;
};
