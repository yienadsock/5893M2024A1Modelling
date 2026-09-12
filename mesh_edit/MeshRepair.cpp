#include "MeshRepair.h"

#include "../converter/DirectedEdgeMesh.h"

#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{

typedef std::pair<std::size_t, std::size_t> Undirected;
typedef std::pair<std::size_t, std::size_t> Directed;

struct Incidence
{
    std::size_t face;
    std::size_t tail;
    std::size_t head;
};

Undirected Edge(std::size_t first, std::size_t second)
{
    return first < second ? Undirected(first, second) : Undirected(second, first);
}

double Area(const FaceIndexedMesh &mesh, std::size_t face)
{
    const FaceIndexedMesh::Face &corners = mesh.Faces()[face];
    const FaceIndexedMesh::Vertex &a = mesh.Vertices()[corners[0]];
    const FaceIndexedMesh::Vertex &b = mesh.Vertices()[corners[1]];
    const FaceIndexedMesh::Vertex &c = mesh.Vertices()[corners[2]];
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    const double cx = uy * vz - uz * vy;
    const double cy = uz * vx - ux * vz;
    const double cz = ux * vy - uy * vx;
    return 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
}

std::size_t Root(std::vector<std::size_t> &parent, std::size_t node)
{
    while (parent[node] != node)
    {
        parent[node] = parent[parent[node]];
        node = parent[node];
    }
    return node;
}

void Join(std::vector<std::size_t> &parent, std::size_t first, std::size_t second)
{
    const std::size_t firstRoot = Root(parent, first);
    const std::size_t secondRoot = Root(parent, second);
    if (firstRoot != secondRoot) parent[secondRoot] = firstRoot;
}

// One record per face corner; face (a,b,c) gives directed edges a->b, b->c, c->a.
std::map<Undirected, std::vector<Incidence> > Incidences(const FaceIndexedMesh &mesh)
{
    std::map<Undirected, std::vector<Incidence> > groups;
    for (std::size_t face = 0; face < mesh.FaceCount(); ++face)
    {
        const FaceIndexedMesh::Face &corners = mesh.Faces()[face];
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            Incidence record;
            record.face = face;
            record.tail = corners[corner];
            record.head = corners[(corner + 1) % 3];
            groups[Edge(record.tail, record.head)].push_back(record);
        }
    }
    return groups;
}

} // namespace

MeshRepair::MeshRepair(const FaceIndexedMesh &mesh)
    : repaired(mesh), removedFaces(0), holes(0)
{
    for (std::size_t pass = 0; pass < 1000; ++pass)
    {
        const bool cleaned = RemoveBadFaces();
        const bool filled = FillHoles();
        if (!cleaned && !filled) return;
    }
    throw std::runtime_error("repair did not converge");
}

