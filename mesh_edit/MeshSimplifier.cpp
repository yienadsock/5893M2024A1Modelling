#include "MeshSimplifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace
{

const double PI = 3.14159265358979323846;
const size_t NONE = static_cast<size_t>(-1);

typedef pair<size_t, size_t> Edge;

Edge key(size_t a, size_t b) { return a < b ? Edge(a, b) : Edge(b, a); }

struct Cand
{
    double cost;       // |K|, smallest first
    size_t vertex;
    size_t version;
};

struct ByCost
{
    bool operator()(const Cand &a, const Cand &b) const { return a.cost > b.cost; }
};

} // namespace

MeshSimplifier::MeshSimplifier(const FaceIndexedMesh &mesh, double keep)
    : thin(mesh),
      liveFace(mesh.FaceCount(), 1),
      liveVert(mesh.VertexCount(), 1),
      K(mesh.VertexCount(), 0.0),
      H(mesh.VertexCount(), 0.0),
      deadVerts(0)
{
    if (thin.FaceCount() == 0) return;
    const size_t target = max<size_t>(
        4, static_cast<size_t>(ceil(mesh.VertexCount() * keep)));

    // edges -> the (at most two) faces on them, plus the faces around a vertex
    around.assign(thin.VertexCount(), vector<size_t>());
    for (size_t f = 0; f < thin.FaceCount(); ++f)
    {
        const FaceIndexedMesh::Face &t = thin.faces[f];
        for (size_t k = 0; k < 3; ++k)
        {
            const size_t a = t[k], b = t[(k + 1) % 3];
            around[a].push_back(f);
            auto entry = edgeFaces.find(key(a, b));
            if (entry == edgeFaces.end())
                edgeFaces.insert(make_pair(key(a, b), make_pair(f, NONE)));
            else if (entry->second.second == NONE)
                entry->second.second = f;
            else
                throw runtime_error("simplification needs a manifold input");
        }
    }

    // initial curvatures, then a greedy queue keyed on |K|
    for (size_t v = 0; v < thin.VertexCount(); ++v) curvature(v);

    vector<size_t> version(thin.VertexCount(), 0);
    vector<char> blocked(thin.VertexCount(), 0);
    priority_queue<Cand, vector<Cand>, ByCost> todo;
    for (size_t v = 0; v < thin.VertexCount(); ++v)
        todo.push(Cand{fabs(K[v]), v, 0});

    size_t verts = thin.VertexCount();
    size_t faces = thin.FaceCount();
    const long long euler = static_cast<long long>(verts)
        - 3 * static_cast<long long>(faces) / 2 + faces;

    while (!todo.empty())
    {
        const Cand c = todo.top();
        todo.pop();
        if (!liveVert[c.vertex] || blocked[c.vertex] || c.version != version[c.vertex])
            continue;

        vector<size_t> ring;
        if (!chop(c.vertex, ring))
        {
            blocked[c.vertex] = 1;
            continue;
        }
        ++deadVerts;
        --verts;
        faces -= 2;

        // Eulerian check: one vertex, three edges and two faces per chop
        const long long after = static_cast<long long>(verts)
            - 3 * static_cast<long long>(faces) / 2 + faces;
        if (after != euler)
            throw runtime_error("Euler check failed during simplification");

        if (verts <= target) break;

        // only the old 1-ring moved, so refresh that and re-queue it
        set<size_t> touched(ring.begin(), ring.end());
        for (size_t v : touched)
        {
            curvature(v);
            ++version[v];
            blocked[v] = 0;
            todo.push(Cand{fabs(K[v]), v, version[v]});
        }
    }

    // bin the dead faces and renumber whatever vertices are still used
    vector<size_t> remap(thin.VertexCount(), NONE);
    vector<FaceIndexedMesh::Vertex> vertsOut;
    vector<FaceIndexedMesh::Face> facesOut;
    for (size_t f = 0; f < thin.FaceCount(); ++f)
    {
        if (!liveFace[f]) continue;
        FaceIndexedMesh::Face tri = {};
        for (size_t k = 0; k < 3; ++k)
        {
            const size_t v = thin.faces[f][k];
            if (remap[v] == NONE)
            {
                remap[v] = vertsOut.size();
                vertsOut.push_back(thin.vertices[v]);
            }
            tri[k] = remap[v];
        }
        facesOut.push_back(tri);
    }
    thin.vertices.swap(vertsOut);
    thin.faces.swap(facesOut);
}

double MeshSimplifier::triArea(size_t f) const
{
    const FaceIndexedMesh::Face &t = thin.faces[f];
    const FaceIndexedMesh::Vertex &a = thin.vertices[t[0]];
    const FaceIndexedMesh::Vertex &b = thin.vertices[t[1]];
    const FaceIndexedMesh::Vertex &c = thin.vertices[t[2]];
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * sqrt(cx * cx + cy * cy + cz * cz);
}

