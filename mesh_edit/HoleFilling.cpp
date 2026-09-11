#include "HoleFilling.h"

#include "../converter/DirectedEdgeMesh.h"

#include <set>
#include <stdexcept>

HoleFilling::HoleFilling(const FaceIndexedMesh &mesh)
    : repaired(mesh), holes(0), skipped(0)
{
    // One pass finds every unpaired edge of the current connectivity and
    // groups them into boundary loops. Rebuilding afterwards repeats the
    // search, so loops created by this pass are also closed.
    for (std::size_t pass = 0; pass < 1000; ++pass)
    {
        const DirectedEdgeMesh connectivity(repaired);
        std::vector<char> used(connectivity.DirectedEdgeCount(), 0);
        bool filledAny = false;
        for (std::size_t edge = 0; edge < used.size(); ++edge)
        {
            if (connectivity.otherHalves[edge] != -1 || used[edge] != 0) continue;
            const std::vector<std::size_t> loop = BoundaryLoop(connectivity, used, edge);
            if (FillableLoop(connectivity, loop))
            {
                FillHole(connectivity, loop);
                ++holes;
                filledAny = true;
            }
            // Count the defects of the original mesh once; later passes only
            // re-visit loops this pass could not close.
            else if (pass == 0) ++skipped;
        }
        if (!filledAny) return;
    }
    throw std::runtime_error("hole filling did not converge");
}

bool HoleFilling::FillableLoop(const DirectedEdgeMesh &connectivity,
                               const std::vector<std::size_t> &loop) const
{
    if (loop.size() < 3) return false;
    std::set<std::size_t> heads;
    std::set<std::size_t> faces;
    for (std::size_t edge : loop)
    {
        heads.insert(connectivity.Endpoints(edge)[1]);
        faces.insert(edge / 3);
    }
    return heads.size() == loop.size() && faces.size() == loop.size();
}

std::vector<std::size_t> HoleFilling::BoundaryLoop(
    const DirectedEdgeMesh &connectivity, std::vector<char> &used, std::size_t start) const
{
    std::vector<std::size_t> loop;
    std::size_t edge = start;
    std::size_t budget = connectivity.otherHalves.size() + 1;
    do
    {
        if (budget-- == 0) throw std::runtime_error("boundary walk did not close");
        loop.push_back(edge);
        used[edge] = 1;

        // Rotate around the head vertex, crossing every paired edge, until
        // the next edge of the same hole boundary appears.
        std::size_t next = 3 * (edge / 3) + (edge % 3 + 1) % 3;
        std::size_t rotation = connectivity.otherHalves.size() + 1;
        while (connectivity.otherHalves[next] != -1)
        {
            if (rotation-- == 0)
                throw std::runtime_error("vertex fan did not reach the hole boundary");
            next = connectivity.otherHalves[next];
            next = 3 * (next / 3) + (next % 3 + 1) % 3;
        }
        edge = next;
    } while (edge != start);
    return loop;
}

void HoleFilling::FillHole(const DirectedEdgeMesh &connectivity,
                           const std::vector<std::size_t> &loop)
{
    // The head vertex of each boundary edge lies on the hole, so averaging
    // the heads gives the centre of gravity requested by the assignment.
    FaceIndexedMesh::Vertex centre = {};
    for (std::size_t edge : loop)
    {
        const DirectedEdgeMesh::EdgeKey ends = connectivity.Endpoints(edge);
        const FaceIndexedMesh::Vertex &point = repaired.vertices[ends[1]];
        centre[0] += point[0];
        centre[1] += point[1];
        centre[2] += point[2];
    }
    centre[0] /= loop.size();
    centre[1] /= loop.size();
    centre[2] /= loop.size();

    const std::size_t centreId = repaired.vertices.size();
    repaired.vertices.push_back(centre);

    // The fan face (head, tail, centre) contains the reverse of the boundary
    // edge (tail, head), so every new edge pairs with exactly one hole edge.
    for (std::size_t edge : loop)
    {
        const DirectedEdgeMesh::EdgeKey ends = connectivity.Endpoints(edge);
        repaired.faces.push_back(FaceIndexedMesh::Face{{ends[1], ends[0], centreId}});
    }
}
