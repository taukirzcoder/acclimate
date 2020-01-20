/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
                          Christian Otto <christian.otto@pik-potsdam.de>

  This file is part of Acclimate.

  Acclimate is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ACCLIMATE_H
#define ACCLIMATE_H

#include "types.h"

namespace acclimate {

#undef assert
#ifdef DEBUG
#define error(a)                                                                        \
    {                                                                                   \
        std::ostringstream ss;                                                          \
        ss << id() << " error: " << a << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                           \
    }
#define error_(a)                                                              \
    {                                                                          \
        std::ostringstream ss;                                                 \
        ss << "error: " << a << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                  \
    }
#define assert_(expr)                                                                                  \
    if (!(expr)) {                                                                                     \
        std::ostringstream ss;                                                                         \
        ss << "assertion failed: " << __STRING(expr) << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                                          \
    }
#define assert(expr)                                                                                            \
    if (!(expr)) {                                                                                              \
        std::ostringstream ss;                                                                                  \
        ss << id() << " assertion failed: " << __STRING(expr) << " (" << __FILE__ << ", l." << __LINE__ << ")"; \
        throw acclimate::exception(ss.str());                                                                   \
    }
#ifndef TEST
#define assertstep(a)                                                   \
    if (model()->run()->step() != IterationStep::a) {                   \
        std::ostringstream ss;                                          \
        ss << id() << " error: should be in " << __STRING(a) << " step" \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";            \
        throw acclimate::exception(ss.str());                           \
    }
#define assertstepor(a, b)                                                                          \
    if (model()->run()->step() != IterationStep::a && model()->run()->step() != IterationStep::b) { \
        std::ostringstream ss;                                                                      \
        ss << id() << " error: should be in " << __STRING(a) << " or " << __STRING(b) << " step"    \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";                                        \
        throw acclimate::exception(ss.str());                                                       \
    }
#define assertstepnot(a)                                                    \
    if (model()->run()->step() == IterationStep::a) {                       \
        std::ostringstream ss;                                              \
        ss << id() << " error: should NOT be in " << __STRING(a) << " step" \
           << " (" << __FILE__ << ", l." << __LINE__ << ")";                \
        throw acclimate::exception(ss.str());                               \
    }
#else
#define assertstep(a) \
    {}
#define assertstepor(a, b) \
    {}
#define assertstepnot(a) \
    {}
#endif
#define warning(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": Warning: " << a << std::endl; }
#define info(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": " << a << std::endl; }
#define debug(a) \
    _Pragma("omp critical (output)") { std::cout << model()->run()->timeinfo() << ", " << id() << ": " << __STRING(a) << " = " << a << std::endl; }
#define warning_(a) \
    _Pragma("omp critical (output)") { std::cout << "Warning: " << a << std::endl; }
#define info_(a) \
    _Pragma("omp critical (output)") { std::cout << a << std::endl; }

#else  // !def DEBUG

#define error(a)                              \
    {                                         \
        std::ostringstream ss;                \
        ss << a;                              \
        throw acclimate::exception(ss.str()); \
    }
#define error_(a)                             \
    {                                         \
        std::ostringstream ss;                \
        ss << a;                              \
        throw acclimate::exception(ss.str()); \
    }
#define assert_(a) \
    {}
#define assert(a) \
    {}
#define assertstep(a) \
    {}
#define assertstepor(a, b) \
    {}
#define assertstepnot(a) \
    {}
#define warning(a) \
    {}
#define info(a) \
    {}
#define debug(a) \
    {}
#define warning_(a) \
    {}
#define info_(a) \
    {}

#endif

}  // namespace acclimate

#endif
