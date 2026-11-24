#pragma once

#include <string>
#include <vector>

// Рекурсивно обходит каталог root и возвращает отсортированный список
// обычных файлов в виде относительных путей с разделителем '/'.
// Символические ссылки пропускаются (защита от циклов).
// Бросает std::runtime_error, если root открыть нельзя.
std::vector<std::string> listFilesRecursive(const std::string& root);

// Считает SHA-256 файла, читая его блоками по 64 КиБ.
// При ошибке возвращает false и заполняет error.
bool hashFile(const std::string& path, std::string& outHex, std::string& error);
