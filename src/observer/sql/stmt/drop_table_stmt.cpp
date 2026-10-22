/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/stmt/drop_table_stmt.h"

#include "common/log/log.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

RC DropTableStmt::create(Db *db, const DropTableSqlNode &drop_table, Stmt *&stmt)
{
  stmt = nullptr;

  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  const string &table_name = drop_table.relation_name;
  if (table_name.empty()) {
    LOG_WARN("invalid argument. table name is empty");
    return RC::INVALID_ARGUMENT;
  }

  // 元数据校验：表不存在时直接失败，满足 drop 不存在的表要返回 FAILURE 的要求
  if (nullptr == db->find_table(table_name.c_str())) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  stmt = new DropTableStmt(table_name);
  return RC::SUCCESS;
}
