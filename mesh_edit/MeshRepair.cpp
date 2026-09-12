#include "MeshRepair.h"

#include "../converter/DirectedEdgeMesh.h"

#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

using namespace std;

namespace
{

typedef pair<size_t, size_t> Edge;    // undirected key: (smaller, bigger)
typedef pair<size_t, size_t> Dir;     // directed, as (tail, head)

struct Hit { size_t face, tail, head; };

Edge key(size_t a, size_t b) { return a < b ? Edge(a, b) : Edge(b, a); }

double area(const FaceIndexedMesh &m, size_t f)
{
    const FaceIndexedMesh::Face &t = m.Faces()[f];
    const FaceIndexedMesh::Vertex &a = m.Vertices()[t[0]];
    const FaceIndexedMesh::Vertex &b = m.Vertices()[t[1]];
    const FaceIndexedMesh::Vertex &c = m.Vertices()[t[2]];
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
}

size_t rootOf(vector<size_t> &parent, size_t node)
{
    while (parent[node] != node)
    {
        parent[node] = parent[parent[node]];
        node = parent[node];
    }
    return node;
}

void join(vector<size_t> &parent, size_t a, size_t b)
{
    const size_t ra = rootOf(parent, a), rb = rootOf(parent, b);
    if (ra != rb) parent[rb] = ra;
}

typedef map<Edge, vector<Hit> > IncidenceMap;

// one record per face corner; face (a,b,c) gives a->b, b->c, c->a
IncidenceMap incidences(const FaceIndexedMesh &m)
{
    IncidenceMap groups;
    for (size_t f = 0; f < m.FaceCount(); ++f)
    {
        const FaceIndexedMesh::Face &t = m.Faces()[f];
        for (size_t k = 0; k < 3; ++k)
        {
            Hit h;
            h.face = f;
            h.tail = t[k];
            h.head = t[(k + 1) % 3];
            groups[key(h.tail, h.head)].push_back(h);
        }
    }
    return groups;
}

} // namespace

MeshRepair::MeshRepair(const FaceIndexedMesh &mesh)
    : fixed(mesh), droppedFaces(0), filledHoles(0)
{
    for (size_t pass = 0; pass < 1000; ++pass)
    {
        const bool swept = trim();
        const bool patched = patch();
        if (!swept && !patched) return;
    }
    throw runtime_error("repair did not converge");
}

