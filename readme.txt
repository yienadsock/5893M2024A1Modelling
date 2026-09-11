Windows build instructions
--------------------------
cd ..\converter
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt"
qmake
mingw32-make
.\release\converter.exe tri2face ..\handout_models\tetrahedron.tri
.\release\converter.exe face2diredge ..\handout_models\tetrahedron.face

cd ..\mesh_analysis
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt"
qmake
mingw32-make
.\release\mesh_analysis.exe ..\handout_models

cd ..\mesh_edit
qmake -project "CONFIG += console c++11" "CONFIG -= app_bundle qt"
qmake
mingw32-make
.\release\mesh_edit.exe repair ..\handout_models\hamish.tri
.\release\mesh_edit.exe simplify ..\handout_models\hamish_fixed.tri


cd ..\triangle_renderer
qmake -project "QT += core gui widgets opengl openglwidgets" "LIBS += -lopengl32 -lglu32"
qmake
mingw32-make
.\release\triangle_renderer.exe ..\handout_models\cube.tri