#include "MeshAnalysis.h"

#include "../converter/DirectedEdgeMesh.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

using namespace std;

namespace
{

typedef vector<pair<size_t, size_t> > Links;   // one link edge per pair

// a vertex is bad unless its link is one path (boundary) or one cycle
bool linkBad(const Links &edges)
{
    if (edges.empty()) return true;

    map<size_t, vector<size_t> > link;
    for (const auto &e : edges)
    {
        link[e.first].push_back(e.second);
        link[e.second].push_back(e.first);
    }

    // degree 1 at the two ends, 2 everywhere else, nothing else allowed
    size_t ends = 0;
    for (const auto &entry : link)
    {
        if (entry.second.size() == 1) ++ends;
        else if (entry.second.size() != 2) return true;
    }
    if (ends != 0 && ends != 2) return true;

    // walk the link: if some of it can't be reached, two fans share the vertex
    vector<size_t> todo(1, link.begin()->first);
    map<size_t, bool> seen;
    while (!todo.empty())
    {
        const size_t v = todo.back();
        todo.pop_back();
        if (seen[v]) continue;
        seen[v] = true;
        for (size_t next : link[v]) todo.push_back(next);
    }
    return seen.size() != link.size();
}

size_t rootOf(vector<size_t> &parent, size_t face)
{
    while (parent[face] != face)
    {
        parent[face] = parent[parent[face]];
        face = parent[face];
    }
    return face;
}

void joinFaces(vector<size_t> &parent, size_t first, size_t second)
{
    const size_t ra = rootOf(parent, first), rb = rootOf(parent, second);
    if (ra != rb) parent[rb] = ra;
}

} // namespace

MeshAnalysis::MeshAnalysis(const FaceIndexedMesh &mesh)
    : bad("None"), g(0)
{
    const DirectedEdgeMesh c(mesh);
    bad = check(c);
    if (bad == "None") g = euler(c);
}

string MeshAnalysis::check(const DirectedEdgeMesh &c) const
{
    if (c.mesh.FaceCount() == 0)
        return "failing edge: the mesh has no faces";

    // task II: an unpaired edge is a hole, a broken link is a pinch
    string fail;
    for (size_t e = 0; e < c.otherHalves.size(); ++e)
    {
        if (c.otherHalves[e] != -1) continue;
        const DirectedEdgeMesh::EdgeKey ends = c.Endpoints(e);
        fail = "failing edge " + to_string(e) + " (vertices "
             + to_string(ends[0]) + " " + to_string(ends[1]) + ")";
        break;
    }

    // task II again: every vertex link has to be a path or a cycle
    vector<Links> links(c.mesh.VertexCount());
    for (const FaceIndexedMesh::Face &tri : c.mesh.Faces())
    {
        for (size_t corner = 0; corner < 3; ++corner)
            links[tri[corner]].push_back(make_pair(tri[(corner + 1) % 3],
                                                   tri[(corner + 2) % 3]));
    }
    for (size_t v = 0; v < links.size(); ++v)
    {
        if (!linkBad(links[v])) continue;
        if (!fail.empty()) fail += ", ";
        fail += "failing vertex " + to_string(v);
        break;
    }

    return fail.empty() ? "None" : fail;
}

size_t MeshAnalysis::euler(const DirectedEdgeMesh &c) const
{
    // task III: each closed piece satisfies V - E + F = 2 - 2g
    vector<size_t> parent(c.mesh.FaceCount());
    for (size_t f = 0; f < parent.size(); ++f) parent[f] = f;
    for (size_t e = 0; e < c.otherHalves.size(); ++e)
    {
        if (c.otherHalves[e] != -1) joinFaces(parent, e / 3, c.otherHalves[e] / 3);
    }

    map<size_t, size_t> indexOf;
    vector<size_t> faceCount;
    vector<vector<bool> > vertexUsed;
    for (size_t f = 0; f < c.mesh.FaceCount(); ++f)
    {
        const size_t r = rootOf(parent, f);
        auto entry = indexOf.find(r);
        if (entry == indexOf.end())
        {
            const size_t index = faceCount.size();
            entry = indexOf.insert(make_pair(r, index)).first;
            faceCount.push_back(0);
            vertexUsed.push_back(vector<bool>(c.mesh.VertexCount(), false));
        }
        const size_t piece = entry->second;
        ++faceCount[piece];
        for (size_t v : c.mesh.Faces()[f]) vertexUsed[piece][v] = true;
    }

    ptrdiff_t genus = 0;
    for (size_t piece = 0; piece < faceCount.size(); ++piece)
    {
        const ptrdiff_t f = static_cast<ptrdiff_t>(faceCount[piece]);
        // two faces per edge on a closed piece, so E = 3F / 2
        const ptrdiff_t e = 3 * f / 2;
        const ptrdiff_t v = static_cast<ptrdiff_t>(
            count(vertexUsed[piece].begin(), vertexUsed[piece].end(), true));
        genus += (2 - (v - e + f)) / 2;
    }
    return static_cast<size_t>(genus);
}
