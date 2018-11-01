/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// caffe2/caffe2/distributed/store_handler.cc

#include "utils/caffe2_store_handler.h"

#include <memory>

namespace bubblefs {
namespace mycaffe2 {

constexpr std::chrono::milliseconds StoreHandler::kDefaultTimeout;
constexpr std::chrono::milliseconds StoreHandler::kNoTimeout;

StoreHandler::~StoreHandler() {
  // NOP; definition is here to make sure library contains
  // symbols for this abstract class.
}

//CAFFE_KNOWN_TYPE(std::unique_ptr<StoreHandler>);

} // namespace mycaffe2
} // namespace bubblefs