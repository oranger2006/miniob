/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/type/date_type.h"

#include <cctype>
#include <cstdint>
#include <cstdio>

#include "common/log/log.h"
#include "common/value.h"
#include "storage/common/column.h"

bool DateType::is_leap_year(int year)
{
  // 能被 4 整除但不能被 100 整除，或者能被 400 整除
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DateType::days_in_month(int year, int month)
{
  static const int days_of_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days_of_month[month - 1];
}

bool DateType::parse_date(const string &str, int &date_value)
{
  // 先去掉首尾空白字符
  size_t begin = 0;
  size_t end   = str.size();
  while (begin < end && isspace(static_cast<unsigned char>(str[begin]))) {
    begin++;
  }
  while (end > begin && isspace(static_cast<unsigned char>(str[end - 1]))) {
    end--;
  }
  if (begin == end) {
    return false;
  }

  // 按 '-' 切分成 年、月、日 三段，每段必须至少有一位数字
  int  parts[3]   = {0, 0, 0};
  int  part_index = 0;
  bool has_digit  = false;

  for (size_t i = begin; i < end; i++) {
    const char c = str[i];
    if (c == '-') {
      if (!has_digit) {
        return false;  // 出现 '-' 开头或连续 '-'
      }
      part_index++;
      if (part_index > 2) {
        return false;  // 超过三段
      }
      has_digit = false;
    } else if (c >= '0' && c <= '9') {
      parts[part_index] = parts[part_index] * 10 + (c - '0');
      has_digit         = true;
      if (parts[part_index] > 9999) {
        return false;  // 防止整数溢出，同时排除明显非法的年份
      }
    } else {
      return false;  // 出现非法字符
    }
  }

  if (part_index != 2 || !has_digit) {
    return false;
  }

  const int year  = parts[0];
  const int month = parts[1];
  const int day   = parts[2];

  if (year < 1 || year > 9999) {
    return false;
  }
  if (month < 1 || month > 12) {
    return false;
  }
  if (day < 1 || day > days_in_month(year, month)) {
    return false;
  }

  date_value = year * 10000 + month * 100 + day;
  return true;
}

string DateType::format_date(int date_value)
{
  const int year  = date_value / 10000;
  const int month = (date_value / 100) % 100;
  const int day   = date_value % 100;

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
  return string(buffer);
}

int DateType::compare(const Value &left, const Value &right) const
{
  if (left.attr_type() != AttrType::DATES) {
    LOG_WARN("left value is not date. type=%s", attr_type_to_string(left.attr_type()));
    return INT32_MAX;
  }

  const int left_value = left.value_.int_value_;

  int right_value = 0;
  if (right.attr_type() == AttrType::DATES) {
    right_value = right.value_.int_value_;
  } else if (right.attr_type() == AttrType::CHARS) {
    // 兜底处理：正常情况下比较前已经由 CharType::cast_to 把字符串转成了日期，
    // 这里再解析一次，避免某些路径上字符串直接参与比较
    if (!parse_date(right.get_string(), right_value)) {
      LOG_WARN("failed to parse date string. value=%s", right.get_string().c_str());
      return INT32_MAX;
    }
  } else {
    LOG_WARN("unsupported compare between date and %s", attr_type_to_string(right.attr_type()));
    return INT32_MAX;
  }

  if (left_value < right_value) {
    return -1;
  }
  if (left_value > right_value) {
    return 1;
  }
  return 0;
}

int DateType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  const int32_t left_value  = reinterpret_cast<const int32_t *>(left.data())[left_idx];
  const int32_t right_value = reinterpret_cast<const int32_t *>(right.data())[right_idx];

  if (left_value < right_value) {
    return -1;
  }
  if (left_value > right_value) {
    return 1;
  }
  return 0;
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::CHARS: {
      const string str = format_date(val.value_.int_value_);
      result.set_string(str.c_str());
      return RC::SUCCESS;
    }
    default: {
      LOG_WARN("unsupported cast date to %s", attr_type_to_string(type));
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int date_value = 0;
  if (!parse_date(data, date_value)) {
    LOG_WARN("invalid date string. data=%s", data.c_str());
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
  val.set_date(date_value);
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  result = format_date(val.value_.int_value_);
  return RC::SUCCESS;
}
