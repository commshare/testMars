/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserve.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

// Paddle/paddle/memory/detail/memory_block.h
// Paddle/paddle/memory/detail/memory_block.cc

#pragma once

#include <cstddef>

namespace bubblefs {
namespace mypaddle {
namespace memory {
namespace detail {

// Forward Declarations
class MetadataCache;

/*! \brief A class used to interpret the contents of a memory block */
class MemoryBlock {
 public:
  enum Type {
    FREE_CHUNK,    // memory is free and idle
    ARENA_CHUNK,   // memory is being occupied
    HUGE_CHUNK,    // memory is out of management
    INVALID_CHUNK  // memory is invalid
  };

 public:
  void init(MetadataCache& cache, Type t, size_t index, size_t size,
            void* left_buddy, void* right_buddy);

 public:
  /*! \brief The type of the allocation */
  Type type(MetadataCache& cache) const;

  /*! \brief The size of the data region */
  size_t size(MetadataCache& cache) const;

  /*! \brief An index to track the allocator */
  size_t index(MetadataCache& cache) const;

  /*! \brief The total size of the block */
  size_t total_size(MetadataCache& cache) const;

  /*! \brief Check the left buddy of the block */
  bool has_left_buddy(MetadataCache& cache) const;

  /*! \brief Check the right buddy of the block */
  bool has_right_buddy(MetadataCache& cache) const;

  /*! \brief Get the left buddy */
  MemoryBlock* left_buddy(MetadataCache& cache) const;

  /*! \brief Get the right buddy */
  MemoryBlock* right_buddy(MetadataCache& cache) const;

 public:
  /*! \brief Split the allocation into left/right blocks */
  void split(MetadataCache& cache, size_t size);

  /*! \brief Merge left and right blocks together */
  void merge(MetadataCache& cache, MemoryBlock* right_buddy);

  /*! \brief Mark the allocation as free */
  void mark_as_free(MetadataCache& cache);

  /*! \brief Change the type of the allocation */
  void set_type(MetadataCache& cache, Type t);

 public:
  /*! \brief Get a pointer to the memory block's data */
  void* data() const;

  /*! \brief Get a pointer to the memory block's metadata */
  MemoryBlock* metadata() const;

 public:
  static size_t overhead();
};

void MemoryBlock::init(MetadataCache& cache, Type t, size_t index, size_t size,
                       void* left_buddy, void* right_buddy) {
  cache.store(this, Metadata(t, index, size - sizeof(Metadata), size,
                             static_cast<MemoryBlock*>(left_buddy),
                             static_cast<MemoryBlock*>(right_buddy)));
}

MemoryBlock::Type MemoryBlock::type(MetadataCache& cache) const {
  return cache.load(this).type;
}

size_t MemoryBlock::size(MetadataCache& cache) const {
  return cache.load(this).size;
}

size_t MemoryBlock::total_size(MetadataCache& cache) const {
  return cache.load(this).total_size;
}

MemoryBlock* MemoryBlock::left_buddy(MetadataCache& cache) const {
  return cache.load(this).left_buddy;
}

MemoryBlock* MemoryBlock::right_buddy(MetadataCache& cache) const {
  return cache.load(this).right_buddy;
}

void MemoryBlock::split(MetadataCache& cache, size_t size) {
  // make sure the split fits
  PADDLE_ASSERT(total_size(cache) >= size);

  // bail out if there is no room for another partition
  if (total_size(cache) - size <= sizeof(Metadata)) {
    return;
  }

  // find the position of the split
  void* right_partition = reinterpret_cast<uint8_t*>(this) + size;

  size_t remaining_size = total_size(cache) - size;

  // Add the new block as a buddy
  auto metadata = cache.load(this);

  // Write the metadata for the new block
  auto new_block_right_buddy = metadata.right_buddy;

  cache.store(
      static_cast<MemoryBlock*>(right_partition),
      Metadata(FREE_CHUNK, index(cache), remaining_size - sizeof(Metadata),
               remaining_size, this, new_block_right_buddy));

  metadata.right_buddy = static_cast<MemoryBlock*>(right_partition);
  metadata.size = size - sizeof(Metadata);
  metadata.total_size = size;

  cache.store(this, metadata);

  // Write metadata for the new block's right buddy
  if (new_block_right_buddy != nullptr) {
    auto buddy_metadata = cache.load(new_block_right_buddy);

    buddy_metadata.left_buddy = static_cast<MemoryBlock*>(right_partition);

    cache.store(new_block_right_buddy, buddy_metadata);
  }
}

void MemoryBlock::merge(MetadataCache& cache, MemoryBlock* right_buddy) {
  // only free blocks can be merged
  PADDLE_ASSERT(type(cache) == FREE_CHUNK);
  PADDLE_ASSERT(right_buddy->type(cache) == FREE_CHUNK);

  auto metadata = cache.load(this);

  // link this->buddy's buddy
  metadata.right_buddy = right_buddy->right_buddy(cache);

  // link buddy's buddy -> this
  if (metadata.right_buddy != nullptr) {
    auto buddy_metadata = cache.load(metadata.right_buddy);

    buddy_metadata.left_buddy = this;

    cache.store(metadata.right_buddy, buddy_metadata);
  }

  metadata.size += right_buddy->total_size(cache);
  metadata.total_size += right_buddy->total_size(cache);

  cache.store(this, metadata);
  cache.store(right_buddy, Metadata(INVALID_CHUNK, 0, 0, 0, nullptr, nullptr));
}

void MemoryBlock::mark_as_free(MetadataCache& cache) {
  // check for double free or corruption
  PADDLE_ASSERT(type(cache) != FREE_CHUNK);
  PADDLE_ASSERT(type(cache) != INVALID_CHUNK);

  set_type(cache, FREE_CHUNK);
}

void MemoryBlock::set_type(MetadataCache& cache, Type t) {
  auto metadata = cache.load(this);

  metadata.type = t;

  cache.store(this, metadata);
}

bool MemoryBlock::has_left_buddy(MetadataCache& cache) const {
  return left_buddy(cache) != nullptr;
}

bool MemoryBlock::has_right_buddy(MetadataCache& cache) const {
  return right_buddy(cache) != nullptr;
}

size_t MemoryBlock::index(MetadataCache& cache) const {
  return cache.load(this).index;
}

void* MemoryBlock::data() const {
  return const_cast<Metadata*>(reinterpret_cast<const Metadata*>(this)) + 1;
}

MemoryBlock* MemoryBlock::metadata() const {
  return const_cast<MemoryBlock*>(reinterpret_cast<const MemoryBlock*>(
      reinterpret_cast<const Metadata*>(this) - 1));
}

}  // namespace detail
}  // namespace memory
}  // namespace mypaddle
}  // namespace bubblefs