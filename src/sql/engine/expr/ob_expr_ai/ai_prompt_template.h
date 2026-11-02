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
#include <limits>

namespace oceanbase
{
namespace common
{
// The emitter receives either a literal slice (index == -1) or an argument index.
// Scan only the template: braces inside an argument are never interpreted again.
template <typename Emit>
bool visit_ai_prompt_template(const char *text, int64_t length, uint64_t argument_count, Emit emit)
{
  for (int64_t i = 0; i < length;) {
    if ((text[i] == '{' || text[i] == '}') && i + 1 < length && text[i + 1] == text[i]) {
      if (!emit(text + i, 1, -1)) { return false; }
      i += 2;
    } else if (text[i] == '{') {
      ++i;
      uint64_t index = 0;
      const int64_t start = i;
      while (i < length && text[i] >= '0' && text[i] <= '9') {
        const uint64_t digit = text[i++] - '0';
        if (index > (std::numeric_limits<int64_t>::max() - digit) / 10) { return false; }
        index = index * 10 + digit;
      }
      if (i == start || i >= length || text[i] != '}' || index >= argument_count) { return false; }
      if (!emit(nullptr, 0, static_cast<int64_t>(index))) { return false; }
      ++i;
    } else if (text[i] == '}') {
      return false;
    } else {
      const int64_t start = i;
      while (i < length && text[i] != '{' && text[i] != '}') { ++i; }
      if (!emit(text + start, i - start, -1)) { return false; }
    }
  }
  return true;
}
} // namespace common
} // namespace oceanbase
