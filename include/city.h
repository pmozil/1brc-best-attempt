#pragma once

#ifndef CITY_INFO_H
#define CITY_INFO_H

#include <limits>
#include <random>
#include <string>
#include <unordered_map>

#include "rapidhash.h"


struct stringly_hash
{
  using is_transparent = void;

private:
  inline static const uint64_t seed = [] {
    std::random_device rd;
    return (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
  }();

  [[nodiscard]] static size_t hash_sv(std::string_view sv) noexcept
  {
    return rapidhash_withSeed(sv.data(), sv.size(), seed);
  }

public:
  [[nodiscard]] size_t operator()(std::string_view rhs)   const noexcept { return hash_sv(rhs); }
  [[nodiscard]] size_t operator()(const char* rhs)        const noexcept { return hash_sv(rhs); }
  [[nodiscard]] size_t operator()(const std::string& rhs) const noexcept { return hash_sv(rhs); }
};

struct CityInfo {
  using map_t =
      std::unordered_map<std::string, CityInfo, stringly_hash, std::equal_to<>>;

  int min{std::numeric_limits<int>::max()};
  int max{std::numeric_limits<int>::min()};
  int count{0};
  int sum{0};

  static CityInfo parse(std::string_view line) {
    CityInfo res{};

    const auto pos = line.find(';');

    const std::string_view temp_str = line.substr(pos + 1);
    try {
      const float val = std::stof(temp_str.data());
      res.min = val;
      res.max = val;
      res.sum = val;
      res.count = 1;

      return res;

    } catch (std::exception &) {
      return {};
    }
  }

  static int parse_int(const char *__restrict__ s) {
    int mod = 1;
    if (*s == '-') {
      mod = -1;
      s++;
    }

    if (s[1] == '.') {
      int res = ((s[0] * 10) + s[2] - ('0' * 11)) * mod;
      return res;
    }
    int res = (s[0] * 100 + s[1] * 10 + s[3] - ('0' * 111)) * mod;
    return res;
  }

  static inline size_t parse_and_insert(const char *syms, const size_t beg, const size_t end,
                                        map_t &cities) {

    size_t pos = beg;
    while (pos < end && syms[pos] != ';') {
      pos++;
    }
    std::string_view name(&syms[beg], pos - beg);
    pos++;
    size_t num_beg = pos;
    pos += 3;
    while (pos < end && syms[pos] != '\n') {
      pos++;
    }

    const int val = parse_int(syms + num_beg);
    auto it = cities.find(name);
    if (it != cities.end()) {
      CityInfo &old_val = it->second;
      old_val.min = std::min(old_val.min, val);
      old_val.max = std::max(old_val.max, val);
      old_val.sum = old_val.sum + val;
      old_val.count = old_val.count + 1;
    } else {
      CityInfo res{};
      res.min = val;
      res.max = val;
      res.sum = val;
      res.count = 1;
      cities.emplace(name, res);
    }
    return pos + 1;
  }

  CityInfo &operator+=(CityInfo const &rhs) {
    min = std::min(min, rhs.min);
    max = std::max(max, rhs.max);
    sum += rhs.sum;
    count += rhs.count;

    return *this;
  }

  [[nodiscard]] float mean() const {
    return static_cast<float>(sum) / static_cast<float>(count);
  }
};

#endif