double MeshSimplifier::angleAt(size_t v, const FaceIndexedMesh::Face &tri) const
{
    const size_t corner = tri[0] == v ? 0 : tri[1] == v ? 1 : 2;
    const FaceIndexedMesh::Vertex &p = thin.vertices[v];
    const FaceIndexedMesh::Vertex &a = thin.vertices[tri[(corner + 1) % 3]];
    const FaceIndexedMesh::Vertex &b = thin.vertices[tri[(corner + 2) % 3]];
    const double ux = a[0] - p[0], uy = a[1] - p[1], uz = a[2] - p[2];
    const double wx = b[0] - p[0], wy = b[1] - p[1], wz = b[2] - p[2];
    const double cx = uy * wz - uz * wy;
    const double cy = uz * wx - ux * wz;
    const double cz = ux * wy - uy * wx;
    return atan2(sqrt(cx * cx + cy * cy + cz * cz), ux * wx + uy * wy + uz * wz);
}

double MeshSimplifier::cotan(size_t at, size_t first, size_t second) const
{
    const FaceIndexedMesh::Vertex &v = thin.vertices[at];
    const FaceIndexedMesh::Vertex &a = thin.vertices[first];
    const FaceIndexedMesh::Vertex &b = thin.vertices[second];
    const double ux = a[0] - v[0], uy = a[1] - v[1], uz = a[2] - v[2];
    const double wx = b[0] - v[0], wy = b[1] - v[1], wz = b[2] - v[2];
    const double cx = uy * wz - uz * wy;
    const double cy = uz * wx - ux * wz;
    const double cz = ux * wy - uy * wx;
    return (ux * wx + uy * wy + uz * wz) / sqrt(cx * cx + cy * cy + cz * cz);
}

vector<size_t> MeshSimplifier::ringOf(size_t v) const
{
    vector<size_t> ring;
    if (around[v].empty()) return ring;

    size_t face = around[v][0];
    size_t corner = thin.faces[face][0] == v ? 0 : thin.faces[face][1] == v ? 1 : 2;
    ring.push_back(thin.faces[face][(corner + 1) % 3]);
    ring.push_back(thin.faces[face][(corner + 2) % 3]);

    for (size_t steps = 0; steps <= around[v].size(); ++steps)
    {
        const size_t last = ring.back();
        const auto entry = edgeFaces.find(key(v, last));
        if (entry == edgeFaces.end()) throw runtime_error("vertex is not interior");
        const size_t other = entry->second.first == face
            ? entry->second.second : entry->second.first;
        if (other == NONE) throw runtime_error("vertex is on the boundary");

        corner = thin.faces[other][0] == v ? 0 : thin.faces[other][1] == v ? 1 : 2;
        const size_t n1 = thin.faces[other][(corner + 1) % 3];
        const size_t n2 = thin.faces[other][(corner + 2) % 3];
        const size_t next = n1 == last ? n2 : n1;
        if (next == ring.front()) break;   // round the ring, we're done
        ring.push_back(next);
        face = other;
    }
    return ring;
}

void MeshSimplifier::curvature(size_t v)
{
    double angles = 0.0;
    double mixedArea = 0.0;
    map<size_t, double> weight;
    for (size_t f : around[v])
    {
        const FaceIndexedMesh::Face &t = thin.faces[f];
        const size_t corner = t[0] == v ? 0 : t[1] == v ? 1 : 2;
        const size_t a = t[(corner + 1) % 3], b = t[(corner + 2) % 3];
        angles += angleAt(v, t);
        mixedArea += triArea(f) / 3.0;
        // the cotangent opposite edge (v, neighbour) sits at the third corner
        weight[a] += cotan(b, v, a);
        weight[b] += cotan(a, v, b);
    }

    // Gaussian from the angle defect, mean from Laplace-Beltrami
    K[v] = (2.0 * PI - angles) / mixedArea;
    double hx = 0.0, hy = 0.0, hz = 0.0;
    for (const auto &w : weight)
    {
        const FaceIndexedMesh::Vertex &n = thin.vertices[w.first];
        hx += w.second * (n[0] - thin.vertices[v][0]);
        hy += w.second * (n[1] - thin.vertices[v][1]);
        hz += w.second * (n[2] - thin.vertices[v][2]);
    }
    const double len = sqrt(hx * hx + hy * hy + hz * hz);
    H[v] = 0.25 * len / mixedArea;
}

