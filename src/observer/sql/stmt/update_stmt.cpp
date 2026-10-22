/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/update_stmt.h"

#include "common/lang/unordered_map.h"
#include "common/log/log.h"
#include "common/value.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/db/db.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"

UpdateStmt::UpdateStmt(Table *table, const FieldMeta *field_meta, const Value &value, FilterStmt *filter_stmt)
    : table_(table), field_meta_(field_meta), value_(value), filter_stmt_(filter_stmt)
{}

UpdateStmt::~UpdateStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC UpdateStmt::create(Db *db, const UpdateSqlNode &update, Stmt *&stmt)
{
  stmt = nullptr;

  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  const string &table_name = update.relation_name;
  if (table_name.empty()) {
    LOG_WARN("invalid argument. table name is empty");
    return RC::INVALID_ARGUMENT;
  }

  // 元数据校验：表必须存在
  Table *table = db->find_table(table_name.c_str());
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // 元数据校验：被更新的字段必须存在
  const string    &field_name = update.attribute_name;
  const FieldMeta *field_meta = table->table_meta().field(field_name.c_str());
  if (nullptr == field_meta) {
    LOG_WARN("no such field. table=%s, field=%s", table_name.c_str(), field_name.c_str());
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  // 把用户给的值转换成字段类型。转换不了（比如用字符串更新整型字段）就报错，
  // 上层会把这条 UPDATE 输出成 FAILURE。
  Value value;
  if (field_meta->type() == update.value.attr_type()) {
    value = update.value;
  } else if (OB_FAIL(Value::cast_to(update.value, field_meta->type(), value))) {
    LOG_WARN("failed to cast value for update. field=%s, from=%s, to=%s",
        field_meta->name(),
        attr_type_to_string(update.value.attr_type()),
        attr_type_to_string(field_meta->type()));
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  // 构建 WHERE 条件。UPDATE 不带 WHERE 时 conditions 为空，filter_stmt 为空，等价于全表更新
  unordered_map<string, Table *> table_map;
  table_map.insert(pair<string, Table *>(table_name, table));

  FilterStmt *filter_stmt = nullptr;
  RC          rc          = FilterStmt::create(
      db, table, &table_map, update.conditions.data(), static_cast<int>(update.conditions.size()), filter_stmt);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to create filter statement. rc=%s", strrc(rc));
    return rc;
  }

  stmt = new UpdateStmt(table, field_meta, value, filter_stmt);
  return RC::SUCCESS;
}