bool MeshRepair::RemoveBadFaces()
{
    if (repaired.FaceCount() == 0) return false;
    std::vector<char> keep(repaired.FaceCount(), 1);
    const std::map<Undirected, std::vector<Incidence> > groups = Incidences(repaired);

    // Detached debris: keep only the largest edge-connected component.
    {
        std::vector<std::size_t> parent(repaired.FaceCount());
        for (std::size_t index = 0; index < parent.size(); ++index) parent[index] = index;
        for (const auto &entry : groups)
            for (std::size_t index = 1; index < entry.second.size(); ++index)
                Join(parent, entry.second[0].face, entry.second[index].face);
        std::map<std::size_t, std::size_t> sizes;
        for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
            ++sizes[Root(parent, face)];
        std::size_t largestRoot = sizes.begin()->first;
        for (const auto &entry : sizes)
            if (entry.second > sizes[largestRoot]) largestRoot = entry.first;
        for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
            if (Root(parent, face) != largestRoot) keep[face] = 0;
    }

    // Fins: over-full edges keep the largest face per direction; slivers go.
    for (const auto &entry : groups)
    {
        if (entry.second.size() <= 2) continue;
        std::map<Directed, std::size_t> best;
        for (const Incidence &record : entry.second)
        {
            const Directed direction(record.tail, record.head);
            const auto found = best.find(direction);
            if (found == best.end() || Area(repaired, record.face) > Area(repaired, found->second))
                best[direction] = record.face;
        }
        for (const Incidence &record : entry.second)
            if (best[Directed(record.tail, record.head)] != record.face)
                keep[record.face] = 0;
    }

    // Flaps: a face with two or more unpaired edges hangs into the hole.
    {
        std::vector<unsigned char> unpaired(repaired.FaceCount(), 0);
        for (const auto &entry : groups)
        {
            bool paired = false;
            if (entry.second.size() == 2)
            {
                const Incidence &first = entry.second[0];
                const Incidence &second = entry.second[1];
                paired = first.face != second.face && first.tail == second.head
                         && first.head == second.tail;
            }
            if (!paired)
                for (const Incidence &record : entry.second) ++unpaired[record.face];
        }
        for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
            if (unpaired[face] >= 2) keep[face] = 0;
    }

    // Pinches: keep the largest fan per vertex (.tri re-welds split vertices).
    {
        std::vector<std::vector<std::size_t> > incident(repaired.VertexCount());
        for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
        {
            if (!keep[face]) continue;
            for (std::size_t corner = 0; corner < 3; ++corner)
                incident[repaired.faces[face][corner]].push_back(face);
        }
        for (std::size_t vertex = 0; vertex < incident.size(); ++vertex)
        {
            if (incident[vertex].size() < 2) continue;
            std::vector<std::size_t> parent(incident[vertex].size());
            for (std::size_t index = 0; index < parent.size(); ++index) parent[index] = index;
            std::map<std::size_t, std::vector<std::size_t> > byNeighbour;
            for (std::size_t index = 0; index < incident[vertex].size(); ++index)
                for (std::size_t corner = 0; corner < 3; ++corner)
                    if (repaired.faces[incident[vertex][index]][corner] != vertex)
                        byNeighbour[repaired.faces[incident[vertex][index]][corner]].push_back(index);
            for (const auto &entry : byNeighbour)
                for (std::size_t index = 1; index < entry.second.size(); ++index)
                    Join(parent, entry.second[0], entry.second[index]);
            std::map<std::size_t, std::vector<std::size_t> > fans;
            for (std::size_t index = 0; index < incident[vertex].size(); ++index)
                fans[Root(parent, index)].push_back(incident[vertex][index]);
            if (fans.size() < 2) continue;
            std::vector<std::size_t> largestFan;
            for (const auto &entry : fans)
                if (entry.second.size() > largestFan.size()) largestFan = entry.second;
            for (const auto &entry : fans)
                if (entry.second != largestFan)
                    for (std::size_t face : entry.second) keep[face] = 0;
        }
    }

    // Compact the surviving faces and drop every vertex they no longer use.
    std::size_t dropped = 0;
    for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
        if (!keep[face]) ++dropped;
    if (dropped == 0) return false;
    removedFaces += dropped;

    std::vector<std::size_t> remap(repaired.VertexCount(), static_cast<std::size_t>(-1));
    std::vector<FaceIndexedMesh::Vertex> vertices;
    std::vector<FaceIndexedMesh::Face> faces;
    for (std::size_t face = 0; face < repaired.FaceCount(); ++face)
    {
        if (!keep[face]) continue;
        FaceIndexedMesh::Face remapped = {};
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            const std::size_t vertex = repaired.faces[face][corner];
            if (remap[vertex] == static_cast<std::size_t>(-1))
            {
                remap[vertex] = vertices.size();
                vertices.push_back(repaired.vertices[vertex]);
            }
            remapped[corner] = remap[vertex];
        }
        faces.push_back(remapped);
    }
    repaired.vertices.swap(vertices);
    repaired.faces.swap(faces);
    return true;
}

bool MeshRepair::FillHoles()
{
    const DirectedEdgeMesh connectivity(repaired);
    std::vector<char> used(connectivity.DirectedEdgeCount(), 0);
    bool filled = false;

    for (std::size_t start = 0; start < used.size(); ++start)
    {
        if (connectivity.otherHalves[start] != -1 || used[start] != 0) continue;

        // Walk the hole: rotate around the head, crossing paired edges, to the start.
        std::vector<std::size_t> loop;
        std::size_t edge = start;
        do
        {
            if (loop.size() > used.size())
                throw std::runtime_error("boundary walk did not close");
            loop.push_back(edge);
            used[edge] = 1;
            std::size_t next = 3 * (edge / 3) + (edge % 3 + 1) % 3;
            std::size_t budget = used.size() + 1;
            while (connectivity.otherHalves[next] != -1)
            {
                if (budget-- == 0)
                    throw std::runtime_error("vertex fan did not reach the boundary");
                next = connectivity.otherHalves[next];
                next = 3 * (next / 3) + (next % 3 + 1) % 3;
            }
            edge = next;
        } while (edge != start);

        // A hole is a simple loop of at least three edges from distinct faces.
        std::set<std::size_t> heads, faces;
        for (std::size_t item : loop)
        {
            heads.insert(connectivity.Endpoints(item)[1]);
            faces.insert(item / 3);
        }
        if (loop.size() < 3 || heads.size() != loop.size() || faces.size() != loop.size())
            continue;

        // One vertex at the centre of gravity, fanned to pair the boundary edges.
        FaceIndexedMesh::Vertex centre = {};
        for (std::size_t item : loop)
        {
            const DirectedEdgeMesh::EdgeKey ends = connectivity.Endpoints(item);
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
        for (std::size_t item : loop)
        {
            const DirectedEdgeMesh::EdgeKey ends = connectivity.Endpoints(item);
            repaired.faces.push_back(FaceIndexedMesh::Face{{ends[1], ends[0], centreId}});
        }
        ++holes;
        filled = true;
    }
    return filled;
}
