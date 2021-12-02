#ifndef BF_H3_HPP
#define BF_H3_HPP

#include <limits>
#include <random>
#include <string.h>

namespace bf {

/// An implementation of the H3 hash function family.
template <typename T, int N>
class h3
{
  static size_t const bits_per_byte =
    std::numeric_limits<unsigned char>::digits;

public:
  constexpr static size_t byte_range =
    std::numeric_limits<unsigned char>::max() + 1;
  h3()=default;
  explicit h3(T seed ):bytes_(N*byte_range) {
    T bits[N * bits_per_byte];
    std::minstd_rand0 prng(seed);
    for (size_t bit = 0; bit < N * bits_per_byte; ++bit) {
      bits[bit] = 0;
      for (size_t i = 0; i < sizeof(T) / 2; i++)
        bits[bit] = (bits[bit] << 16) | (prng() & 0xFFFF);
    }

    for (size_t byte = 0; byte < N; ++byte)
      for (size_t val = 0; val < byte_range; ++val) {
        auto byte_idx = byte * byte_range + val;
        // bytes_[byte][val] = 0;
        bytes_[byte_idx] = 0;
        for (size_t bit = 0; bit < bits_per_byte; ++bit)
          if (val & (1 << bit))
            bytes_[byte_idx] ^= bits[byte * bits_per_byte + bit];
      }
  }

  T operator()(void const* data, size_t size, size_t offset = 0) const
  {
    auto *p = static_cast<unsigned char const*>(data);
    T result = 0;
    // Duff's Device.
    auto n = (size + 7) / 8;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
    switch (size % 8) 
    {
      case 0:	do { result ^= bytes_[offset++ *byte_range + *p++];
        case 7:      result ^= bytes_[offset++ *byte_range + *p++];
        case 6:      result ^= bytes_[offset++ *byte_range + *p++];
        case 5:      result ^= bytes_[offset++ *byte_range + *p++];
        case 4:      result ^= bytes_[offset++ *byte_range + *p++];
        case 3:      result ^= bytes_[offset++ *byte_range + *p++];
        case 2:      result ^= bytes_[offset++ *byte_range + *p++];
        case 1:      result ^= bytes_[offset++ *byte_range + *p++];
              } while ( --n > 0 );
    }
#pragma GCC diagnostic pop
    return result;
  }

  char* serialize(char* buf) {
    unsigned int sz = bytes_.size() * sizeof(T);
    memmove(buf, bytes_.data(), sz);
    return buf + sz;
  }

  unsigned int serializedSize() const {
    return bytes_.size() * sizeof(T);
  }

  int fromBuf(const char* buf, unsigned int len) {
    bytes_.assign(reinterpret_cast<const T*>(buf), reinterpret_cast<const T*>(buf + len));
    return 0;
  }

private:
  //T bytes_[N][byte_range];
  std::vector<T> bytes_;
};

} // namespace bf

#endif 
