/*
 * Copyright (c) 2025 OceanBase.
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

#pragma once

#include <cstdint>

namespace oceanbase
{
namespace common
{
// A batch response must contain each request index exactly once. Storage is owned
// by the caller's allocator; values map request indices back to response positions.
class AiResponseIndexMap
{
public:
  AiResponseIndexMap(int64_t count, int64_t *positions)
      : count_(count), positions_(positions)
  {
    if (positions_ != nullptr) {
      for (int64_t i = 0; i < count_; ++i) { positions_[i] = -1; }
    }
  }

  bool record(int64_t response_position, int64_t request_index)
  {
    if (positions_ == nullptr || response_position < 0 || response_position >= count_
        || request_index < 0 || request_index >= count_ || positions_[request_index] != -1) {
      return false;
    }
    positions_[request_index] = response_position;
    return true;
  }

  int64_t response_position(int64_t request_index) const
  {
    return positions_ != nullptr && request_index >= 0 && request_index < count_
        ? positions_[request_index] : -1;
  }

  bool complete() const
  {
    if (count_ <= 0 || positions_ == nullptr) { return false; }
    for (int64_t i = 0; i < count_; ++i) {
      if (positions_[i] == -1) { return false; }
    }
    return true;
  }

private:
  int64_t count_;
  int64_t *positions_;
};
} // namespace common
} // namespace oceanbase
