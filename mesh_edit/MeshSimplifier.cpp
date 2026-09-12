#include "MeshSimplifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

const double kPi = 3.14159265358979323846;
const std::size_t kNoFace = static_cast<std::size_t>(-1);

typedef std::pair<std::size_t, std::size_t> Edge;

Edge MakeEdge(std::size_t first, std::size_t second)
{
    return first < second ? Edge(first, second) : Edge(second, first);
}

struct Candidate
{
    double cost;       // |K|, smallest first
    std::size_t vertex;
    std::size_t version;
};

struct ByCost
{
    bool operator()(const Candidate &first, const Candidate &second) const
    {
        return first.cost > second.cost;
    }
};

} // namespace

MeshSimplifier::MeshSimplifier(const FaceIndexedMesh &mesh, double keepRatio)
    : simplified(mesh),
      aliveFace(mesh.FaceCount(), 1),
      aliveVertex(mesh.VertexCount(), 1),
      gaussian(mesh.VertexCount(), 0.0),
      mean(mesh.VertexCount(), 0.0),
      removedVertices(0)
{
    if (simplified.FaceCount() == 0) return;
    const std::size_t targetVertices = std::max<std::size_t>(
        4, static_cast<std::size_t>(std::ceil(mesh.VertexCount() * keepRatio)));

    // Edge-face adjacency and per-vertex incident faces, for a closed manifold.
    incident.assign(simplified.VertexCount(), std::vector<std::size_t>());
    for (std::size_t face = 0; face < simplified.FaceCount(); ++face)
    {
        const FaceIndexedMesh::Face &corners = simplified.faces[face];
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const std::size_t first = corners[corner];
            const std::size_t second = corners[(corner + 1) % 3];
            incident[first].push_back(face);
            std::map<Edge, std::pair<std::size_t, std::size_t> >::iterator entry =
                edgeFaces.find(MakeEdge(first, second));
            if (entry == edgeFaces.end())
                edgeFaces.insert(std::make_pair(MakeEdge(first, second),
                                                std::make_pair(face, kNoFace)));
            else if (entry->second.second == kNoFace)
                entry->second.second = face;
            else
                throw std::runtime_error("simplification needs a manifold input");
        }
    }

    // Initial curvatures, then the greedy queue ordered by |K|.
    for (std::size_t vertex = 0; vertex < simplified.VertexCount(); ++vertex)
        UpdateCurvature(vertex);

    std::vector<std::size_t> version(simplified.VertexCount(), 0);
    std::vector<char> blocked(simplified.VertexCount(), 0);
    std::priority_queue<Candidate, std::vector<Candidate>, ByCost> queue;
    for (std::size_t vertex = 0; vertex < simplified.VertexCount(); ++vertex)
        queue.push(Candidate{std::fabs(gaussian[vertex]), vertex, 0});

    std::size_t activeVertices = simplified.VertexCount();
    std::size_t activeFaces = simplified.FaceCount();
    const long long characteristic = static_cast<long long>(activeVertices)
        - 3 * static_cast<long long>(activeFaces) / 2 + activeFaces;

    while (!queue.empty())
    {
        const Candidate candidate = queue.top();
        queue.pop();
        if (!aliveVertex[candidate.vertex] || blocked[candidate.vertex]
            || candidate.version != version[candidate.vertex])
            continue;

        std::vector<std::size_t> ring;
        if (!TryRemove(candidate.vertex, ring))
        {
            blocked[candidate.vertex] = 1;
            continue;
        }
        ++removedVertices;
        --activeVertices;
        activeFaces -= 2;

        // Eulerian check: removing a vertex, three edges and two faces keeps V - E + F.
        const long long after = static_cast<long long>(activeVertices)
            - 3 * static_cast<long long>(activeFaces) / 2 + activeFaces;
        if (after != characteristic)
            throw std::runtime_error("Euler check failed during simplification");

        if (activeVertices <= targetVertices) break;

        // Only the old 1-ring changed: refresh its curvature and re-queue.
        std::set<std::size_t> affected(ring.begin(), ring.end());
        for (std::size_t vertex : affected)
        {
            UpdateCurvature(vertex);
            ++version[vertex];
            blocked[vertex] = 0;
            queue.push(Candidate{std::fabs(gaussian[vertex]), vertex, version[vertex]});
        }
    }

    // Drop dead faces and renumber the vertices they still use.
    std::vector<std::size_t> remap(simplified.VertexCount(), kNoFace);
    std::vector<FaceIndexedMesh::Vertex> vertices;
    std::vector<FaceIndexedMesh::Face> faces;
    for (std::size_t face = 0; face < simplified.FaceCount(); ++face)
    {
        if (!aliveFace[face]) continue;
        FaceIndexedMesh::Face remapped = {};
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const std::size_t vertex = simplified.faces[face][corner];
            if (remap[vertex] == kNoFace)
            {
                remap[vertex] = vertices.size();
                vertices.push_back(simplified.vertices[vertex]);
            }
            remapped[corner] = remap[vertex];
        }
        faces.push_back(remapped);
    }
    simplified.vertices.swap(vertices);
    simplified.faces.swap(faces);
}

