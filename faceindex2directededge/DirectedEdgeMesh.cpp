#include "DirectedEdgeMesh.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{
typedef std::array<std::size_t, 2> EdgeKey;

struct EdgeHash
{
    std::size_t operator()(const EdgeKey &key) const
    {
        const std::size_t first = std::hash<std::size_t>()(key[0]);
        const std::size_t second = std::hash<std::size_t>()(key[1]);
        return first ^ (second + 0x9e3779b9U + (first << 6) + (first >> 2));
    }
};

EdgeKey UndirectedKey(const EdgeKey &endpoints)
{
    return EdgeKey{{std::min(endpoints[0], endpoints[1]),
                    std::max(endpoints[0], endpoints[1])}};
}
}

DirectedEdgeMesh::DirectedEdgeMesh(FaceIndexedMesh inputMesh)
    : mesh(std::move(inputMesh)), unpairedEdgeCount(0), isolatedVertexCount(0)
{
    BuildConnectivity();
}

DirectedEdgeMesh::EdgeKey DirectedEdgeMesh::Endpoints(std::size_t edgeId) const
{
    const FaceIndexedMesh::Face &face = mesh.Faces()[edgeId / 3];
    const std::size_t corner = edgeId % 3;
    return EdgeKey{{face[(corner + 2) % 3], face[corner]}};
}

void DirectedEdgeMesh::BuildConnectivity()
{
    // Signed IDs allow -1 to represent a missing outgoing edge or other half.
    const std::size_t maximumId = static_cast<std::size_t>(
        std::numeric_limits<EdgeId>::max());
    if (mesh.FaceCount() > maximumId / 3
        || mesh.FaceCount() > otherHalves.max_size() / 3
        || mesh.VertexCount() > firstDirectedEdges.max_size())
        throw std::runtime_error("Mesh exceeds the supported directed-edge size.");

    const std::size_t edgeCount = mesh.FaceCount() * 3;
    firstDirectedEdges.assign(mesh.VertexCount(), -1);
    otherHalves.assign(edgeCount, -1);
    std::vector<bool> degenerateFaces(mesh.FaceCount(), false);
    for (std::size_t faceId = 0; faceId < mesh.FaceCount(); ++faceId)
    {
        const FaceIndexedMesh::Face &face = mesh.Faces()[faceId];
        degenerateFaces[faceId] = face[0] == face[1] || face[1] == face[2]
                                 || face[2] == face[0];
    }

    std::unordered_map<EdgeKey, std::vector<std::size_t>, EdgeHash> edgeGroups;
    for (std::size_t edgeId = 0; edgeId < edgeCount; ++edgeId)
    {
        const EdgeKey endpoints = Endpoints(edgeId);
        if (firstDirectedEdges[endpoints[0]] == -1)
            firstDirectedEdges[endpoints[0]] = static_cast<EdgeId>(edgeId);
        // Retain every incidence, including repeated directions and self-loops.
        edgeGroups[UndirectedKey(endpoints)].push_back(edgeId);
    }

    // Visit groups by their smallest edge ID so diagnostics are deterministic.
    for (std::size_t edgeId = 0; edgeId < edgeCount; ++edgeId)
    {
        const EdgeKey endpoints = Endpoints(edgeId);
        const EdgeKey key = UndirectedKey(endpoints);
        const std::vector<std::size_t> &group = edgeGroups.at(key);
        if (group.front() != edgeId)
            continue;

        bool hasDegenerateFace = false;
        for (std::size_t i = 0; i < group.size(); ++i)
            hasDegenerateFace = hasDegenerateFace || degenerateFaces[group[i] / 3];

        std::string reason;
        if (hasDegenerateFace)
            reason = "incident to a face with repeated vertex indices";
        else if (group.size() == 1)
            reason = "boundary edge";
        else if (group.size() > 2)
            reason = "non-manifold edge with more than two incidences";
        else
        {
            const EdgeKey opposite = Endpoints(group[1]);
            if (endpoints[0] == opposite[1] && endpoints[1] == opposite[0]
                && group[0] / 3 != group[1] / 3)
            {
                otherHalves[group[0]] = static_cast<EdgeId>(group[1]);
                otherHalves[group[1]] = static_cast<EdgeId>(group[0]);
                continue;
            }
            reason = "inconsistent face orientation (same-direction edges)";
        }

        UnpairedGroup issue;
        issue.vertices = key;
        issue.edges = group;
        issue.reason = reason;
        unpairedEdgeCount += group.size();
        unpairedGroups.push_back(std::move(issue));
    }

    isolatedVertexCount = static_cast<std::size_t>(std::count(
        firstDirectedEdges.begin(), firstDirectedEdges.end(), EdgeId(-1)));
}

void DirectedEdgeMesh::WriteDirectedEdge(std::ostream &output) const
{
    const std::vector<std::string> &header = mesh.HeaderLines();
    for (std::size_t i = 0; i < header.size(); ++i)
        output << header[i] << '\n';

    output << std::defaultfloat
           << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t vertexId = 0; vertexId < mesh.VertexCount(); ++vertexId)
    {
        const FaceIndexedMesh::Vertex &vertex = mesh.Vertices()[vertexId];
        output << "Vertex " << vertexId << ' ' << vertex[0] << ' '
               << vertex[1] << ' ' << vertex[2] << '\n';
    }
    for (std::size_t vertexId = 0; vertexId < firstDirectedEdges.size(); ++vertexId)
        output << "FirstDirectedEdge " << vertexId << ' '
               << firstDirectedEdges[vertexId] << '\n';

    for (std::size_t faceId = 0; faceId < mesh.FaceCount(); ++faceId)
    {
        const FaceIndexedMesh::Face &face = mesh.Faces()[faceId];
        output << "Face " << faceId << ' ' << face[0] << ' '
               << face[1] << ' ' << face[2] << '\n';
    }
    for (std::size_t edgeId = 0; edgeId < otherHalves.size(); ++edgeId)
        output << "OtherHalf " << edgeId << ' ' << otherHalves[edgeId] << '\n';
    if (!output)
        throw std::runtime_error("Failed while writing the directed-edge file.");
}

void DirectedEdgeMesh::WriteDiagnostics(std::ostream &output) const
{
    for (std::size_t i = 0; i < unpairedGroups.size(); ++i)
    {
        const UnpairedGroup &issue = unpairedGroups[i];
        output << "faceindex2directededge: warning: " << issue.reason
               << " at vertices " << issue.vertices[0] << ", " << issue.vertices[1]
               << "; directed edges";
        for (std::size_t j = 0; j < issue.edges.size(); ++j)
            output << ' ' << issue.edges[j];
        output << " use OtherHalf -1.\n";
    }
    if (isolatedVertexCount != 0)
        output << "faceindex2directededge: warning: " << isolatedVertexCount
               << " isolated vertices use FirstDirectedEdge -1.\n";
}
