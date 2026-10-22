/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sql/operator/update_physical_operator.h"

#include "common/log/log.h"
#include "storage/field/field_meta.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"

namespace {

/**
 * @brief 把一个 Value 写到记录的某个字段上
 * @details 先把字段区域清零，再拷贝值。清零是必要的：CHARS 字段的宽度通常大于实际字符串
 *          长度（比如 char(20) 存 'abc'），如果直接用 value.data() 拷贝 field->len()
 *          个字节，会读越界并把脏数据写进记录。插入路径（Table::set_value_to_record）
 *          也是同样的语义：只拷贝有效长度，其余保持 0。
 */
RC write_value_to_field(Record &record, const FieldMeta *field, const Value &value)
{
  RC rc = record.reset_filed(field->offset(), field->len());
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to reset field. field=%s, rc=%s", field->name(), strrc(rc));
    return rc;
  }

  int copy_len = value.length();
  if (copy_len > field->len()) {
    copy_len = field->len();  // 超长时截断，与插入语义保持一致
  }
  if (copy_len <= 0) {
    return RC::SUCCESS;
  }

  rc = record.set_field(field->offset(), copy_len, value.data());
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to set field. field=%s, rc=%s", field->name(), strrc(rc));
  }
  return rc;
}

}  // namespace

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  unique_ptr<PhysicalOperator> &child = children_[0];

  RC rc = child->open(trx);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to open child operator. rc=%s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  // 先把命中的记录复制出来。更新过程会删除旧记录，不能在遍历扫描结果的同时改动这些记录。
  // 这里用 copy_data 做深拷贝，保证扫描结束后记录内容依然有效（不受页面换出影响）。
  records_.clear();
  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current tuple");
      child->close();
      return RC::INTERNAL;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record   &record    = row_tuple->record();

    Record record_copy;
    rc = record_copy.copy_data(record.data(), record.len());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to copy record. rc=%s", strrc(rc));
      child->close();
      return rc;
    }
    record_copy.set_rid(record.rid());
    records_.emplace_back(std::move(record_copy));
  }
  child->close();

  if (records_.empty()) {
    // WHERE 没有命中任何行时什么都不做，但仍然返回成功
    return RC::SUCCESS;
  }

  const int updated_count = static_cast<int>(records_.size());
  LOG_INFO("begin to update records. table=%s, field=%s, count=%d",
      table_->name(), field_meta_->name(), updated_count);

  // 逐行更新：构造新记录 -> 删除旧记录 -> 插入新记录
  for (Record &record : records_) {
    Record new_record;
    rc = new_record.copy_data(record.data(), record.len());
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to copy record for update. rc=%s", strrc(rc));
      return rc;
    }
    new_record.set_rid(record.rid());

    rc = write_value_to_field(new_record, field_meta_, value_);
    if (OB_FAIL(rc)) {
      return rc;
    }

    // 删除旧记录（同时维护该表上所有索引的旧键）
    rc = trx_->delete_record(table_, record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to delete old record during update. rc=%s", strrc(rc));
      return rc;
    }

    // 插入新记录（同时插入索引新键）
    rc = trx_->insert_record(table_, new_record);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to insert new record during update. rc=%s", strrc(rc));

      // 插入失败要把旧记录恢复回去，否则这一行数据就丢了
      RC recover_rc = trx_->insert_record(table_, record);
      if (OB_FAIL(recover_rc)) {
        LOG_ERROR("failed to recover old record after update failure. table=%s, rc=%s",
            table_->name(), strrc(recover_rc));
      }
      return rc;
    }
  }

  LOG_INFO("update records success. table=%s, count=%d", table_->name(), updated_count);
  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next() { return RC::RECORD_EOF; }

RC UpdatePhysicalOperator::close() { return RC::SUCCESS; }
