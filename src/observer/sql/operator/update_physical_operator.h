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

#include "common/value.h"
#include "sql/operator/physical_operator.h"
#include "storage/record/record.h"

class Table;
class FieldMeta;
class Trx;

/**
 * @brief 物理算子，更新记录
 * @ingroup PhysicalOperator
 * @details 执行方式是「逐行删除旧记录 + 插入新记录」。
 *          这样可以直接复用插入/删除路径上已有的索引维护逻辑，避免更新索引列时
 *          忘记同步索引键（原地覆盖还要自己处理旧键删除、新键插入以及页内布局变化）。
 */
class UpdatePhysicalOperator : public PhysicalOperator
{
public:
  UpdatePhysicalOperator(Table *table, const FieldMeta *field_meta, const Value &value)
      : table_(table), field_meta_(field_meta), value_(value)
  {}

  virtual ~UpdatePhysicalOperator() = default;

  PhysicalOperatorType type() const override { return PhysicalOperatorType::UPDATE; }

  OpType get_op_type() const override { return OpType::UPDATE; }

  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;

  Tuple *current_tuple() override { return nullptr; }

private:
  Table           *table_      = nullptr;
  const FieldMeta *field_meta_ = nullptr;
  Value            value_;      ///< 已经转换成字段类型的目标值
  Trx             *trx_        = nullptr;
  vector<Record>   records_;    ///< 命中的行，先收集再更新
};
