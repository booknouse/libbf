#include <bf/counter_vector.hpp>

#include <cassert>
#include <string.h>

namespace bf {

counter_vector::counter_vector(size_t cells, size_t width)
    : bits_(cells * width), width_(width) {
  assert(cells > 0);
  assert(width > 0);
}

counter_vector& counter_vector::operator|=(counter_vector const& other) {
  assert(size() == other.size());
  assert(width() == other.width());
  for (size_t cell = 0; cell < size(); ++cell) {
    bool carry = false;
    size_t lsb = cell * width_;
    for (size_t i = 0; i < width_; ++i) {
      bool b1 = bits_[lsb + i];
      bool b2 = other.bits_[lsb + i];
      bits_[lsb + i] = b1 ^ b2 ^ carry;
      carry = (b1 && b2) || (carry && (b1 != b2));
    }
    if (carry)
      for (size_t i = 0; i < width_; ++i)
        bits_.set(lsb + i);
  }
  return *this;
}

counter_vector operator|(counter_vector const& x, counter_vector const& y) {
  counter_vector cv(x);
  return cv |= y;
}

bool counter_vector::increment(size_t cell, size_t value) {
  assert(cell < size());
  assert(value != 0);
  size_t lsb = cell * width_;
  bool carry = false;
  for (size_t i = 0; i < width_; ++i) {
    bool b1 = bits_[lsb + i];
    bool b2 = value & (1 << i);
    bits_[lsb + i] = b1 ^ b2 ^ carry;
    carry = (b1 && b2) || (carry && (b1 != b2));
  }
  if (carry)
    for (size_t i = 0; i < width_; ++i)
      bits_[lsb + i] = true;
  return !carry;
}

bool counter_vector::decrement(size_t cell, size_t value) {
  assert(cell < size());
  assert(value != 0);
  value = ~value + 1; // A - B := A + ~B + 1
  bool carry = false;
  size_t lsb = cell * width_;
  for (size_t i = 0; i < width_; ++i) {
    bool b1 = bits_[lsb + i];
    bool b2 = value & (1 << i);
    bits_[lsb + i] = b1 ^ b2 ^ carry;
    carry = (b1 && b2) || (carry && (b1 != b2));
  }
  return carry;
}

size_t counter_vector::count(size_t cell) const {
  assert(cell < size());
  size_t cnt = 0, order = 1;
  size_t lsb = cell * width_;
  for (auto i = lsb; i < lsb + width_; ++i, order <<= 1)
    if (bits_[i])
      cnt |= order;
  return cnt;
}

void counter_vector::set(size_t cell, size_t value) {
  assert(cell < size());
  assert(value <= max());
  bitvector bits(width_, value);
  auto lsb = cell * width_;
  for (size_t i = 0; i < width_; ++i)
    bits_[lsb + i] = bits[i];
}

void counter_vector::clear() {
  bits_.reset();
}

size_t counter_vector::size() const {
  return bits_.size() / width_;
}

size_t counter_vector::max() const {
  using limits = std::numeric_limits<size_t>;
  return limits::max() >> (limits::digits - width());
}

size_t counter_vector::width() const {
  return width_;
}

char* counter_vector::serialize(char* buf) {
  auto bits_sz = bits_.serializedSize();
  memmove(buf, &bits_sz, sizeof(bits_sz));
  buf += sizeof(bits_sz);
  buf = bits_.serialize(buf);
  const unsigned int width_sz = sizeof(width_);
  memmove(buf, &width_, width_sz);
  return buf + width_sz;
}

unsigned int counter_vector::serializedSize() const {
  return sizeof(unsigned int) + bits_.serializedSize() + sizeof(width_);
}

int counter_vector::fromBuf(const char* buf, unsigned int len) {
  auto buf_start = buf;
  auto bits_sz = reinterpret_cast<const unsigned int*>(buf);
  buf += sizeof(unsigned int);
  if (bits_.fromBuf(buf, *bits_sz) != 0)
    return 1;
  buf += *bits_sz;
  const unsigned int width_sz = sizeof(width_);
  memmove(&width_, buf, width_sz);
  buf += width_sz;
  if (buf - buf_start != len)
    return 2;
  return 0;
}
} // namespace bf
