#include "MeshAnalysis.h"

#include "../converter/DirectedEdgeMesh.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace
{

typedef std::vector<std::pair<std::size_t, std::size_t> > LinkEdges;

// True when a vertex link is neither one path (boundary) nor one cycle.
// An isolated vertex has an empty link, and a bow-tie has two separate fans.
bool LinkFails(const LinkEdges &edges)
{
    if (edges.empty()) return true;

    std::map<std::size_t, std::vector<std::size_t> > link;
    for (const auto &edge : edges)
    {
        link[edge.first].push_back(edge.second);
        link[edge.second].push_back(edge.first);
    }

    // A manifold vertex has degree 1 or 2 everywhere, with zero ends for an
    // interior cycle and exactly two ends for a boundary path.
    std::size_t ends = 0;
    for (const auto &entry : link)
    {
        if (entry.second.size() == 1) ++ends;
        else if (entry.second.size() != 2) return true;
    }
    if (ends != 0 && ends != 2) return true;

    // Every link edge must be reachable from the first neighbour: two fans
    // meeting at one vertex form a non-manifold bow-tie.
    std::vector<std::size_t> pending(1, link.begin()->first);
    std::map<std::size_t, bool> visited;
    while (!pending.empty())
    {
        const std::size_t neighbour = pending.back();
        pending.pop_back();
        if (visited[neighbour]) continue;
        visited[neighbour] = true;
        for (std::size_t next : link[neighbour]) pending.push_back(next);
    }
    return visited.size() != link.size();
}

// Union-find over faces, used to split the mesh into connected components.
std::size_t FindRoot(std::vector<std::size_t> &parent, std::size_t face)
{
    while (parent[face] != face)
    {
        parent[face] = parent[parent[face]];
        face = parent[face];
    }
    return face;
}

void JoinFaces(std::vector<std::size_t> &parent, std::size_t first, std::size_t second)
{
    const std::size_t firstRoot = FindRoot(parent, first);
    const std::size_t secondRoot = FindRoot(parent, second);
    if (firstRoot != secondRoot) parent[secondRoot] = firstRoot;
}

} // namespace

MeshAnalysis::MeshAnalysis(const FaceIndexedMesh &mesh)
    : failureText("None"), surfaceGenus(0)
{
    const DirectedEdgeMesh connectivity(mesh);
    failureText = FindFailure(connectivity);
    if (failureText == "None") surfaceGenus = ComputeGenus(connectivity);
}

std::string MeshAnalysis::FindFailure(const DirectedEdgeMesh &connectivity) const
{
    if (connectivity.mesh.FaceCount() == 0)
        return "failing edge: the mesh has no faces";

    std::string failure;

    // Task II: a closed surface pairs every directed edge with its opposite
    // half, so the first unpaired edge identifies a hole in the mesh.
    for (std::size_t edge = 0; edge < connectivity.otherHalves.size(); ++edge)
    {
        if (connectivity.otherHalves[edge] != -1) continue;
        const DirectedEdgeMesh::EdgeKey ends = connectivity.Endpoints(edge);
        failure = "failing edge " + std::to_string(edge) + " (vertices "
                + std::to_string(ends[0]) + " " + std::to_string(ends[1]) + ")";
        break;
    }

    // Task II: collect the link of every vertex and report the first vertex
    // whose link is not a single path or cycle.
    std::vector<LinkEdges> links(connectivity.mesh.VertexCount());
    for (const FaceIndexedMesh::Face &face : connectivity.mesh.Faces())
    {
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            links[face[corner]].push_back(std::make_pair(face[(corner + 1) % 3],
                                                         face[(corner + 2) % 3]));
        }
    }
    for (std::size_t vertex = 0; vertex < links.size(); ++vertex)
    {
        if (!LinkFails(links[vertex])) continue;
        if (!failure.empty()) failure += ", ";
        failure += "failing vertex " + std::to_string(vertex);
        break;
    }

    return failure.empty() ? "None" : failure;
}

std::size_t MeshAnalysis::ComputeGenus(const DirectedEdgeMesh &connectivity) const
{
    // Task III: each closed component satisfies V - E + F = 2 - 2g. Faces are
    // grouped into components by the pairing of their directed edges.
    std::vector<std::size_t> parent(connectivity.mesh.FaceCount());
    for (std::size_t face = 0; face < parent.size(); ++face) parent[face] = face;
    for (std::size_t edge = 0; edge < connectivity.otherHalves.size(); ++edge)
    {
        if (connectivity.otherHalves[edge] != -1)
            JoinFaces(parent, edge / 3, connectivity.otherHalves[edge] / 3);
    }

    std::map<std::size_t, std::size_t> componentOfRoot;
    std::vector<std::size_t> faceCount;
    std::vector<std::vector<bool> > vertexUsed;
    for (std::size_t face = 0; face < connectivity.mesh.FaceCount(); ++face)
    {
        const std::size_t root = FindRoot(parent, face);
        std::map<std::size_t, std::size_t>::iterator entry = componentOfRoot.find(root);
        if (entry == componentOfRoot.end())
        {
            const std::size_t index = faceCount.size();
            entry = componentOfRoot.insert(std::make_pair(root, index)).first;
            faceCount.push_back(0);
            vertexUsed.push_back(std::vector<bool>(connectivity.mesh.VertexCount(), false));
        }
        const std::size_t component = entry->second;
        ++faceCount[component];
        for (std::size_t vertex : connectivity.mesh.Faces()[face])
            vertexUsed[component][vertex] = true;
    }

    std::ptrdiff_t genus = 0;
    for (std::size_t component = 0; component < faceCount.size(); ++component)
    {
        const std::ptrdiff_t faces = static_cast<std::ptrdiff_t>(faceCount[component]);
        // A closed component has two faces per edge, so E = 3F / 2.
        const std::ptrdiff_t edges = 3 * faces / 2;
        const std::ptrdiff_t vertices = static_cast<std::ptrdiff_t>(
            std::count(vertexUsed[component].begin(), vertexUsed[component].end(), true));
        genus += (2 - (vertices - edges + faces)) / 2;
    }
    return static_cast<std::size_t>(genus);
}
