#include "DirectedEdgeMesh.h"

#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <utility>

FaceIndexedMesh FaceIndexedMesh::ReadFace(std::istream &input)
{
    FaceIndexedMesh mesh;
    std::string line, kind;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream record(line);
        if (!(record >> kind)) continue;
        if (kind[0] == '#')
        {
            mesh.headerLines.push_back(line);
            continue;
        }
        std::size_t id;
        if (kind == "Vertex")
        {
            Vertex vertex;
            if (!(record >> id >> vertex[0] >> vertex[1] >> vertex[2])
                || id != mesh.vertices.size())
                throw std::runtime_error("Cannot read Vertex record.");
            mesh.vertices.push_back(vertex);
        }
        else if (kind == "Face")
        {
            Face face;
            if (!(record >> id >> face[0] >> face[1] >> face[2])
                || id != mesh.faces.size())
                throw std::runtime_error("Cannot read Face record.");
            for (std::size_t index : face)
                if (index >= mesh.vertices.size())
                    throw std::runtime_error("Face vertex index is out of range.");
            mesh.faces.push_back(face);
        }
        else throw std::runtime_error("Unknown face-file record: " + kind);
    }
    if (input.bad() || mesh.headerLines.empty())
        throw std::runtime_error("Cannot read .face file or missing header.");
    return mesh;
}

DirectedEdgeMesh::DirectedEdgeMesh(FaceIndexedMesh inputMesh) : mesh(std::move(inputMesh))
{
    BuildConnectivity();
}

DirectedEdgeMesh::EdgeKey DirectedEdgeMesh::Endpoints(std::size_t edgeId) const
{
    const auto &face = mesh.Faces()[edgeId / 3];
    return EdgeKey{{face[(edgeId % 3 + 2) % 3], face[edgeId % 3]}};
}

void DirectedEdgeMesh::BuildConnectivity()
{
    firstDirectedEdges.assign(mesh.VertexCount(), -1);
    otherHalves.assign(mesh.FaceCount() * 3, -1);
    std::map<EdgeKey, std::vector<std::size_t>> groups;
    for (std::size_t e = 0; e < otherHalves.size(); ++e)
    {
        const EdgeKey edge = Endpoints(e);
        if (firstDirectedEdges[edge[0]] == -1)
            firstDirectedEdges[edge[0]] = static_cast<EdgeId>(e);
        groups[EdgeKey{{std::min(edge[0], edge[1]), std::max(edge[0], edge[1])}}].push_back(e);
    }
    for (const auto &entry : groups)
    {
        const auto &edges = entry.second;
        // Only two reverse incidences in different faces pair up; the rest stay -1.
        if (edges.size() != 2 || edges[0] / 3 == edges[1] / 3) continue;
        const EdgeKey a = Endpoints(edges[0]), b = Endpoints(edges[1]);
        if (a[0] != a[1] && a[0] == b[1] && a[1] == b[0])
        {
            otherHalves[edges[0]] = static_cast<EdgeId>(edges[1]);
            otherHalves[edges[1]] = static_cast<EdgeId>(edges[0]);
        }
    }
}

void DirectedEdgeMesh::WriteDirectedEdge(std::ostream &output) const
{
    for (const auto &line : mesh.HeaderLines()) output << line << '\n';
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (std::size_t i = 0; i < mesh.VertexCount(); ++i)
    {
        const auto &vertex = mesh.Vertices()[i];
        output << "Vertex " << i << ' ' << vertex[0] << ' '
               << vertex[1] << ' ' << vertex[2] << '\n';
    }
    for (std::size_t i = 0; i < firstDirectedEdges.size(); ++i)
        output << "FirstDirectedEdge " << i << ' ' << firstDirectedEdges[i] << '\n';
    for (std::size_t i = 0; i < mesh.FaceCount(); ++i)
    {
        const auto &face = mesh.Faces()[i];
        output << "Face " << i << ' ' << face[0] << ' ' << face[1] << ' ' << face[2] << '\n';
    }
    for (std::size_t i = 0; i < otherHalves.size(); ++i)
        output << "OtherHalf " << i << ' ' << otherHalves[i] << '\n';
}

void DirectedEdgeMesh::WriteDiagnostics(std::ostream &output) const
{
    const auto isolated = std::count(firstDirectedEdges.begin(), firstDirectedEdges.end(), -1);
    if (UnpairedEdgeCount() || isolated)
        output << "converter: warning: " << UnpairedEdgeCount()
               << " unpaired directed edges, " << isolated << " isolated vertices.\n";
}

