TEMPLATE = app
TARGET = faceindex2directededge
CONFIG += console c++11
CONFIG -= app_bundle qt
SOURCES += main.cpp DirectedEdgeMesh.cpp FaceFileReader.cpp
HEADERS += DirectedEdgeMesh.h ../face2faceindex/FaceIndexedMesh.h
