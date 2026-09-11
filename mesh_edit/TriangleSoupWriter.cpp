#include "TriangleSoupWriter.h"

#include <iomanip>
#include <limits>

void WriteTriangleSoup(std::ostream &output, const FaceIndexedMesh &mesh)
{
    output << mesh.FaceCount() << '\n';
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (const FaceIndexedMesh::Face &face : mesh.Faces())
    {
        for (std::size_t corner = 0; corner < face.size(); ++corner)
        {
            const FaceIndexedMesh::Vertex &vertex = mesh.Vertices()[face[corner]];
            output << vertex[0] << ' ' << vertex[1] << ' ' << vertex[2] << '\n';
        }
    }
}
