#ifndef NACHOS_FILESYS_FILE_PATH__HH
#define NACHOS_FILESYS_FILE_PATH__HH

#include <list>
#include <string>

class FilePath {
public:
    void Merge(const char* subpath);

    inline std::list<std::string>& List() { return path; }

    std::string Split();

    std::string GetPath();

    void Print();

private:
    std::list<std::string> path;
};

#endif