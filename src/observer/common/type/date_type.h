/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "common/type/data_type.h"

/**
 * @brief 日期类型
 * @ingroup DataType
 * @details DATE 在存储上占用 4 字节，内部用形如 YYYYMMDD 的整数表示
 *          （例如 2022-10-10 存为 20221010）。
 *
 * 之所以不用"距 1970-01-01 的天数"或 Unix 时间戳，是因为题目要求日期可以
 * 早于 1970-01-01、晚于 2038-02，而 32 位有符号秒级时间戳覆盖不了这个范围。
 * YYYYMMDD 编码在 year∈[1,9999] 内都不会溢出 int32，且**数值大小与日期的
 * 先后顺序一致**（20221231 < 20230101），因此可以直接用整数比较来实现日期
 * 比较，B+ 树索引的键比较也能自动复用。
 */
class DateType : public DataType
{
public:
  DateType() : DataType(AttrType::DATES) {}
  ~DateType() override = default;

  int compare(const Value &left, const Value &right) const override;
  int compare(const Column &left, const Column &right, int left_idx, int right_idx) const override;

  RC cast_to(const Value &val, AttrType type, Value &result) const override;

  RC set_value_from_str(Value &val, const string &data) const override;

  RC to_string(const Value &val, string &result) const override;

public:
  /// 是否为闰年
  static bool is_leap_year(int year);

  /// 指定年月的天数（已考虑闰年）
  static int days_in_month(int year, int month);

  /**
   * @brief 解析日期字符串
   * @details 接受 `YYYY-M-D` 形式，月、日可以是 1 位或 2 位（如 `2020-1-1`）。
   *          会校验年、月、日的取值范围以及闰年规则，非法输入返回 false。
   * @param str        待解析字符串
   * @param date_value 解析成功时输出 YYYYMMDD 形式的整数
   */
  static bool parse_date(const string &str, int &date_value);

  /// 将 YYYYMMDD 整数格式化为 `YYYY-MM-DD`
  static string format_date(int date_value);
};
