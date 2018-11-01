// Copyright (c) 2016-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

// rocksdb/include/rocksdb/utilities/object_registry.h

#ifndef BUBBLEFS_OBJECT_REGISTRY_H_
#define BUBBLEFS_OBJECT_REGISTRY_H_

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

namespace bubblefs {
namespace base {
  
// Creates a new T using the factory function that was registered with a pattern
// that matches the provided "target" string according to std::regex_match.
//
// If no registered functions match, returns nullptr. If multiple functions
// match, the factory function used is unspecified.
//
// Populates res_guard with result pointer if caller is granted ownership.
template <typename T>
T* NewCustomObject(const std::string& target, std::unique_ptr<T>* res_guard);

// Returns a new T when called with a string. Populates the unique_ptr argument
// if granting ownership to caller.
template <typename T>
using FactoryFunc = std::function<T*(const std::string&, std::unique_ptr<T>*)>;

// To register a factory function for a type T, initialize a Registrar<T> object
// with static storage duration. For example:
//
//   static Registrar<Env> hdfs_reg("hdfs://.*", &CreateHdfsEnv);
//
// Then, calling NewCustomObject<Env>("hdfs://some_path", ...) will match the
// regex provided above, so it returns the result of invoking CreateHdfsEnv.
template <typename T>
class Registrar {
 public:
  explicit Registrar(std::string pattern, FactoryFunc<T> factory);
};

// Implementation details follow.

namespace internal {

template <typename T>
struct RegistryEntry {
  std::regex pattern;
  FactoryFunc<T> factory;
};

template <typename T>
struct Registry {
  static Registry* Get() {
    static Registry<T> instance;
    return &instance;
  }
  std::vector<RegistryEntry<T>> entries;

 private:
  Registry() = default;
};

}  // namespace internal

template <typename T>
T* NewCustomObject(const std::string& target, std::unique_ptr<T>* res_guard) {
  res_guard->reset();
  for (const auto& entry : internal::Registry<T>::Get()->entries) {
    if (std::regex_match(target, entry.pattern)) {
      return entry.factory(target, res_guard);
    }
  }
  return nullptr;
}

template <typename T>
Registrar<T>::Registrar(std::string pattern, FactoryFunc<T> factory) {
  internal::Registry<T>::Get()->entries.emplace_back(internal::RegistryEntry<T>{
      std::regex(std::move(pattern)), std::move(factory)});
}

}  // namespace base
}  // namespace bubblefs

#endif  // BUBBLEFS_OBJECT_REGISTRY_H_