/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef X_SRC_EXPECT_EXPECT_CONDITION_H
#define X_SRC_EXPECT_EXPECT_CONDITION_H

#include <memory>
#include <string>

#include "plugin/x/ngs/include/ngs/error_code.h"


namespace xpl {

class Expect_condition {
 public:
  using Expect_condition_ptr = std::unique_ptr<Expect_condition>;

 public:
  explicit Expect_condition(const uint32_t k, const std::string &v)
  : m_key(k), m_value(v) {
  }

  Expect_condition(const Expect_condition &other)
  : m_key(other.m_key), m_value(other.m_value) {
  }

  virtual ~Expect_condition() {}

  virtual Expect_condition_ptr clone() = 0;
  virtual ngs::Error_code check_if_error() = 0;

  virtual uint32_t key() const { return m_key; }
  virtual const std::string &value() { return m_value; }

 private:
  const uint32_t m_key;
  const std::string m_value;
};

}  // namespace xpl

#endif  // X_SRC_EXPECT_EXPECT_CONDITION_H