bool MeshSimplifier::chop(size_t v, vector<size_t> &ring)
{
    ring = ringOf(v);
    const size_t size = ring.size();
    if (size < 3 || size != around[v].size()) return false;
    if (set<size_t>(ring.begin(), ring.end()).size() != size) return false;

    // reference normal of the surface around the vertex
    const FaceIndexedMesh::Face &ref = thin.faces[around[v][0]];
    const FaceIndexedMesh::Vertex &r0 = thin.vertices[ref[0]];
    const FaceIndexedMesh::Vertex &r1 = thin.vertices[ref[1]];
    const FaceIndexedMesh::Vertex &r2 = thin.vertices[ref[2]];
    const double ux = r1[0] - r0[0], uy = r1[1] - r0[1], uz = r1[2] - r0[2];
    const double wx = r2[0] - r0[0], wy = r2[1] - r0[1], wz = r2[2] - r0[2];
    const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;

    // fan from the ring vertex whose thinnest triangle is the fattest one, and
    // only diagonals that aren't already edges
    double bestMin = 0.0;
    size_t best = size;
    for (size_t centre = 0; centre < size; ++centre)
    {
        const size_t c = ring[centre];
        bool ok = true;
        double smallest = numeric_limits<double>::max();
        for (size_t step = 1; step <= size - 2; ++step)
        {
            const size_t a = ring[(centre + step) % size];
            const size_t b = ring[(centre + step + 1) % size];
            const FaceIndexedMesh::Vertex &pc = thin.vertices[c];
            const FaceIndexedMesh::Vertex &pa = thin.vertices[a];
            const FaceIndexedMesh::Vertex &pb = thin.vertices[b];
            const double ex = pa[0] - pc[0], ey = pa[1] - pc[1], ez = pa[2] - pc[2];
            const double fx = pb[0] - pc[0], fy = pb[1] - pc[1], fz = pb[2] - pc[2];
            const double cx = ey * fz - ez * fy;
            const double cy = ez * fx - ex * fz;
            const double cz = ex * fy - ey * fx;
            const double area2 = sqrt(cx * cx + cy * cy + cz * cz);
            if (area2 < 1e-14 || cx * nx + cy * ny + cz * nz <= 0.0)
            {
                ok = false;
                break;
            }
            smallest = min(smallest, area2);
        }
        if (!ok) continue;
        if (size == 3)
        {
            // don't let the fan triangle double-cover the face opposite it
            const size_t a = ring[(centre + 1) % 3];
            const size_t b = ring[(centre + 2) % 3];
            const auto entry = edgeFaces.find(key(a, b));
            const size_t other = thin.faces[entry->second.first][0] == v
                              || thin.faces[entry->second.first][1] == v
                              || thin.faces[entry->second.first][2] == v
                ? entry->second.second : entry->second.first;
            const FaceIndexedMesh::Face &opposite = thin.faces[other];
            if (opposite[0] == c || opposite[1] == c || opposite[2] == c) ok = false;
        }
        if (!ok) continue;
        for (size_t step = 2; step <= size - 2; ++step)
        {
            if (edgeFaces.count(key(c, ring[(centre + step) % size])))
            {
                ok = false;
                break;
            }
        }
        if (ok && smallest > bestMin)
        {
            bestMin = smallest;
            best = centre;
        }
    }
    if (best == size) return false;

    // commit: bin the incident faces, add the fan, fix up edges and vertices
    const vector<size_t> dead = around[v];
    const set<size_t> deadSet(dead.begin(), dead.end());
    for (size_t f : dead)
    {
        liveFace[f] = 0;
        const FaceIndexedMesh::Face &t = thin.faces[f];
        for (size_t k = 0; k < 3; ++k)
        {
            auto entry = edgeFaces.find(key(t[k], t[(k + 1) % 3]));
            if (entry->second.first == f) entry->second.first = NONE;
            else entry->second.second = NONE;
            if (entry->second.first == NONE && entry->second.second == NONE)
                edgeFaces.erase(entry);
        }
    }
    liveVert[v] = 0;
    around[v].clear();

    const set<size_t> ringSet(ring.begin(), ring.end());
    for (size_t n : ringSet)
    {
        vector<size_t> keep;
        for (size_t f : around[n])
            if (!deadSet.count(f)) keep.push_back(f);
        around[n].swap(keep);
    }

    const size_t c = ring[best];
    for (size_t step = 1; step <= size - 2; ++step)
    {
        const FaceIndexedMesh::Face fan = {{
            c, ring[(best + step) % size], ring[(best + step + 1) % size]}};
        const size_t newFace = thin.faces.size();
        thin.faces.push_back(fan);
        liveFace.push_back(1);
        for (size_t k = 0; k < 3; ++k)
        {
            const size_t a = fan[k], b = fan[(k + 1) % 3];
            auto entry = edgeFaces.find(key(a, b));
            if (entry == edgeFaces.end())
                edgeFaces.insert(make_pair(key(a, b), make_pair(newFace, NONE)));
            else if (entry->second.first == NONE)
                entry->second.first = newFace;
            else if (entry->second.second == NONE)
                entry->second.second = newFace;
            else
                throw runtime_error("simplification created a non-manifold edge "
                    + to_string(a) + "-" + to_string(b)
                    + " while removing vertex " + to_string(v));
            around[a].push_back(newFace);
        }
    }
    return true;
}
