# University of Leeds 2024-25
# COMP 5893M Assignment 1

These command-line utilities convert .tri to .face and .face to .diredge. Keep the face2faceindex and faceindex2directededge directories beside each other.

UNIVERSITY LINUX:
=================

Run the following commands separately inside each program's directory:

qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt"
qmake
make

To convert a model, run from the repository root:

./face2faceindex/face2faceindex handout_models/tetrahedron.tri
./faceindex2directededge/faceindex2directededge handout_models/tetrahedron.face

Output files are written beside the input with .face or .diredge suffixes. Existing output files will not be overwritten.

TASK II: MANIFOLD TESTING
=========================

The C++ program in mesh_processing reuses the Task I classes and needs only the C++ STL. Keep all three source directories beside each other. From mesh_processing, build using the supplied project:

qmake mesh_processing.pro
make

To regenerate a project using the handout's qmake -project stage, use:

qmake -project -o manifoldtest.pro "CONFIG += console c++11" "CONFIG -= app_bundle qt" "DEFINES += TASK1_LIBRARY"
qmake manifoldtest.pro
make

TASK1_LIBRARY excludes the two Task I main functions when sharing their classes.

Run with exactly one .tri, .face or .diredge model:

./manifoldtest ../handout_models/tetrahedron.tri

The program prints the model name and Yes/No, and writes the same result to "manifold test results.txt" in the current working directory. Each successful run overwrites the report with only that model's result. A read error leaves any existing report unchanged. File errors return a nonzero exit status.
