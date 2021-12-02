#include <bf/bloom_filter/basic.hpp>
#include <memory>
#include <cassert>
#include <cmath>

namespace bf {

size_t basic_bloom_filter::m(double fp, size_t capacity) {
  auto ln2 = std::log(2);
  return std::ceil(-(capacity * std::log(fp) / ln2 / ln2));
}

size_t basic_bloom_filter::k(size_t cells, size_t capacity) {
  auto frac = static_cast<double>(cells) / static_cast<double>(capacity);
  return std::ceil(frac * std::log(2));
}

basic_bloom_filter::basic_bloom_filter(std::shared_ptr<base_hasher> h, size_t cells, bool partition)
    : hasher_(std::move(h)), bits_(cells), partition_(partition) {
}

basic_bloom_filter::basic_bloom_filter(double fp, size_t capacity, size_t seed,
                                       bool double_hashing, bool partition)
    : partition_(partition) {
  auto required_cells = m(fp, capacity);
  auto optimal_k = k(required_cells, capacity);
  if (partition_)
    required_cells += optimal_k - required_cells % optimal_k;
  bits_.resize(required_cells);
  hasher_ = make_hasher(optimal_k, seed, double_hashing);
}

basic_bloom_filter::basic_bloom_filter(std::shared_ptr<base_hasher> h, bitvector b)
    : hasher_(std::move(h)), bits_(std::move(b)) {
}

basic_bloom_filter::basic_bloom_filter(basic_bloom_filter&& other)
    : hasher_(std::move(other.hasher_)), bits_(std::move(other.bits_)) ,partition_(other.partition_){
}

basic_bloom_filter::basic_bloom_filter(const basic_bloom_filter& other): hasher_(other.hasher_), bits_(other.bits_),  partition_(other.partition_){
}
void basic_bloom_filter::add(object const& o) {
  auto digests = (*hasher_)(o);
  if (partition_) {
    assert(bits_.size() % digests.size() == 0);
    auto parts = bits_.size() / digests.size();
    for (size_t i = 0; i < digests.size(); ++i)
      bits_.set(i * parts + (digests[i] % parts));
  } else {
    for (auto d : digests)
      bits_.set(d % bits_.size());
  }
}

size_t basic_bloom_filter::lookup(object const& o) const {
  auto digests = (*hasher_)(o);
  if (partition_) {
    assert(bits_.size() % digests.size() == 0);
    auto parts = bits_.size() / digests.size();
    for (size_t i = 0; i < digests.size(); ++i)
      if (!bits_[i * parts + (digests[i] % parts)])
        return 0;
  } else {
    for (auto d : digests)
      if (!bits_[d % bits_.size()])
        return 0;
  }

  return 1;
}

void basic_bloom_filter::clear() {
  bits_.reset();
}

void basic_bloom_filter::remove(object const& o) {
  for (auto d : (*hasher_)(o))
    bits_.reset(d % bits_.size());
}

void basic_bloom_filter::swap(basic_bloom_filter& other) {
  using std::swap;
  swap(hasher_, other.hasher_);
  swap(bits_, other.bits_);
}

bitvector const& basic_bloom_filter::storage() const {
  return bits_;
}
std::shared_ptr<base_hasher> const& basic_bloom_filter::hasher_function() const {
  return hasher_;
}

char* basic_bloom_filter::serialize(char* buf) {
  auto hasher_sz = hasher_->serializedSize();
  memmove(buf, &hasher_sz, sizeof(hasher_sz));
  buf += sizeof(hasher_sz);
  buf = hasher_->serialize(buf);
  auto bits_sz = bits_.serializedSize();
  memmove(buf, &bits_sz, sizeof(bits_sz));
  buf += sizeof(bits_sz);
  buf = bits_.serialize(buf);
  memmove(buf, &partition_, sizeof(partition_));
  return buf + sizeof(partition_);
}
unsigned int basic_bloom_filter::serializedSize() const {
  return sizeof(unsigned int) * 2 + hasher_->serializedSize()
         + bits_.serializedSize() + sizeof(partition_);
}
int basic_bloom_filter::fromBuf(char* buf, unsigned int len) {
  auto buf_start = buf;
  unsigned int* hasher_sz = reinterpret_cast<unsigned int*>(buf);
  buf += sizeof(unsigned int);
  hasher_ = hasher_factory::createHasher(buf);
  if(!hasher_)
    return 1;
  if (hasher_->fromBuf(buf, *hasher_sz) != 0)
    return 2;
  buf += *hasher_sz;
  unsigned int* cells_sz = reinterpret_cast<unsigned int*>(buf);
  buf += sizeof(unsigned int);
  if (bits_.fromBuf(buf, *cells_sz) != 0)
    return 3;
  buf += *cells_sz;
  memmove(&partition_, buf, sizeof(partition_));
  buf += sizeof(partition_);
  if (buf - buf_start != len)
    return 4;
  return 0;
}
} // namespace bf
