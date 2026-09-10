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
