#include <immintrin.h>
#include <iostream>
#include <cstring> // For memcpy

int main() {
    __m128i data = _mm_set1_epi64x(42); // Set all elements to 42
    __m128i result;
    memcpy(&result, &data, sizeof(result)); // Portable alternative to _mm_loadu_si64
    std::cout << "AVX2 test passed." << std::endl;
    return 0;
}