bool MeshRepair::trim()
{
    if (fixed.FaceCount() == 0) return false;
    vector<char> keep(fixed.FaceCount(), 1);
    const IncidenceMap groups = incidences(fixed);

    // stray bits: keep only the biggest edge-connected chunk
    {
        vector<size_t> parent(fixed.FaceCount());
        for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
        for (const auto &g : groups)
            for (size_t i = 1; i < g.second.size(); ++i)
                join(parent, g.second[0].face, g.second[i].face);
        map<size_t, size_t> sizes;
        for (size_t f = 0; f < fixed.FaceCount(); ++f) ++sizes[rootOf(parent, f)];
        size_t biggest = sizes.begin()->first;
        for (const auto &s : sizes)
            if (s.second > sizes[biggest]) biggest = s.first;
        for (size_t f = 0; f < fixed.FaceCount(); ++f)
            if (rootOf(parent, f) != biggest) keep[f] = 0;
    }

    // fins: over-full edges keep the biggest face per direction, slivers go
    for (const auto &g : groups)
    {
        if (g.second.size() <= 2) continue;
        map<Dir, size_t> best;
        for (const Hit &h : g.second)
        {
            const Dir d(h.tail, h.head);
            const auto found = best.find(d);
            if (found == best.end() || area(fixed, h.face) > area(fixed, found->second))
                best[d] = h.face;
        }
        for (const Hit &h : g.second)
            if (best[Dir(h.tail, h.head)] != h.face) keep[h.face] = 0;
    }

    // flaps: a face with 2+ unpaired edges is hanging into the hole
    {
        vector<unsigned char> unpaired(fixed.FaceCount(), 0);
        for (const auto &g : groups)
        {
            bool paired = false;
            if (g.second.size() == 2)
            {
                const Hit &p = g.second[0], &q = g.second[1];
                paired = p.face != q.face && p.tail == q.head && p.head == q.tail;
            }
            if (!paired)
                for (const Hit &h : g.second) ++unpaired[h.face];
        }
        for (size_t f = 0; f < fixed.FaceCount(); ++f)
            if (unpaired[f] >= 2) keep[f] = 0;
    }

    // pinches: keep the biggest fan per vertex (.tri re-welds split vertices)
    {
        vector<vector<size_t> > around(fixed.VertexCount());
        for (size_t f = 0; f < fixed.FaceCount(); ++f)
        {
            if (!keep[f]) continue;
            for (size_t k = 0; k < 3; ++k) around[fixed.faces[f][k]].push_back(f);
        }
        for (size_t v = 0; v < around.size(); ++v)
        {
            if (around[v].size() < 2) continue;
            vector<size_t> parent(around[v].size());
            for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
            map<size_t, vector<size_t> > byNeighbour;
            for (size_t i = 0; i < around[v].size(); ++i)
                for (size_t k = 0; k < 3; ++k)
                    if (fixed.faces[around[v][i]][k] != v)
                        byNeighbour[fixed.faces[around[v][i]][k]].push_back(i);
            for (const auto &n : byNeighbour)
                for (size_t i = 1; i < n.second.size(); ++i)
                    join(parent, n.second[0], n.second[i]);
            map<size_t, vector<size_t> > fans;
            for (size_t i = 0; i < around[v].size(); ++i)
                fans[rootOf(parent, i)].push_back(around[v][i]);
            if (fans.size() < 2) continue;
            vector<size_t> biggest;
            for (const auto &fan : fans)
                if (fan.second.size() > biggest.size()) biggest = fan.second;
            for (const auto &fan : fans)
                if (fan.second != biggest)
                    for (size_t f : fan.second) keep[f] = 0;
        }
    }

    // squeeze out the dead faces, and any vertex they stop using
    size_t gone = 0;
    for (size_t f = 0; f < fixed.FaceCount(); ++f)
        if (!keep[f]) ++gone;
    if (gone == 0) return false;
    droppedFaces += gone;

    vector<size_t> remap(fixed.VertexCount(), static_cast<size_t>(-1));
    vector<FaceIndexedMesh::Vertex> verts;
    vector<FaceIndexedMesh::Face> faces;
    for (size_t f = 0; f < fixed.FaceCount(); ++f)
    {
        if (!keep[f]) continue;
        FaceIndexedMesh::Face tri = {};
        for (size_t k = 0; k < 3; ++k)
        {
            const size_t v = fixed.faces[f][k];
            if (remap[v] == static_cast<size_t>(-1))
            {
                remap[v] = verts.size();
                verts.push_back(fixed.vertices[v]);
            }
            tri[k] = remap[v];
        }
        faces.push_back(tri);
    }
    fixed.vertices.swap(verts);
    fixed.faces.swap(faces);
    return true;
}

bool MeshRepair::patch()
{
    const DirectedEdgeMesh c(fixed);
    vector<char> used(c.DirectedEdgeCount(), 0);
    bool any = false;

    for (size_t start = 0; start < used.size(); ++start)
    {
        if (c.otherHalves[start] != -1 || used[start] != 0) continue;

        // walk the hole: keep turning around the head, crossing paired edges,
        // until another boundary edge of the same loop turns up
        vector<size_t> loop;
        size_t e = start;
        do
        {
            if (loop.size() > used.size()) throw runtime_error("boundary walk did not close");
            loop.push_back(e);
            used[e] = 1;
            size_t next = 3 * (e / 3) + (e % 3 + 1) % 3;
            size_t budget = used.size() + 1;
            while (c.otherHalves[next] != -1)
            {
                if (budget-- == 0) throw runtime_error("vertex fan did not reach the boundary");
                next = c.otherHalves[next];
                next = 3 * (next / 3) + (next % 3 + 1) % 3;
            }
            e = next;
        } while (e != start);

        // a fillable hole is a simple loop: 3+ edges from distinct faces
        set<size_t> heads, faces;
        for (size_t item : loop)
        {
            heads.insert(c.Endpoints(item)[1]);
            faces.insert(item / 3);
        }
        if (loop.size() < 3 || heads.size() != loop.size() || faces.size() != loop.size())
            continue;

        // one new vertex at the centre of gravity, fanned out so every fan
        // face pairs up with a boundary edge
        FaceIndexedMesh::Vertex centre = {};
        for (size_t item : loop)
        {
            const DirectedEdgeMesh::EdgeKey ends = c.Endpoints(item);
            const FaceIndexedMesh::Vertex &p = fixed.vertices[ends[1]];
            centre[0] += p[0];
            centre[1] += p[1];
            centre[2] += p[2];
        }
        centre[0] /= loop.size();
        centre[1] /= loop.size();
        centre[2] /= loop.size();
        const size_t mid = fixed.vertices.size();
        fixed.vertices.push_back(centre);
        for (size_t item : loop)
        {
            const DirectedEdgeMesh::EdgeKey ends = c.Endpoints(item);
            fixed.faces.push_back(FaceIndexedMesh::Face{{ends[1], ends[0], mid}});
        }
        ++filledHoles;
        any = true;
    }
    return any;
}
