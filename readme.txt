Building instructions & Usage
--------------------------

University Linux: run these commands in order, starting from the repository root.

For Task 1

cd converter
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt"
qmake
make

./converter tri2face ../handout_models/tetrahedron.tri
./converter face2diredge ../handout_models/tetrahedron.face

For Task 2~3

cd ../mesh_analysis
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt" "SOURCES += ../converter/FaceIndexedMesh.cpp ../converter/DirectedEdgeMesh.cpp"
qmake
make

./mesh_analysis ../handout_models

For Task 4~5

cd ../mesh_edit
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt" "SOURCES += ../converter/FaceIndexedMesh.cpp ../converter/DirectedEdgeMesh.cpp"
qmake
make

./mesh_edit repair ../handout_models/hamish.tri
./mesh_edit simplify ../handout_models/hamish_fixed.tri

The analysis report is saved as mesh_analysis/manifold test results.txt.
Converted and edited models are saved beside the input; choose a new output path if a file already exists.
