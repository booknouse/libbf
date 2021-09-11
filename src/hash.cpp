#include <bf/hash.hpp>
#include <stdexcept>
#include <memory>
#include <string.h>
#include <cassert>
#include <iostream>

namespace bf {

default_hash_function::default_hash_function(size_t seed) : h3_(seed) {
}

size_t default_hash_function::operator()(object const& o) const {
  // FIXME: fall back to a generic universal hash function (e.g., HMAC/MD5) for
  // too large objects.
  if (o.size() > max_obj_size)
    throw std::runtime_error("object too large");
  return o.size() == 0 ? 0 : h3_(o.data(), o.size());
}

unsigned char* default_hash_function::serialize(unsigned char* buf){
  return h3_.serialize(buf);
}

int default_hash_function::fromBuf(unsigned char*buf, unsigned int len){
  return h3_.fromBuf(buf,len);
}

default_hasher::default_hasher(std::vector<std::shared_ptr<default_hash_function>>& fns)
    : fns_(std::move(fns)) {
}

std::vector<digest> default_hasher::operator()(object const& o) const {
  std::vector<digest> d(fns_.size());
  for (size_t i = 0; i < fns_.size(); ++i)
    d[i] = (*fns_[i])(o);
  return d;
}

unsigned char* default_hasher::serialize(unsigned char* buf) {
  unsigned char type = 0;
  unsigned int sz = sizeof(type);
  memmove(buf, &type, sz);
  buf += sz;
  unsigned int ct = fns_.size();
  sz = sizeof(ct);
  memmove(buf, &ct, sz);
  buf += sz;
  for (auto& fn : fns_) {
    auto fn_sz = fn->serializedSize();
    memmove(buf, &fn_sz, sizeof(fn_sz));
    buf += sizeof(fn_sz);
    buf = fn->serialize(buf);
  }
  return buf;
}

unsigned int default_hasher::serializedSize() const {
  unsigned int sz = sizeof(unsigned char) + sizeof(unsigned int);
  sz += fns_.size() * sizeof(unsigned int);
  for (auto& fn : fns_) {
    sz += fn->serializedSize();
  }
  return sz;
}

int default_hasher::fromBuf(unsigned char* buf, unsigned int len) {
  auto buf_start = buf;
  if (*buf != 0)
    return 1;
  buf += sizeof(unsigned char);
  unsigned int* ct = reinterpret_cast<unsigned int*>(buf);
  buf += sizeof(unsigned int);
  for (unsigned int i = 0; i < *ct; i++) {
    unsigned int* h3_sz = reinterpret_cast<unsigned int*>(buf);
    buf += sizeof(unsigned int);
    auto fn = std::make_shared<default_hash_function>();
    if (fn->fromBuf(buf, *h3_sz) != 0)
      return 2;
    fns_.push_back(std::move(fn));
    buf += *h3_sz;
  }
  if (buf - buf_start != len)
    return 3;
  return 0;
}

double_hasher::double_hasher(size_t k, std::shared_ptr<default_hash_function>& h1, std::shared_ptr<default_hash_function>& h2)
    : k_(k), h1_(std::move(h1)), h2_(std::move(h2)) {
}

std::vector<digest> double_hasher::operator()(object const& o) const {
  auto d1 = (*h1_)(o);
  auto d2 = (*h2_)(o);
  std::vector<digest> d(k_);
  for (size_t i = 0; i < d.size(); ++i)
    d[i] = d1 + i * d2;
  return d;
}

unsigned char* double_hasher::serialize(unsigned char* buf) {
  unsigned char type = 1;
  unsigned int sz = sizeof(type);
  memmove(buf, &type, sz);
  buf += sz;
  sz = sizeof(k_);
  memmove(buf, &k_, sz);
  buf += sz;
  auto h1_sz = h1_->serializedSize();
  memmove(buf, &h1_sz, sizeof(h1_sz));
  buf += sizeof(h1_sz);
  buf = h1_->serialize(buf);
  auto h2_sz = h2_->serializedSize();
  memmove(buf, &h2_sz, sizeof(h2_sz));
  buf += sizeof(h2_sz);
  buf = h2_->serialize(buf);
  return buf;
}

unsigned int double_hasher::serializedSize() const {
  unsigned int total_sz =
    sizeof(unsigned char) + sizeof(k_) + 2 * sizeof(unsigned int);
  total_sz += h1_->serializedSize();
  total_sz += h2_->serializedSize();
  return total_sz;
}

int double_hasher::fromBuf(unsigned char* buf, unsigned int len) {
  auto buf_start = buf;
  if (*buf != 1)
    return 1;
  buf += sizeof(unsigned char);
  k_ = *reinterpret_cast<size_t*>(buf);
  buf += sizeof(size_t);
  {
    unsigned int* h3_sz = reinterpret_cast<unsigned int*>(buf);
    buf += sizeof(unsigned int);
    h1_ = std::make_shared<default_hash_function>();
    if (h1_->fromBuf(buf, *h3_sz) != 0)
      return 2;
    buf += *h3_sz;
  }
  {
    unsigned int* h3_sz = reinterpret_cast<unsigned int*>(buf);
    buf += sizeof(unsigned int);
    h2_ = std::make_shared<default_hash_function>();
    if (h2_->fromBuf(buf, *h3_sz) != 0)
      return 3;
    buf += *h3_sz;
  }
  if (buf - buf_start != len)
    return 4;
  return 0;
}

std::shared_ptr<base_hasher> make_hasher(size_t k, size_t seed, bool double_hashing) {
  assert(k > 0);
  std::minstd_rand0 prng(seed);
  if (double_hashing) {
    auto h1 = std::make_shared<default_hash_function>(prng());
    auto h2 = std::make_shared<default_hash_function>(prng());
    return std::make_shared<double_hasher>(k, h1, h2);
  } else {
    std::vector<std::shared_ptr<default_hash_function>> fns(k);
    for (size_t i = 0; i < k; ++i) {
      fns[i] = std::make_shared<default_hash_function>(prng());
    }
    return std::make_shared<default_hasher>(fns);
  }
}

} // namespace bf
