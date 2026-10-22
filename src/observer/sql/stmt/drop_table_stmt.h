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

#include "sql/stmt/stmt.h"

class Db;

/**
 * @brief 删除表语句
 * @ingroup Statement
 * @details DROP TABLE 的语法解析在 yacc 中已经能生成 SCF_DROP_TABLE，
 *          这里补齐语句层，把表名解析出来并校验表是否存在。
 */
class DropTableStmt : public Stmt
{
public:
  DropTableStmt() = default;
  explicit DropTableStmt(const string &table_name) : table_name_(table_name) {}
  ~DropTableStmt() override = default;

  StmtType type() const override { return StmtType::DROP_TABLE; }

public:
  static RC create(Db *db, const DropTableSqlNode &drop_table, Stmt *&stmt);

  const string &table_name() const { return table_name_; }

private:
  string table_name_;
};
