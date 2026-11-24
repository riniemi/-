#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Собственная реализация SHA-256 (FIPS 180-4), без сторонних библиотек.
class Sha256 {
public:
    Sha256();

    // Можно вызывать много раз: данные подаются блоками произвольного размера.
    void update(const std::uint8_t* data, std::size_t len);

    // Завершает вычисление и возвращает хеш в виде 64 hex-символов.
    // После вызова объект использовать нельзя (бросает std::logic_error).
    std::string finalHex();

    // Удобная обёртка для строк (используется в тестах).
    static std::string hashString(const std::string& s);

private:
    void processBlock(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::size_t bufferLen_;
    std::uint64_t totalLen_;  // всего байт на входе
    bool finalized_;
};
