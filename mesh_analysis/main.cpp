#include "MeshAnalysis.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

using namespace std;

namespace
{

const char *const reportName = "manifold test results.txt";

struct Result
{
    string verdict;
    string detail;
};

void usage(ostream &out)
{
    out << "Usage: mesh_test model.tri|model.face|model.diredge [more files or directories]\n"
           "Tests every file, writes manifold test results.txt and reports the failing\n"
           "edge and vertex IDs of non-manifold meshes plus the genus of the others.\n";
}

string fileName(const string &path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == string::npos ? path : path.substr(slash + 1);
}

string extensionOf(const string &path)
{
    const size_t dot = path.find_last_of('.');
    const size_t slash = path.find_last_of("/\\");
    if (dot == string::npos || (slash != string::npos && dot < slash)) return string();
    string ext = path.substr(dot + 1);
    transform(ext.begin(), ext.end(), ext.begin(),
              [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool supported(const string &path)
{
    const string ext = extensionOf(path);
    return ext == "tri" || ext == "face" || ext == "diredge";
}

#ifdef _WIN32

bool isDir(const string &path)
{
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void addDir(const string &dir, vector<string> &paths)
{
    WIN32_FIND_DATAA entry;
    const HANDLE search = FindFirstFileA((dir + "\\*").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do
    {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        const string path = dir + "\\" + entry.cFileName;
        if (supported(path)) paths.push_back(path);
    } while (FindNextFileA(search, &entry) != 0);
    FindClose(search);
}

#else

bool isDir(const string &path)
{
    struct stat info;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

void addDir(const string &dir, vector<string> &paths)
{
    DIR *handle = opendir(dir.c_str());
    if (handle == 0) return;
    while (const dirent *entry = readdir(handle))
    {
        const string path = dir + "/" + entry->d_name;
        if (supported(path) && !isDir(path)) paths.push_back(path);
    }
    closedir(handle);
}

#endif

// handy: pass a folder and it picks up every mesh file inside
vector<string> collect(int argc, char **argv)
{
    vector<string> paths;
    for (int i = 1; i < argc; ++i)
    {
        const string path = argv[i];
        if (isDir(path)) addDir(path, paths);
        else paths.push_back(path);
    }
    sort(paths.begin(), paths.end());
    return paths;
}

FaceIndexedMesh readMesh(const string &path)
{
    ifstream in(path.c_str());
    if (!in) throw runtime_error("cannot open input file");
    if (extensionOf(path) == "tri") return FaceIndexedMesh::ReadTriangleSoup(in);

    // .face and .diredge share the geometry records, so keep those lines and
    // let ReadFace rebuild the Task I connectivity for us
    ostringstream kept;
    string line, kind;
    while (getline(in, line))
    {
        istringstream record(line);
        record >> kind;
        if (kind != "FirstDirectedEdge" && kind != "OtherHalf") kept << line << '\n';
    }
    istringstream rebuilt(kept.str());
    return FaceIndexedMesh::ReadFace(rebuilt);
}

Result analyse(const string &path)
{
    Result r;
    try
    {
        const MeshAnalysis m(readMesh(path));
        if (m.manifold()) { r.verdict = "Yes"; r.detail = "genus " + to_string(m.genus()); }
        else { r.verdict = "No"; r.detail = m.why(); }
    }
    catch (const exception &err)
    {
        r.verdict = "Error";
        r.detail = err.what();
    }
    return r;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (string(argv[1]) == "--help" || string(argv[1]) == "-h"))
    {
        usage(cout);
        return 0;
    }
    if (argc < 2) { usage(cerr); return 1; }

    const vector<string> paths = collect(argc, argv);
    if (paths.empty())
    {
        cerr << "mesh_analysis: no .tri, .face or .diredge files found\n";
        return 1;
    }

    ostringstream report;
    report << "Model\tManifold\tDetail\n";
    for (const string &path : paths)
    {
        const Result r = analyse(path);
        report << fileName(path) << '\t' << r.verdict << '\t' << r.detail << '\n';
    }

    ofstream out(reportName);
    if (!out)
    {
        cerr << "mesh_analysis: cannot write " << reportName << '\n';
        return 1;
    }
    out << report.str();
    out.close();
    if (!out)
    {
        cerr << "mesh_analysis: cannot finish writing " << reportName << '\n';
        return 1;
    }
    cout << report.str();
    return 0;
}
