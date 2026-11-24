#include "fs_utils.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <stdexcept>

#include "sha256.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace {

#ifdef _WIN32

// RAII-обёртка над HANDLE поиска: закрывается автоматически.
struct FindCloser {
    void operator()(void* h) const {
        if (h != INVALID_HANDLE_VALUE) FindClose(h);
    }
};

void walk(const std::string& root, const std::string& rel, std::vector<std::string>& out) {
    const std::string dir = rel.empty() ? root : root + "/" + rel;
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA((dir + "/*").c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) {
        if (rel.empty()) throw std::runtime_error("cannot open directory: " + root);
        return;
    }
    std::unique_ptr<void, FindCloser> guard(h);

    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;  // ссылки

        const std::string childRel = rel.empty() ? name : rel + "/" + name;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk(root, childRel, out);
        } else {
            out.push_back(childRel);
        }
    } while (FindNextFileA(h, &data));
}

#else

// RAII-обёртка над DIR*: closedir вызывается автоматически, даже при исключении.
struct DirCloser {
    void operator()(DIR* d) const {
        if (d) closedir(d);
    }
};

void walk(const std::string& root, const std::string& rel, std::vector<std::string>& out) {
    const std::string dir = rel.empty() ? root : root + "/" + rel;
    std::unique_ptr<DIR, DirCloser> d(opendir(dir.c_str()));
    if (!d) {
        if (rel.empty()) throw std::runtime_error("cannot open directory: " + root);
        return;  // вложенный каталог без прав доступа просто пропускаем
    }

    while (dirent* entry = readdir(d.get())) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        const std::string childRel = rel.empty() ? name : rel + "/" + name;
        struct stat st;
        if (lstat((root + "/" + childRel).c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk(root, childRel, out);
        } else if (S_ISREG(st.st_mode)) {
            out.push_back(childRel);
        }
        // символические ссылки и спецфайлы пропускаем
    }
}

#endif

}  // namespace

std::vector<std::string> listFilesRecursive(const std::string& root) {
    std::vector<std::string> files;
    walk(root, "", files);
    std::sort(files.begin(), files.end());
    return files;
}

bool hashFile(const std::string& path, std::string& outHex, std::string& error) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        error = "cannot open file";
        return false;
    }

    Sha256 sha;
    std::vector<char> buf(64 * 1024);
    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = in.gcount();
        if (got > 0) {
            sha.update(reinterpret_cast<const std::uint8_t*>(buf.data()),
                       static_cast<std::size_t>(got));
        }
    }
    if (in.bad()) {
        error = "read error";
        return false;
    }
    outHex = sha.finalHex();
    return true;
}
