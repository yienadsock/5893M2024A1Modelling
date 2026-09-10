#include "../faceindex2directededge/DirectedEdgeMesh.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Course convention: a closed, consistently oriented triangle surface.
bool DirectedEdgeMesh::IsManifold() const
{
    if (mesh.FaceCount() == 0 || UnpairedEdgeCount() != 0) return false;

    std::vector<std::size_t> degree(mesh.VertexCount(), 0);
    for (const auto &face : mesh.Faces())
        for (std::size_t vertex : face) ++degree[vertex];

    for (std::size_t vertex = 0; vertex < mesh.VertexCount(); ++vertex)
    {
        const EdgeId start = firstDirectedEdges[vertex];
        if (start == -1) return false; // Isolated vertex.
        EdgeId edge = start;
        std::size_t visited = 0;
        do
        {
            // Cross the edge, then take the next edge to stay at this vertex.
            const EdgeId opposite = otherHalves[edge];
            edge = 3 * (opposite / 3) + (opposite % 3 + 1) % 3;
            ++visited;
        } while (edge != start && visited <= degree[vertex]);
        // A second disconnected fan at the same vertex was not visited.
        if (edge != start || visited != degree[vertex]) return false;
    }
    return true;
}

FaceIndexedMesh ReadMesh(const std::string &path)
{
    std::ifstream input(path.c_str());
    if (!input) throw std::runtime_error("Cannot open input file.");
    if (path.substr(path.find_last_of('.') + 1) == "tri")
        return FaceIndexedMesh::ReadTriangleSoup(input);

    // .face and .diredge share their geometry records. Rebuild connectivity
    // with Task I so every supported input uses the same pairing convention.
    std::ostringstream geometry;
    std::string line, kind;
    while (std::getline(input, line))
    {
        std::istringstream record(line);
        record >> kind;
        if (kind != "FirstDirectedEdge" && kind != "OtherHalf")
            geometry << line << '\n';
    }
    std::istringstream indexed(geometry.str());
    return FaceIndexedMesh::ReadFace(indexed);
}

int main(int argc, char **argv)
{
    const bool help = argc == 2 && std::string(argv[1]) == "--help";
    if (argc != 2 || help)
    {
        (help ? std::cout : std::cerr)
            << "Usage: manifoldtest model.tri|model.face|model.diredge\n";
        return help ? 0 : 1;
    }
    const std::string path = argv[1];
    try
    {
        const DirectedEdgeMesh mesh(ReadMesh(path));
        const std::string result = "Model\tManifold\n"
            + path.substr(path.find_last_of("/\\") + 1) + '\t'
            + (mesh.IsManifold() ? "Yes\n" : "No\n");
        // Replace the report only after this model has been read and tested.
        std::ofstream report("manifold test results.txt");
        report << result;
        report.close();
        if (!report) throw std::runtime_error("Cannot write manifold test results.txt.");
        std::cout << result;
    }
    catch (const std::exception &error)
    {
        std::cerr << path << ": " << error.what() << '\n';
        return 1;
    }
    return 0;
}
