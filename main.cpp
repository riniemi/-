// fim - File Integrity Monitor.
//
//   fim init  <каталог> <база> [-j N]   построить базу хешей
//   fim check <каталог> <база> [-j N]   сравнить каталог с базой
//
// Коды возврата: 0 - всё совпало, 1 - найдены различия, 2 - ошибка.

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "fs_utils.hpp"

namespace {

struct HashResult {
    std::string hash;
    std::string error;  // пусто, если всё хорошо
    bool ok;
    HashResult() : ok(false) {}
};

// Хеширует файлы параллельно. Каждый поток забирает следующий индекс из
// атомарного счётчика и пишет результат строго в свою ячейку вектора,
// поэтому гонок данных нет и мьютекс для результатов не нужен.
std::vector<HashResult> hashAll(const std::string& root,
                                const std::vector<std::string>& files,
                                unsigned threads) {
    std::vector<HashResult> results(files.size());
    std::atomic<std::size_t> next(0);

    auto worker = [&]() {
        for (;;) {
            const std::size_t i = next.fetch_add(1);
            if (i >= files.size()) break;
            HashResult& r = results[i];
            r.ok = hashFile(root + "/" + files[i], r.hash, r.error);
        }
    };

    if (threads < 1) threads = 1;
    if (threads > files.size()) threads = static_cast<unsigned>(files.size());
    if (threads <= 1) {
        worker();
        return results;
    }

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned t = 0; t < threads; ++t) pool.emplace_back(worker);
    for (std::size_t t = 0; t < pool.size(); ++t) pool[t].join();
    return results;
}

// Формат базы: "<sha256-hex>  <относительный путь>" - как у утилиты sha256sum.
bool saveDb(const std::string& dbPath, const std::map<std::string, std::string>& db) {
    std::ofstream out(dbPath.c_str(), std::ios::trunc);
    if (!out) return false;
    for (std::map<std::string, std::string>::const_iterator it = db.begin(); it != db.end(); ++it) {
        out << it->second << "  " << it->first << "\n";
    }
    return static_cast<bool>(out);
}

bool loadDb(const std::string& dbPath, std::map<std::string, std::string>& db) {
    std::ifstream in(dbPath.c_str());
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 67 || line[64] != ' ' || line[65] != ' ') continue;
        db[line.substr(66)] = line.substr(0, 64);
    }
    return true;
}

// Список файлов каталога без самой базы (если она лежит внутри).
std::vector<std::string> collectFiles(const std::string& root, const std::string& dbPath) {
    std::vector<std::string> all = listFilesRecursive(root);
    std::vector<std::string> files;
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (root + "/" + all[i] != dbPath) files.push_back(all[i]);
    }
    return files;
}

void printUsage() {
    std::cerr << "Usage:\n"
                 "  fim init  <dir> <db> [-j N]\n"
                 "  fim check <dir> <db> [-j N]\n";
}

int cmdInit(const std::string& root, const std::string& dbPath, unsigned threads) {
    const std::vector<std::string> files = collectFiles(root, dbPath);
    const std::vector<HashResult> results = hashAll(root, files, threads);

    std::map<std::string, std::string> db;
    int errors = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (results[i].ok) {
            db[files[i]] = results[i].hash;
        } else {
            std::cerr << "WARNING: " << files[i] << ": " << results[i].error << "\n";
            ++errors;
        }
    }

    if (!saveDb(dbPath, db)) {
        std::cerr << "ERROR: cannot write database: " << dbPath << "\n";
        return 2;
    }
    std::cout << "Database created: " << db.size() << " files hashed";
    if (errors) std::cout << ", " << errors << " skipped";
    std::cout << "\n";
    return errors ? 2 : 0;
}

int cmdCheck(const std::string& root, const std::string& dbPath, unsigned threads) {
    std::map<std::string, std::string> db;
    if (!loadDb(dbPath, db)) {
        std::cerr << "ERROR: cannot read database: " << dbPath << "\n";
        return 2;
    }

    const std::vector<std::string> files = collectFiles(root, dbPath);
    const std::vector<HashResult> results = hashAll(root, files, threads);

    int modified = 0, added = 0, removed = 0, errors = 0;
    std::map<std::string, bool> seen;

    for (std::size_t i = 0; i < files.size(); ++i) {
        const std::string& path = files[i];
        seen[path] = true;

        if (!results[i].ok) {
            std::cout << "ERROR     " << path << " (" << results[i].error << ")\n";
            ++errors;
            continue;
        }
        std::map<std::string, std::string>::const_iterator it = db.find(path);
        if (it == db.end()) {
            std::cout << "NEW       " << path << "\n";
            ++added;
        } else if (it->second != results[i].hash) {
            std::cout << "MODIFIED  " << path << "\n";
            ++modified;
        }
    }
    for (std::map<std::string, std::string>::const_iterator it = db.begin(); it != db.end(); ++it) {
        if (!seen.count(it->first)) {
            std::cout << "DELETED   " << it->first << "\n";
            ++removed;
        }
    }

    std::cout << "---\nchecked: " << files.size() << ", modified: " << modified
              << ", new: " << added << ", deleted: " << removed;
    if (errors) std::cout << ", errors: " << errors;
    std::cout << "\n";

    if (errors) return 2;
    return (modified || added || removed) ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        printUsage();
        return 2;
    }

    const std::string mode = argv[1];
    const std::string root = argv[2];
    const std::string dbPath = argv[3];

    unsigned threads = std::thread::hardware_concurrency();
    if (threads == 0) threads = 1;

    for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-j" && i + 1 < argc) {
            const int n = std::atoi(argv[++i]);
            if (n < 1) {
                std::cerr << "ERROR: -j expects a positive number\n";
                return 2;
            }
            threads = static_cast<unsigned>(n);
        } else {
            printUsage();
            return 2;
        }
    }

    try {
        if (mode == "init") return cmdInit(root, dbPath, threads);
        if (mode == "check") return cmdCheck(root, dbPath, threads);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 2;
    }

    printUsage();
    return 2;
}
