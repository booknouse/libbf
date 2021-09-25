#ifndef BF_HASH_POLICY_HPP
#define BF_HASH_POLICY_HPP
#include <bf/h3.hpp>
#include <bf/object.hpp>
#include <functional>
#include <memory>

namespace bf {

/// The hash digest type.
typedef size_t digest;

/// The hash function type.
typedef std::function<digest(object const&)> hash_function;

/// A function that hashes an object *k* times.
typedef std::function<std::vector<digest>(object const&)> hasher;

class default_hash_function
{
public:
  constexpr static size_t max_obj_size = 36;
  default_hash_function()=default;
  explicit default_hash_function(size_t seed);

  size_t operator()(object const& o) const;

  unsigned char* serialize(unsigned char* buf);
  unsigned int serializedSize() const {
    return h3_.serializedSize();
  }
  int fromBuf(unsigned char*, unsigned int len);

private:
  h3<size_t, max_obj_size> h3_;
};

class base_hasher{
public:
  base_hasher()=default;
  virtual std::vector<digest> operator()(object const& o) const = 0;
  virtual unsigned char* serialize(unsigned char* buf) = 0;
  virtual unsigned int serializedSize() const = 0;
  virtual int fromBuf(unsigned char*, unsigned int) = 0;
};

class ap_hasher : public base_hasher {
public:
  ap_hasher() = default;
  ap_hasher(unsigned short idx_);
  std::vector<digest> operator()(object const& o) const override;
  unsigned char* serialize(unsigned char* buf) override;
  unsigned int serializedSize() const override;
  int fromBuf(unsigned char*, unsigned int len) override;
private:
  unsigned short less_than_idx;
};

/// A hasher which hashes an object *k* times.
class default_hasher : public base_hasher
{
public:
  default_hasher()=default;
  default_hasher(std::vector<std::shared_ptr<default_hash_function>>& fns);

  std::vector<digest> operator()(object const& o) const override;

  unsigned char* serialize(unsigned char* buf) override;
  unsigned int serializedSize() const override;
  int fromBuf(unsigned char*, unsigned int len) override;
private:
  std::vector<std::shared_ptr<default_hash_function> > fns_;
};

/// A hasher which hashes an object two times and generates *k* digests through
/// a linear combinations of the two digests.
class double_hasher : public base_hasher
{
public:
  double_hasher()=default;
  double_hasher(size_t k, std::shared_ptr<default_hash_function>& h1,
                std::shared_ptr<default_hash_function>& h2);

  std::vector<digest> operator()(object const& o) const override;

  unsigned char* serialize(unsigned char* buf) override;
  unsigned int serializedSize() const override;
  int fromBuf(unsigned char*, unsigned int len) override;

private:
  size_t k_;
  std::shared_ptr<default_hash_function>  h1_;
  std::shared_ptr<default_hash_function>  h2_;
};

class hasher_factory {
public:
  static std::shared_ptr<base_hasher> createHasher(unsigned char* type) {
    switch (*type) {
      case 0:
        return std::make_shared<default_hasher>();
      case 1:
        return std::make_shared<double_hasher>();
      case 2:
        return std::make_shared<ap_hasher>();
        break;
      default:
        return nullptr;
    }
  }
};
/// Creates a default or double hasher with the default hash function, using
/// seeds from a linear congruential PRNG.
///
/// @param k The number of hash functions to use.
///
/// @param seed The initial seed of the PRNG.
///
/// @param double_hashing If `true`, the function constructs a ::double_hasher
/// and a ::default_hasher otherwise.
///
/// @return A ::hasher with the *k* hash functions.
///
/// @pre `k > 0`
std::shared_ptr<base_hasher> make_hasher(size_t k, size_t seed = 0, bool double_hashing = false);
} // namespace bf

#endif
