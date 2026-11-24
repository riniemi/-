// Минимальные тесты без внешних библиотек: ошибка -> код возврата 1.
#include <cstdint>
#include <iostream>
#include <string>

#include "../src/sha256.hpp"

static int failures = 0;

static void expectEq(const std::string& name, const std::string& got, const std::string& want) {
    if (got == want) {
        std::cout << "[ OK ] " << name << "\n";
    } else {
        std::cout << "[FAIL] " << name << "\n  got:  " << got << "\n  want: " << want << "\n";
        ++failures;
    }
}

int main() {
    // Стандартные тестовые векторы NIST.
    expectEq("empty string", Sha256::hashString(""),
             "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    expectEq("abc", Sha256::hashString("abc"),
             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    expectEq("two-block message",
             Sha256::hashString("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    // Граница паддинга: 55, 56 и 64 байта (особые случаи длины блока).
    expectEq("55 x 'a'", Sha256::hashString(std::string(55, 'a')),
             "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    expectEq("56 x 'a'", Sha256::hashString(std::string(56, 'a')),
             "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    expectEq("64 x 'a'", Sha256::hashString(std::string(64, 'a')),
             "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");

    // Миллион символов 'a', поданных кусками нечётного размера (проверка update()).
    {
        Sha256 h;
        const std::string chunk(997, 'a');
        std::size_t left = 1000000;
        while (left > 0) {
            const std::size_t n = left < chunk.size() ? left : chunk.size();
            h.update(reinterpret_cast<const std::uint8_t*>(chunk.data()), n);
            left -= n;
        }
        expectEq("1,000,000 x 'a' (chunked)", h.finalHex(),
                 "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    }

    std::cout << (failures ? "FAILED\n" : "ALL TESTS PASSED\n");
    return failures ? 1 : 0;
}
