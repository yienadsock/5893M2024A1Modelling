TEMPLATE = app
TARGET = manifoldtest
CONFIG += console c++11
CONFIG -= app_bundle qt
DEFINES += TASK1_LIBRARY
SOURCES += MeshProcessing.cpp ../face2faceindex/FaceIndexedMesh.cpp ../faceindex2directededge/DirectedEdgeMesh.cpp
HEADERS += ../face2faceindex/FaceIndexedMesh.h ../faceindex2directededge/DirectedEdgeMesh.h