double MeshSimplifier::FaceArea(std::size_t face) const
{
    const FaceIndexedMesh::Face &corners = simplified.faces[face];
    const FaceIndexedMesh::Vertex &a = simplified.vertices[corners[0]];
    const FaceIndexedMesh::Vertex &b = simplified.vertices[corners[1]];
    const FaceIndexedMesh::Vertex &c = simplified.vertices[corners[2]];
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

double MeshSimplifier::Angle(std::size_t vertex, const FaceIndexedMesh::Face &face) const
{
    const std::size_t corner = face[0] == vertex ? 0 : face[1] == vertex ? 1 : 2;
    const FaceIndexedMesh::Vertex &v = simplified.vertices[vertex];
    const FaceIndexedMesh::Vertex &a = simplified.vertices[face[(corner + 1) % 3]];
    const FaceIndexedMesh::Vertex &b = simplified.vertices[face[(corner + 2) % 3]];
    const double ux = a[0] - v[0], uy = a[1] - v[1], uz = a[2] - v[2];
    const double wx = b[0] - v[0], wy = b[1] - v[1], wz = b[2] - v[2];
    const double cx = uy * wz - uz * wy;
    const double cy = uz * wx - ux * wz;
    const double cz = ux * wy - uy * wx;
    return std::atan2(std::sqrt(cx * cx + cy * cy + cz * cz),
                      ux * wx + uy * wy + uz * wz);
}

double MeshSimplifier::Cotangent(std::size_t at, std::size_t first, std::size_t second) const
{
    const FaceIndexedMesh::Vertex &v = simplified.vertices[at];
    const FaceIndexedMesh::Vertex &a = simplified.vertices[first];
    const FaceIndexedMesh::Vertex &b = simplified.vertices[second];
    const double ux = a[0] - v[0], uy = a[1] - v[1], uz = a[2] - v[2];
    const double wx = b[0] - v[0], wy = b[1] - v[1], wz = b[2] - v[2];
    const double cx = uy * wz - uz * wy;
    const double cy = uz * wx - ux * wz;
    const double cz = ux * wy - uy * wx;
    return (ux * wx + uy * wy + uz * wz)
           / std::sqrt(cx * cx + cy * cy + cz * cz);
}

std::vector<std::size_t> MeshSimplifier::Ring(std::size_t vertex) const
{
    std::vector<std::size_t> ring;
    if (incident[vertex].empty()) return ring;

    std::size_t currentFace = incident[vertex][0];
    std::size_t corner = simplified.faces[currentFace][0] == vertex ? 0
                       : simplified.faces[currentFace][1] == vertex ? 1 : 2;
    ring.push_back(simplified.faces[currentFace][(corner + 1) % 3]);
    ring.push_back(simplified.faces[currentFace][(corner + 2) % 3]);

    for (std::size_t steps = 0; steps <= incident[vertex].size(); ++steps)
    {
        const std::size_t lastVertex = ring.back();
        const std::map<Edge, std::pair<std::size_t, std::size_t> >::const_iterator entry =
            edgeFaces.find(MakeEdge(vertex, lastVertex));
        if (entry == edgeFaces.end()) throw std::runtime_error("vertex is not interior");
        const std::size_t other = entry->second.first == currentFace
            ? entry->second.second : entry->second.first;
        if (other == kNoFace) throw std::runtime_error("vertex is on the boundary");

        corner = simplified.faces[other][0] == vertex ? 0
               : simplified.faces[other][1] == vertex ? 1 : 2;
        const std::size_t n1 = simplified.faces[other][(corner + 1) % 3];
        const std::size_t n2 = simplified.faces[other][(corner + 2) % 3];
        const std::size_t next = n1 == lastVertex ? n2 : n1;
        if (next == ring.front()) break; // Back at the first ring vertex.
        ring.push_back(next);
        currentFace = other;
    }
    return ring;
}

void MeshSimplifier::UpdateCurvature(std::size_t vertex)
{
    double angles = 0.0;
    double mixedArea = 0.0;
    std::map<std::size_t, double> weight;
    for (std::size_t face : incident[vertex])
    {
        const FaceIndexedMesh::Face &corners = simplified.faces[face];
        const std::size_t corner = corners[0] == vertex ? 0
                                 : corners[1] == vertex ? 1 : 2;
        const std::size_t a = corners[(corner + 1) % 3];
        const std::size_t b = corners[(corner + 2) % 3];
        angles += Angle(vertex, corners);
        mixedArea += FaceArea(face) / 3.0;
        // The cotangent opposite edge (vertex, neighbour) sits at the third corner.
        weight[a] += Cotangent(b, vertex, a);
        weight[b] += Cotangent(a, vertex, b);
    }

    // Gaussian curvature from the angle defect; mean curvature from Laplace-Beltrami.
    gaussian[vertex] = (2.0 * kPi - angles) / mixedArea;
    double hx = 0.0, hy = 0.0, hz = 0.0;
    for (const auto &entry : weight)
    {
        const FaceIndexedMesh::Vertex &neighbour = simplified.vertices[entry.first];
        hx += entry.second * (neighbour[0] - simplified.vertices[vertex][0]);
        hy += entry.second * (neighbour[1] - simplified.vertices[vertex][1]);
        hz += entry.second * (neighbour[2] - simplified.vertices[vertex][2]);
    }
    const double length = std::sqrt(hx * hx + hy * hy + hz * hz);
    mean[vertex] = 0.25 * length / mixedArea;
}

bool MeshSimplifier::TryRemove(std::size_t vertex, std::vector<std::size_t> &ring)
{
    ring = Ring(vertex);
    const std::size_t size = ring.size();
    if (size < 3 || size != incident[vertex].size()) return false;
    if (std::set<std::size_t>(ring.begin(), ring.end()).size() != size) return false;

    // Reference normal of the surface around the vertex.
    const FaceIndexedMesh::Face &reference = simplified.faces[incident[vertex][0]];
    const FaceIndexedMesh::Vertex &r0 = simplified.vertices[reference[0]];
    const FaceIndexedMesh::Vertex &r1 = simplified.vertices[reference[1]];
    const FaceIndexedMesh::Vertex &r2 = simplified.vertices[reference[2]];
    const double ux = r1[0] - r0[0], uy = r1[1] - r0[1], uz = r1[2] - r0[2];
    const double wx = r2[0] - r0[0], wy = r2[1] - r0[1], wz = r2[2] - r0[2];
    const double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;

    // Fan from the ring vertex whose thinnest triangle is largest; new diagonals only.
    double bestMinimum = 0.0;
    std::size_t bestCentre = size;
    for (std::size_t centre = 0; centre < size; ++centre)
    {
        const std::size_t c = ring[centre];
        bool valid = true;
        double minimum = std::numeric_limits<double>::max();
        for (std::size_t step = 1; step <= size - 2; ++step)
        {
            const std::size_t a = ring[(centre + step) % size];
            const std::size_t b = ring[(centre + step + 1) % size];
            const FaceIndexedMesh::Vertex &pc = simplified.vertices[c];
            const FaceIndexedMesh::Vertex &pa = simplified.vertices[a];
            const FaceIndexedMesh::Vertex &pb = simplified.vertices[b];
            const double ex = pa[0] - pc[0], ey = pa[1] - pc[1], ez = pa[2] - pc[2];
            const double fx = pb[0] - pc[0], fy = pb[1] - pc[1], fz = pb[2] - pc[2];
            const double cx = ey * fz - ez * fy;
            const double cy = ez * fx - ex * fz;
            const double cz = ex * fy - ey * fx;
            const double area2 = std::sqrt(cx * cx + cy * cy + cz * cz);
            if (area2 < 1e-14 || cx * nx + cy * ny + cz * nz <= 0.0)
            {
                valid = false;
                break;
            }
            minimum = std::min(minimum, area2);
        }
        if (!valid) continue;
        if (size == 3)
        {
            // The fan triangle must not duplicate the opposite face (double cover).
            const std::size_t a = ring[(centre + 1) % 3];
            const std::size_t b = ring[(centre + 2) % 3];
            const std::map<Edge, std::pair<std::size_t, std::size_t> >::const_iterator entry =
                edgeFaces.find(MakeEdge(a, b));
            const std::size_t other = simplified.faces[entry->second.first][0] == vertex
                                      || simplified.faces[entry->second.first][1] == vertex
                                      || simplified.faces[entry->second.first][2] == vertex
                ? entry->second.second : entry->second.first;
            const FaceIndexedMesh::Face &opposite = simplified.faces[other];
            if (opposite[0] == c || opposite[1] == c || opposite[2] == c) valid = false;
        }
        if (!valid) continue;
        for (std::size_t step = 2; step <= size - 2; ++step)
        {
            if (edgeFaces.count(MakeEdge(c, ring[(centre + step) % size])))
            {
                valid = false;
                break;
            }
        }
        if (valid && minimum > bestMinimum)
        {
            bestMinimum = minimum;
            bestCentre = centre;
        }
    }
    if (bestCentre == size) return false;

    // Commit: drop the incident faces, append the fan, refresh edge and vertex data.
    const std::vector<std::size_t> removed = incident[vertex];
    const std::set<std::size_t> dead(removed.begin(), removed.end());
    for (std::size_t face : removed)
    {
        aliveFace[face] = 0;
        const FaceIndexedMesh::Face &corners = simplified.faces[face];
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            std::map<Edge, std::pair<std::size_t, std::size_t> >::iterator entry =
                edgeFaces.find(MakeEdge(corners[corner], corners[(corner + 1) % 3]));
            if (entry->second.first == face) entry->second.first = kNoFace;
            else entry->second.second = kNoFace;
            if (entry->second.first == kNoFace && entry->second.second == kNoFace)
                edgeFaces.erase(entry);
        }
    }
    aliveVertex[vertex] = 0;
    incident[vertex].clear();

    const std::set<std::size_t> ringSet(ring.begin(), ring.end());
    for (std::size_t neighbour : ringSet)
    {
        std::vector<std::size_t> filtered;
        for (std::size_t face : incident[neighbour])
            if (!dead.count(face)) filtered.push_back(face);
        incident[neighbour].swap(filtered);
    }

    const std::size_t c = ring[bestCentre];
    for (std::size_t step = 1; step <= size - 2; ++step)
    {
        const FaceIndexedMesh::Face fan = {{
            c, ring[(bestCentre + step) % size], ring[(bestCentre + step + 1) % size]}};
        const std::size_t newFace = simplified.faces.size();
        simplified.faces.push_back(fan);
        aliveFace.push_back(1);
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const std::size_t first = fan[corner];
            const std::size_t second = fan[(corner + 1) % 3];
            std::map<Edge, std::pair<std::size_t, std::size_t> >::iterator entry =
                edgeFaces.find(MakeEdge(first, second));
            if (entry == edgeFaces.end())
                edgeFaces.insert(std::make_pair(MakeEdge(first, second),
                                                std::make_pair(newFace, kNoFace)));
            else if (entry->second.first == kNoFace)
                entry->second.first = newFace;
            else if (entry->second.second == kNoFace)
                entry->second.second = newFace;
            else
                throw std::runtime_error("simplification created a non-manifold edge "
                    + std::to_string(first) + "-" + std::to_string(second)
                    + " while removing vertex " + std::to_string(vertex));
            incident[first].push_back(newFace);
        }
    }
    return true;
}
