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

namespace
{

const char *const ReportFileName = "manifold test results.txt";

struct Result
{
    std::string verdict;
    std::string detail;
};

void Usage(std::ostream &output)
{
    output << "Usage: mesh_test model.tri|model.face|model.diredge [more files or directories]\n"
              "Tests every file, writes manifold test results.txt and reports the failing\n"
              "edge and vertex IDs of non-manifold meshes plus the genus of the others.\n";
}

std::string FileName(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Extension(const std::string &path)
{
    const std::size_t dot = path.find_last_of('.');
    const std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return std::string();
    std::string extension = path.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char letter)
                   { return static_cast<char>(std::tolower(letter)); });
    return extension;
}

bool IsSupported(const std::string &path)
{
    const std::string extension = Extension(path);
    return extension == "tri" || extension == "face" || extension == "diredge";
}

#ifdef _WIN32

bool IsDirectory(const std::string &path)
{
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void AppendDirectory(const std::string &directory, std::vector<std::string> &paths)
{
    WIN32_FIND_DATAA entry;
    const HANDLE search = FindFirstFileA((directory + "\\*").c_str(), &entry);
    if (search == INVALID_HANDLE_VALUE) return;
    do
    {
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        const std::string path = directory + "\\" + entry.cFileName;
        if (IsSupported(path)) paths.push_back(path);
    } while (FindNextFileA(search, &entry) != 0);
    FindClose(search);
}

#else

bool IsDirectory(const std::string &path)
{
    struct stat information;
    return stat(path.c_str(), &information) == 0 && S_ISDIR(information.st_mode);
}

void AppendDirectory(const std::string &directory, std::vector<std::string> &paths)
{
    DIR *handle = opendir(directory.c_str());
    if (handle == 0) return;
    while (const dirent *entry = readdir(handle))
    {
        const std::string path = directory + "/" + entry->d_name;
        if (IsSupported(path) && !IsDirectory(path)) paths.push_back(path);
    }
    closedir(handle);
}

#endif

std::vector<std::string> CollectInputFiles(int argc, char **argv)
{
    std::vector<std::string> paths;
    for (int argument = 1; argument < argc; ++argument)
    {
        const std::string path = argv[argument];
        if (IsDirectory(path)) AppendDirectory(path, paths);
        else paths.push_back(path);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

FaceIndexedMesh ReadMesh(const std::string &path)
{
    std::ifstream input(path.c_str());
    if (!input) throw std::runtime_error("cannot open input file");
    if (Extension(path) == "tri") return FaceIndexedMesh::ReadTriangleSoup(input);

    // .face and .diredge share their geometry records; rebuild the Task I connectivity.
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

Result Analyse(const std::string &path)
{
    Result result;
    try
    {
        const MeshAnalysis analysis(ReadMesh(path));
        if (analysis.IsManifold())
        {
            result.verdict = "Yes";
            result.detail = "genus " + std::to_string(analysis.Genus());
        }
        else
        {
            result.verdict = "No";
            result.detail = analysis.Failure();
        }
    }
    catch (const std::exception &error)
    {
        result.verdict = "Error";
        result.detail = error.what();
    }
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"))
    {
        Usage(std::cout);
        return 0;
    }
    if (argc < 2)
    {
        Usage(std::cerr);
        return 1;
    }

    const std::vector<std::string> paths = CollectInputFiles(argc, argv);
    if (paths.empty())
    {
        std::cerr << "mesh_test: no .tri, .face or .diredge files found\n";
        return 1;
    }

    std::ostringstream report;
    report << "Model\tManifold\tDetail\n";
    for (const std::string &path : paths)
    {
        const Result result = Analyse(path);
        report << FileName(path) << '\t' << result.verdict << '\t' << result.detail << '\n';
    }

    std::ofstream output(ReportFileName);
    if (!output)
    {
        std::cerr << "mesh_test: cannot write " << ReportFileName << '\n';
        return 1;
    }
    output << report.str();
    output.close();
    if (!output)
    {
        std::cerr << "mesh_test: cannot finish writing " << ReportFileName << '\n';
        return 1;
    }
    std::cout << report.str();
    return 0;
}
