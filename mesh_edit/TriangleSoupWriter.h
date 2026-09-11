#ifndef TRIANGLE_SOUP_WRITER_H
#define TRIANGLE_SOUP_WRITER_H

#include "../converter/FaceIndexedMesh.h"

#include <iosfwd>

// Writes the .tri handout format: the face count, then one vertex per line.
void WriteTriangleSoup(std::ostream &output, const FaceIndexedMesh &mesh);

#endif
