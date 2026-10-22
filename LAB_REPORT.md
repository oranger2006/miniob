# MiniOB 2025 初赛实验报告

> 本文档记录本次 MiniOB 数据库内核实验的**实现思路、方法与涉及的知识点**，按题目逐项展开，并随开发进度持续更新。
>
> **更新约定**
> - 每完成一道题：更新该题「状态」与「实现记录」，并在文末「更新日志」追加一行。
> - 每次提测：在「提测记录」表格追加一行，写明分数变化。
> - 文中的「现状」字段均来自**对本仓库代码的实际核查**，不是猜测；标注「待验证」的条目尚未核实。

**状态图例**：`✅ 已完成` · `🔧 进行中` · `⬜ 未开始` · `❓ 待验证`

---

## 一、实验环境

| 项目 | 内容 |
|---|---|
| 基础分支 | `competition-2025`（commit `1a65035`） |
| 代码仓库 | `https://github.com/oranger2006/miniob-2025`（**私有仓库**，平台要求） |
| 上游参考 | `https://github.com/oceanbase/miniob`（remote 名 `upstream`，仅用于同步官方 patch） |
| 开发环境 | VMware 虚拟机 Ubuntu，`g++ 11`、`cmake`、`flex`、`bison` |
| 评测环境 | 训练营平台，Ubuntu 22.04 / gcc 11.4 / x86_64 |
| 内存限制 | 评测侧通过 `LD_PRELOAD=libmemtracer.so` 限制 **350MB** |

### 编译与自测命令

```bash
# 首次：编译第三方依赖（libevent/jsoncpp/replxx/cppjieba 等）
bash build.sh init

# 编译 MiniOB（关闭 ASAN 以启用 memtracer，二者不能共存）
bash build.sh debug -DENABLE_ASAN=OFF -DWITH_MEMTRACER=ON --make -j4

# 自测：先清数据目录，否则跨用例数据残留会导致结果漂移
rm -rf /tmp/miniob/data
cd test/case && python3 miniob_test.py --test-cases=basic
```

**注意事项**

- `build.sh` 会创建软链接 `build -> build_debug`，而 `miniob_test.py` 查找的是 `<work-dir>/build/bin/observer`（默认 `/tmp/miniob/build`）。两者通过软链接对齐后，自测才不会重复全量编译。
- 自测**禁止**加 `--report-only`：该参数会用本次输出**覆盖** `test/case/result/basic.result`，等于亲手毁掉基线答案。
- 评测机内存受限，实现时**避免把整表数据读入内存**，尤其多表查询不要裸笛卡尔积、聚合不要全量 materialize。

---

## 二、当前评测成绩

### 2.1 提测记录

| 时间 | 仓库 | 状态 | 成绩 | 备注 |
|---|---|---|---|---|
| 20:27 | `miniob`(公开) | 执行失败 | 0 | `public repo not allowed` |
| 20:38 | `miniob`(公开) | 执行失败 | 0 | 同上 |
| 20:39 | `miniob-2025`(私有) | 执行失败 | 0 | `Repository not found`（token 无效/权限不足） |
| 20:47 | `miniob-2025` | 已取消 | — | 主动取消 |
| **20:49** | `miniob-2025` | **执行成功** | **10.000** | 成功 1 / 失败 23 —— **基线** |

### 2.2 用例明细（基线 10.000）

> 总分池 **460** 分，24 个用例。`-` 表示缺失（应有未有），`+` 表示多余（不应有而有）。

| 题目 | 满分 | 得分 | 失败表现 | 根因归类 |
|---|---|---|---|---|
| **basic** | 10 | **10** ✅ | — | 通过 |
| alias | 20 | 0 | 输出行/列不匹配 | 缺表别名 + 相关子查询 |
| alter | 20 | 0 | `SQL_SYNTAX` | 语法缺失 |
| big-order-by | 30 | 0 | 结果未排序 | 缺 ORDER BY |
| complex-sub-query | 30 | 0 | 缺行 | 缺嵌套子查询 |
| create-view | 30 | 0 | `SQL_SYNTAX` | 语法缺失 |
| date | 10 | 0 | `SQL_SYNTAX` | 缺 DATE 类型 |
| drop-table | 10 | 0 | `FAILURE` | **语法已通，执行层缺失** |
| expression | 10 | 0 | 缺行 | 谓词不支持算术表达式 |
| full-text-index | 30 | 0 | `SQL_SYNTAX` | 缺 TEXT 类型 |
| function | 10 | 0 | `SQL_SYNTAX`（`u_date date`） | 缺 DATE 类型 |
| group-by | 20 | 0 | `SQL_SYNTAX`（`birthday date`） | 缺 DATE 类型 |
| join-tables | 10 | 0 | 输出仅 2 列，期望 4 列 | 缺 JOIN 语法 |
| multi-index | 20 | 0 | `SQL_SYNTAX`（`col4 date`） | 缺 DATE 类型 |
| null | 20 | 0 | `SQL_SYNTAX`（`birthday date null`） | 缺 DATE + NULL |
| order-by | 20 | 0 | 结果未排序 | 缺 ORDER BY |
| simple-sub-query | 20 | 0 | 缺行 | 缺 IN 子查询 |
| text | 20 | 0 | `SQL_SYNTAX` | 缺 TEXT 类型 |
| union | 20 | 0 | 部分行缺失/多余 | 缺 UNION |
| unique | 20 | 0 | `SQL_SYNTAX` | 缺 UNIQUE INDEX |
| update | 10 | 0 | `FAILURE` | **语法已通，执行层缺失** |
| update-mvcc | 30 | 0 | `SQL_SYNTAX` | 缺 UNIQUE + UPDATE |
| vector-basic | 20 | 0 | `SQL_SYNTAX`（`VECTOR(3)`） | 向量类型是桩 |
| vector-search | 20 | 0 | `SQL_SYNTAX` | 同上 |

**结论**：基线 10 分**不是操作失误**，而是官方原始代码在本届新题库（含向量、全文索引、视图）上的真实水平。文档中「官方 62.8% 通过率」指旧题集，与本届 460 分题库不可比。

---

## 三、代码现状核查

这一节是**动手前的关键依据**。逐项核查词法、语法、语句层、执行层四个环节，避免盲目开工。

| 能力 | lexer token | yacc 产生式 | Stmt 实现 | 物理算子 |
|---|---|---|---|---|
| CREATE/DROP INDEX | ✅ | ✅ | ✅ | `index_scan` ✅ |
| GROUP BY | ✅ `GROUP` | ✅ | ✅ | `group_by`/`hash_group_by`/`scalar_group_by` ✅ |
| 聚合函数 | ❌ 无 COUNT/SUM/AVG/MIN/MAX 关键字 | ❓ | — | `aggregate_vec` ✅ |
| **DROP TABLE** | ✅ `DROP` | ✅ 生成 `SCF_DROP_TABLE` | ❌ **无 DropTableStmt，`Stmt::create_stmt` 无 case** | — |
| **UPDATE** | ✅ `UPDATE` | ⚠️ 仅支持**单字段**赋值 | ❌ **`UpdateStmt::create` 是空壳（`// TODO` + `return RC::INTERNAL`）** | ❌ 无 |
| **JOIN / INNER JOIN** | ❌ | ❌ `rel_list` 仅支持逗号分隔 | — | ✅ **`join`/`nested_loop_join`/`hash_join` 已存在** |
| ORDER BY | ❌ | ❌ | ❌ | ❌ |
| UNION | ❌ | ❌ | ❌ | ❌ |
| CREATE VIEW | ❌ | ❌ | ❌ | ❌ |
| ALTER TABLE | ❌ | ❌ | ❌ | ❌ |
| UNIQUE INDEX | ❌ | ❌ | ❌ | ❌ |
| **DATE** | ✅ `DATE` | ✅ `type` 产生式 | ✅ `DateType` 已实现 | ✅ 通用比较路径 |
| TEXT | ❌ | ❌ | `AttrType` 无 `TEXTS` | ❌ |
| NULL | ❌ | ❌ | ❌ | ❌ |
| **VECTOR** | ✅ `VECTOR` | ✅ 仅 `VECTOR`（**不支持 `VECTOR(N)`**） | ⚠️ `VectorType` 是**桩**：`compare` 恒返回 `INT32_MAX`，`to_string` 返回 `UNIMPLEMENTED` | ❌ |

### 三个重要推论

1. **`DROP TABLE` 和 `UPDATE` 的语法已经通了**。评测报的是 `FAILURE` 而不是 `SQL_SYNTAX`，正是因为语法解析成功、但 `Stmt::create_stmt` 里没有对应 `case`，落到默认分支返回失败。这两道题只需补「语句层 + 执行层」，是所有题里**投入产出比最高**的。
2. **`JOIN` 是「只缺语法」**。物理执行（嵌套循环 / 哈希连接）已经实现，`join-tables`（10 分）主要工作量在 yacc 产生式和计划构建，不涉及执行算法。
3. **`VECTOR` 是「只缺实现」**。类型枚举、`VectorType` 类骨架、yacc 类型映射都已就位，但类里全是桩。缺口集中在：`VECTOR(N)` 带长度语法、`STRING_TO_VECTOR`/`VECTOR_TO_STRING`/`DISTANCE` 函数、`Value` 的向量分支、序列化与输出。

---

## 四、依赖分析与实现顺序

### 4.1 关键发现：DATE 是「题眼」

评测输出显示，多个用例在**第一条 `CREATE TABLE` 就中断**，原因都是字段含 `date`：

| 用例 | 中断语句 |
|---|---|
| date | `CREATE TABLE date_table(id int, u_date date)` |
| group-by | `CREATE TABLE aggregation_func(..., birthday date)` |
| multi-index | `CREATE TABLE multi_index(..., col4 date, ...)` |
| function | `CREATE TABLE function_table(..., u_date date)` |
| null | `CREATE TABLE null_table(..., birthday date null)` |

**这意味着 DATE 一个类型改动，可能连带解锁 80 分**（date 10 + group-by 20 + multi-index 20 + function 10 + null 20）。

> **注意**：这不代表这些题「只缺 DATE」。评测在 `CREATE TABLE` 阶段就失败了，后续是否还有别的缺口（如 NULL 关键字、聚合函数）**无法从现有输出判断**。DATE 只是**当前可见的第一道墙**，必须先拆。

同理，**TEXT 类型**是 `text`(20) 和 `full-text-index`(30) 的第一道墙，合计 50 分。

### 4.2 推荐实现顺序

**第一梯队 · 拆墙（类型系统，为后续铺路）**

| 顺序 | 题目 | 分值 | 理由 |
|---|---|---|---|
| 1 | DATE | 10 + 连带 80 | 5 个用例的第一道墙；类型系统改动范围清晰 |
| 2 | TEXT | 20 + 连带 30 | 2 个用例的第一道墙；定长 4096 简化实现 |

**第二梯队 · 快速得分（语法已通，只差执行层）**

| 顺序 | 题目 | 分值 | 理由 |
|---|---|---|---|
| 3 | DROP TABLE | 10 | 语法已生成 AST，只缺 Stmt + 资源释放 |
| 4 | UPDATE | 10 | 语法单字段已通，缺 Stmt + 物理算子 |
| 5 | JOIN | 10 | 物理算子已存在，只缺语法与计划 |

**第三梯队 · 排序与聚合**

| 顺序 | 题目 | 分值 | 理由 |
|---|---|---|---|
| 6 | ORDER BY | 20 + 连带 30 | `big-order-by` 直接依赖；无任何脚手架，需新建算子 |
| 7 | 聚合函数 | （group-by 20 的前置） | lexer 缺 COUNT/SUM/AVG/MIN/MAX |

**第四梯队 · 子查询与高级语句**

| 顺序 | 题目 | 分值 |
|---|---|---|
| 8 | expression（谓词算术） | 10 |
| 9 | simple-sub-query | 20 |
| 10 | complex-sub-query | 30 |
| 11 | alias（表别名 + 相关子查询） | 20 |
| 12 | UNION | 20 |
| 13 | UNIQUE INDEX | 20（`update-mvcc` 30 的前置） |
| 14 | ALTER TABLE | 20 |
| 15 | CREATE VIEW | 30 |
| 16 | NULL | 20 |

**第五梯队 · 本届创新题（分值最高、最重）**

| 顺序 | 题目 | 分值 |
|---|---|---|
| 17 | vector-basic（类型 + 三个函数） | 20 |
| 18 | vector-search（距离检索 / 索引） | 20 |
| 19 | full-text-index（分词 + 倒排 + BM25） | 30 |
| 20 | update-mvcc（UPDATE 的 MVCC 语义） | 30 |

---

## 五、逐题实现方案

### 5.1 basic ✅

**要求**：建表、插入、删除、查询、建索引、表达式计算等基础功能。

**状态**：✅ 已通过（10/10）。基线版本未改任何代码即通过。
**方法**：本仓库 `test/case/result/basic.result` 即为期望输出，改动任何功能后**必须重跑**作为回归判据。

---

### 5.2 DATE 🔧

**要求**：实现 `date` 字段类型，支持建表、插入、比较、索引、输出。
- 取值范围**不是** Unix 时间戳能覆盖的：测试可能**早于 1970-01-01**、也**晚于 2038-02**（所以不能用 32 位秒级时间戳）
- 非法日期（如 `2021-2-31`、`2017-2-29` 非闰年、`2017-11-31` 11 月无 31 日）必须返回 `FAILURE`
- 需处理闰年；`where d='2017-2-29'` 返回 `FAILURE`（注意与 MySQL 返回空集不同，此处以题目约定为准）
- 输出格式 `YYYY-mm-DD`（补零）

**初始现状**：`AttrType` 只有 `UNDEFINED/CHARS/INTS/FLOATS/VECTORS/BOOLEANS`，无 `DATES`；lexer/yacc 无 `DATE` 关键字；无 `DateType` 类。

#### 实现记录（已完成）

**核心设计：内部用 `int32` 表示 `YYYYMMDD`**

日期在存储上占 **4 字节**，值为 `year*10000 + month*100 + day`（例：`2022-10-10` → `20221010`）。

选这个编码而不是「距 1970-01-01 的天数」或 Unix 时间戳，理由有三条：
1. **覆盖范围足够**：`year ∈ [1, 9999]` 都不会超出 `int32`，天然满足「早于 1970、晚于 2038」的要求；
2. **数值大小与日期先后一致**（`20221231 < 20230101`），所以**日期比较可以直接退化成整数比较**，`DateType::compare` 和 B+ 树索引键比较都不用额外处理；
3. **代码量最小**：`Value` 的 union 里已有 `int32_t`，不需要扩展存储。

**改动清单**

| 文件 | 改动 |
|---|---|
| `common/type/attr_type.h` | 枚举在 `FLOATS` 后插入 `DATES` |
| `common/type/attr_type.cpp` | 名称表同步插入 `"dates"`（下标必须与枚举严格对齐） |
| `common/type/date_type.h/.cpp` | **新建** `DateType`：`compare`（含 `Column` 版本）、`cast_to`（→CHARS）、`set_value_from_str`、`to_string`，以及 `parse_date` / `format_date` / `is_leap_year` / `days_in_month` 静态工具 |
| `common/type/data_type.cpp` | 类型工厂按新枚举顺序注册 `make_unique<DateType>()` |
| `common/value.h/.cpp` | `friend class DateType`；新增 `set_date()`；`set_data`/`set_value`/`get_int` 补 `DATES` 分支 |
| `common/type/char_type.cpp` | `cast_to` 支持 `CHARS→DATES`（解析+校验，失败返回错误）；`cast_cost(DATES)=1` |
| `sql/parser/lex_sql.l` | 新增关键字 `DATE`（lexer 已 `case-insensitive`，`date`/`DATE` 都识别） |
| `sql/parser/yacc_sql.y` | `%token DATE`；`type` 产生式新增 `| DATE` |
| `sql/expr/expression.cpp` | 向量化按列比较时把 `DATES` 并入整数分支 |
| `storage/common/codec.h` | `encode_value` 补 `DATES`（LSM 引擎键编码，与整数一致） |

**关键机制说明**

- **字符串 → 日期**这一跳是整个题目的枢纽。实测链路是：
  `INSERT` 的字符串字面量先成为 `CHARS` 类型 `Value`，由 `Table::make_record` 里的 `Value::cast_to(value, field->type(), ...)` 触发 `CharType::cast_to(DATES)`；解析或校验失败即返回错误，插入随之 `FAILURE`。
- **`where d = '2020-1-1'` 能工作的原因**：`LogicalPlanGenerator` 在构建谓词时用 `cast_cost` 决定往哪边转。把 `CharType::cast_cost(DATES)` 从 `INT32_MAX` 改成 `1` 后，右侧字符串会被转成 `DATES` 再比较；**转换失败会让整条语句失败**，正好满足「非法日期查询返回 FAILURE」的约定。
- **索引无需额外改动**：`bplus_tree.h` 的 `AttrComparator` 走的是 `DataType::type_instance(type)->compare(...)`，属于通用路径，`DateType::compare` 一接上即生效（`BPlusTree` 只有 `CHARS` 有范围扫描特判，`DATES` 不受影响）。
- **元数据持久化安全**：`FieldMeta` 用字符串存类型（`attr_type_to_string`/`attr_type_from_string`），所以 `"dates"` 能正确往返，不受枚举下标变化影响。

**已覆盖的合法性校验**

| 输入 | 结果 | 判定依据 |
|---|---|---|
| `2020-1-01`、`2020-01-1`、`1970-1-1` | 合法 | 月/日允许 1~2 位 |
| `2016-2-29` | 合法 | 2016 是闰年 |
| `2017-2-29` | `FAILURE` | 2017 非闰年，2 月只有 28 天 |
| `2017-21-29` | `FAILURE` | 月份 > 12 |
| `2017-12-32` | `FAILURE` | 12 月只有 31 天 |
| `2017-11-31` | `FAILURE` | 11 月只有 30 天 |
| 空串 / `2020-1` / `2020-1-1-1` / 含非数字字符 | `FAILURE` | 段数或字符非法 |

**知识点**
- 数据类型扩展的**完整链路**：词法 → 语法 → 类型工厂 → `Value` → 记录存储 → 输出格式化
- 定长字段的存储与对齐；用整数编码表达有序语义（`YYYYMMDD` 的单调性）
- 日期合法性校验（闰年规则、每月天数）
- 隐式类型转换的**方向选择**（`cast_cost` 决定把字符串转成日期，而不是反向）
- 索引键比较复用类型的 `compare` 实现

**验证**：⏳ 待编译验证。本地 `test/case/test/primary-date.test`；平台 `date` 用例

---

### 5.3 TEXT ⬜

**要求**：实现超长字段 `text`，**固定长度 4096 字节**，可当字符串处理；输入超过 4096 字节时**截断保存**；需调整 `record_manager`（当前按定长管理页面）。

**现状**：`AttrType` 无 `TEXTS`，lexer/yacc 无 `TEXT` 关键字。

**实现思路**

1. `attr_type.h` 加 `TEXTS`，`attr_type.cpp` 加映射。
2. 新建 `common/type/text_type.h/.cpp`（可**直接复用 `CharType` 逻辑**，仅长度上限不同）。
3. lexer 加 `TEXT RETURN_TOKEN(TEXT)`，yacc type 产生式加 `| TEXT`。
4. **长度处理**：建表时把 `TEXT` 字段长度固定为 4096；插入时若超长，**截断到 4096**（注意是字节还是字符，需按题目与 MySQL 行为确认）。
5. **记录长度**：确认 `TableMeta` 与 `Record` 的长度计算包含 4096 字节字段，避免超页；若单记录超页需确认现有实现是否支持。
6. **输出**：按字符串输出，不加引号。

**涉及文件**：`common/type/attr_type.*`、`common/type/text_type.*`（新建）、`common/type/data_type.cpp`、`sql/parser/lex_sql.l`、`sql/parser/yacc_sql.y`、`storage/record/record_manager.*`

**知识点**
- 定长记录管理与页内布局
- 字符串截断语义
- 大字段对单页容量的影响

**验证**：本地 `primary-text.test`；平台 `text`、`full-text-index` 用例

---

### 5.4 DROP TABLE ⬜

**要求**：删除表，并**清除所有关联资源**（数据页、索引等）。删除后再 `select` 该表应返回 `FAILURE`（元数据校验）。

**现状**：语法**已通**——`yacc_sql.y` 的 `drop_table_stmt` 产生式已生成 `ParsedSqlNode(SCF_DROP_TABLE)`；但**没有 `DropTableStmt` 类**，`Stmt::create_stmt` 也无 `SCF_DROP_TABLE` 分支，因此落到默认分支返回 `FAILURE`。

**实现思路**

1. 新建 `sql/stmt/drop_table_stmt.h/.cpp`：`create()` 校验表存在 → 组装 `DropTableStmt`。
2. `stmt.cpp` 的 `Stmt::create_stmt` 增加 `case SCF_DROP_TABLE`。
3. 执行层：在 `Db` 层实现删表
   - 从 catalog 移除表元数据
   - 释放数据页 / 删除表数据文件
   - **遍历并删除该表上的所有索引**（B+ 树文件也要删）
4. 元数据校验：`select/insert/update/delete` 前确认表存在，删除后访问返回 `FAILURE`。

**涉及文件**：`sql/stmt/drop_table_stmt.*`（新建）、`sql/stmt/stmt.cpp`、`storage/db/db.*`、`storage/table/table.*`、`storage/index/*`

**知识点**
- Catalog（系统表）管理
- 资源生命周期与回收（页、文件、索引句柄）
- 元数据校验与错误返回规范

**验证**：本地 `primary-drop-table.test`；平台 `drop-table` 用例

---

### 5.5 UPDATE ⬜

**要求**：实现单字段更新，支持带 where 和不带 where；更新需同步索引；元数据校验（表/字段不存在 → `FAILURE`）；where 查不到数据 → 仍返回 `SUCCESS`；类型不可转换 → `FAILURE`。

**现状**：
- 语法：`UPDATE ID SET ID EQ value where` —— **仅单字段**，多字段赋值需扩展
- `UpdateStmt::create` 是**空壳**：

  ```cpp
  RC UpdateStmt::create(Db *db, const UpdateSqlNode &update, Stmt *&stmt)
  {
    // TODO
    stmt = nullptr;
    return RC::INTERNAL;
  }
  ```
- `Stmt::create_stmt` **无 `SCF_UPDATE` 分支**
- 无 `UpdatePhysicalOperator`

**实现思路**

1. **Stmt**：实现 `UpdateStmt::create` —— 打开表、校验字段存在、构造待更新 `Value`（含类型转换与校验）、保存 where 条件。
2. **接进 `create_stmt`**：加 `case SCF_UPDATE`。
3. **逻辑计划**：`UpdateLogicalOperator`（表 + 字段 + 值 + 谓词）。
4. **物理计划**：新建 `update_physical_operator.h/.cpp`，**仿 `delete_physical_operator`**：
   - 子算子产出待更新行（`TableScan` + `Predicate`）
   - 对每行：先构造新记录 → **删除旧记录 + 插入新记录**（比原地覆盖更安全，能正确处理变长字段与索引）
   - **同步更新索引**：删旧键、插新键；若更新的是索引列，务必用旧值删、新值插
5. **事务/MVCC**：在事务上下文中执行，失败要能回滚（`update-mvcc` 会专门考察）。
6. **空结果集**：无匹配行时不报错，返回 `SUCCESS`（符合输出规范）。

**涉及文件**：`sql/stmt/update_stmt.cpp`、`sql/stmt/stmt.cpp`、`sql/operator/update_logical_operator.*`（新建）、`sql/operator/update_physical_operator.*`（新建）、`sql/executor/*`（执行入口）

**知识点**
- 逻辑计划 → 物理计划的转换
- 谓词下推与行迭代模型（`PhysicalOperator::next/current_tuple`）
- 索引维护一致性（删旧键/插新键）
- 更新的原子性与回滚

**验证**：本地 `primary-update.test`；平台 `update` 用例

---

### 5.6 JOIN ⬜

**要求**：`INNER JOIN`，支持多表、多条 `on` 条件。

**现状**：**lexer 与 yacc 中完全没有 JOIN / INNER 关键字**；`rel_list` 仅支持逗号分隔的笛卡尔积，**不支持表别名**。但 `join_physical_operator`、`nested_loop_join_physical_operator`、`hash_join_physical_operator` **已存在**。

**实现思路**

1. lexer 加 `INNER`、`JOIN`、`ON` 关键字；yacc 增加 `rel_list INNER JOIN relation ON condition_list` 产生式。
2. `parse_defs.h` 的 `SelectSqlNode` 增加 join 描述结构（表、on 条件列表）。
3. `SelectStmt::create` 构建 `JoinLogicalOperator`（已存在）。
4. 计划选择：内连接可用 `NestedLoopJoin`；带等值条件时用 `HashJoin` 更优，也能避免大数据量下的内存问题。
5. 注意 `select *` 的输出列名规则（多表需带表名，用 `.` 分隔），与输出规范对齐。

**涉及文件**：`sql/parser/lex_sql.l`、`sql/parser/yacc_sql.y`、`sql/parser/parse_defs.h`、`sql/stmt/select_stmt.cpp`、`sql/operator/join_logical_operator.*`

**知识点**
- 连接语义与语法树表示
- 嵌套循环连接 vs 哈希连接的选择
- 内存约束下的连接执行（禁止裸笛卡尔积）

**验证**：本地 `primary-join-tables.test`、`dblab-hash-join.test`；平台 `join-tables` 用例

---

### 5.7 ORDER BY ⬜

**要求**：支持 `order by`，默认升序；支持多字段、`asc`/`desc`；不要求 `order by 1` 这种数字形式。

**现状**：lexer/yacc **无 ORDER / BY 关键字**（`SELECT` 语法里没有 order by 子句），**无任何排序算子**。

**实现思路**

1. lexer 加 `ORDER`、`BY`、`ASC`、`DESC`；yacc 在 `SELECT` 产生式后加 `order_by` 子句，收集 `(字段, 方向)` 列表。
2. `SelectSqlNode` 增加 `order_by` 字段；`SelectStmt` 传递下去。
3. 新建 `OrderByPhysicalOperator`：
   - 从子算子拉取全部行（受 350MB 限制，**不能无脑全 materialize**，需考虑分批/外部排序）
   - 用多字段比较器排序（按 `asc/desc` 逐字段比较）
   - 逐行 `next()` 输出
4. 执行顺序：`ORDER BY` 应在**投影之后**、最终输出之前。

**涉及文件**：`sql/parser/lex_sql.l`、`sql/parser/yacc_sql.y`、`sql/parser/parse_defs.h`、`sql/stmt/select_stmt.cpp`、`sql/operator/order_by_physical_operator.*`（新建）

**知识点**
- 排序算子与多列比较器
- 内存限制下的排序策略（外部排序 / 分块）
- SQL 子句的执行顺序（`where` → `group by` → `having` → `order by` → `limit`）

**验证**：本地 `primary-order-by.test`、`dblab-sort.test`；平台 `order-by`、`big-order-by` 用例

---

### 5.8 聚合函数与 GROUP BY ⬜

**要求**：`max/min/count/avg`；`group by` 分组聚合。聚合字段不会与普通字段混用（如 `select id, count(age)` 不考）。NULL 值不计入统计。

**现状**：`GROUP BY` 语法**已有**（`group_by` 产生式、`GROUP` token）；`group_by_physical_operator`、`hash_group_by_physical_operator`、`scalar_group_by_physical_operator`、`aggregate_vec_physical_operator` **已存在**。但 lexer **无 COUNT/SUM/AVG/MIN/MAX 关键字**。

**实现思路**

1. 确认聚合函数在语法中是走「函数调用」还是需要新增关键字（**待验证**：可能通过 `ID '(' ... ')'` 形式支持）。
2. 若需关键字：lexer 加 `COUNT/SUM/AVG/MIN/MAX`，yacc 增加聚合函数表达式产生式。
3. `Expression` 增加聚合表达式类型（`AggregateExpr`），`SelectStmt` 识别聚合列并构建 `GroupByLogicalOperator`。
4. 无 `group by` 但有聚合时，走 `ScalarGroupBy`（输出单行）。
5. **NULL 语义**：`count(col)` 跳过 NULL，`count(*)` 计所有行；`avg` 分母不含 NULL。
6. **整数 avg 返回浮点**：注意输出格式（保留两位、去多余 0）。

**涉及文件**：`sql/parser/lex_sql.l`、`sql/parser/yacc_sql.y`、`sql/expr/expression.*`、`sql/stmt/select_stmt.cpp`

**知识点**
- 聚合表达式与分组语义
- 聚合算子的两种形态（标量聚合 / 分组聚合）
- NULL 在聚合中的处理规则
- 整数与浮点运算的类型提升

**验证**：本地 `primary-aggregation-func.test`、`primary-group-by.test`；平台 `group-by`、`function` 用例

---

### 5.9 expression（谓词算术）⬜

**要求**：`where` 条件中支持 `+ - * /` 运算表达式；类型混合比较（如 `t.id > 1.1`）；异常按 NULL 规则处理。

**现状**：评测 `select * from exp_table where 7+col2 < 13;` 返回空（缺行），说明谓词不支持算术表达式。

**实现思路**

1. 确认 `ConditionSqlNode` 的左右操作数目前只支持「字段 / 值」，需改为支持 `Expression`。
2. 复用现有表达式体系（`ArithExpr`、`ComparisonExpr`）在 `SelectStmt::create` 中构建条件表达式。
3. 类型推导与隐式转换：整数 ↔ 浮点比较需提升到浮点。
4. 除零等异常按 NULL 规则处理。

**知识点**：表达式树构建与求值、类型提升、三值逻辑

**验证**：本地 `primary-expression.test`；平台 `expression` 用例

---

### 5.10 子查询（simple / complex / alias）⬜

**要求**
- `simple-sub-query`：`IN` / `NOT IN`、与子查询结果比较、子查询含聚合；子查询不与主查询关联
- `complex-sub-query`：嵌套多表 + `AND` 条件 + 聚合；**考虑关联子查询**（子查询引用外层表）
- `alias`：表别名 + 相关子查询（`select * from t1 a where id in (select ... from t2 b where b.col >= a.col)`）

**现状**：lexer/yacc 无子查询语法；`rel_list` 不支持表别名。

**实现思路**

1. `parse_defs.h` 增加子查询节点；yacc 在 `where_condition` 中允许子查询作为右操作数。
2. 表别名：`rel_list` 产生式支持 `relation ID`，`RelAttrSqlNode` 记录别名，字段解析时用别名映射到真实表。
3. 执行策略：
   - **非关联**：先独立执行子查询得到结果集，再与主查询条件做 `IN` / 比较
   - **关联**：需要对每行外层数据重算子查询（可加简单缓存），注意性能
4. `IN` 语义与 NULL 的交互：`x IN (含 NULL 的集合)` 需按三值逻辑处理。

**知识点**：子查询语义、相关/非相关子查询的执行差异、别名解析、三值逻辑

**验证**：本地 `primary-simple-sub-query.test`、`primary-complex-sub-query.test`；平台 `simple-sub-query`、`complex-sub-query`、`alias` 用例

---

### 5.11 UNION ⬜

**要求**：`select ... union select ...`，结果去重（`UNION` 默认去重）。

**现状**：lexer/yacc 无 `UNION`。

**实现思路**：lexer 加 `UNION`；yacc 支持 `select_stmt UNION select_stmt`；计划层用「两路输入 + 去重」算子（可用哈希去重，注意内存上限）。列数/类型需校验一致。

**知识点**：集合运算语义、去重策略与内存权衡

**验证**：平台 `union` 用例

---

### 5.12 UNIQUE INDEX ⬜

**要求**：`create unique index`；插入/更新时违反唯一性需失败。

**现状**：lexer/yacc 无 `UNIQUE`。

**实现思路**

1. lexer 加 `UNIQUE`；yacc 的 `create_index_stmt` 增加 `UNIQUE` 可选修饰符，`CreateIndexSqlNode` 增加 `unique` 标志。
2. 索引元数据 `IndexMeta`（或 `TableMeta` 下的索引定义）增加唯一性标记并持久化。
3. 写入路径（insert / update）在插索引键前**先查重**，存在则返回失败并回滚本次插入。
4. 建索引时若表内已有重复值，建索引本身应失败。

**知识点**：唯一约束语义、索引元数据持久化、写入路径的前置校验与回滚

**验证**：本地 `primary-unique.test`；平台 `unique`、`update-mvcc` 用例

---

### 5.13 ALTER TABLE ⬜

**要求**：`alter table t add column col int;`（至少支持加列）。

**现状**：lexer/yacc 无 `ALTER`。

**实现思路**

1. lexer 加 `ALTER`；yacc 增加 `ALTER TABLE ID ADD COLUMN attr_info` 产生式。
2. 元数据：`TableMeta` 增加字段并持久化（注意字段顺序与既有记录的兼容）。
3. 存量数据：加列后旧记录缺少该列，读取时按默认值（NULL / 0）填充，**避免全表重写**。
4. 输出：`select *` 需包含新列。

**知识点**：表结构演进、元数据持久化、旧数据兼容（schema evolution）

**验证**：平台 `alter` 用例

---

### 5.14 CREATE VIEW ⬜

**要求**：见 `docs/docs/game/create-view-2025.md`。核心是**可更新视图规则**：单表完整列可增删改；部分列视图仅能操作所含列；含表达式列不可写该列；含聚合的视图整体只读；多表视图需按基表拆分。

**现状**：lexer/yacc 无 `CREATE VIEW`。

**实现思路**

1. lexer 加 `VIEW`；yacc 增加 `CREATE VIEW ID AS select_stmt`，支持可选列名列表。
2. 视图元数据：保存视图名、列别名、底层 `SelectStmt` 的**文本或结构化定义**（推荐存 SQL 文本，查询时重新解析，简化实现）。
3. 查询视图：把视图名解析为子查询，走既有的 select 执行路径。
4. **可更新性判定**（本题核心）：
   - 解析视图定义，统计涉及的基表数量
   - 标记每个输出列是「原始列」还是「表达式列 / 聚合列」
   - `INSERT`/`UPDATE`/`DELETE` 时按规则校验：只允许映射回单一基表的具体行
5. 嵌套视图：递归解析，权限继承源视图（题目说明暂不考嵌套更新，但查询要支持）。

**知识点**：视图重写、可更新视图的判定规则、元数据存储

**验证**：平台 `create-view` 用例

---

### 5.15 NULL ⬜

**要求**：`nullable` / `null` 关键字；NULL 与**任何值**比较均为 FALSE；`is null` / `is not null`；可用 NULL 字段建索引；聚合跳过 NULL。

**现状**：lexer/yacc 无 `NULL`。

**实现思路**

1. 每个字段增加「可为空」标志（`FieldMeta`），默认不可空。
2. `Value` 增加 NULL 状态（或在记录中用空位图/特殊标记表示）。
3. 比较逻辑：任一侧为 NULL → 结果为 FALSE（**注意 `=` 与 `is` 的区别**：`null = null` 为 FALSE，`null is null` 为 TRUE）。
4. 索引：NULL 值是否入索引需明确约定（可参考 MariaDB 行为）。
5. 聚合：`count(col)` 跳过 NULL，`avg` 分母排除 NULL。
6. 非空字段插入 NULL → `FAILURE`。

**知识点**：三值逻辑（TRUE/FALSE/UNKNOWN）、NULL 在索引与聚合中的语义、可空性元数据

**验证**：本地 `primary-null.test`；平台 `null` 用例

---

### 5.16 vector-basic ⬜

**要求**：`VECTOR(N)` 类型（默认 2048，最大 16383，每元素 4 字节 float）；**不得写 `VECTOR()`**；支持 `STRING_TO_VECTOR`（别名 `TO_VECTOR`）、`VECTOR_TO_STRING`（别名 `FROM_VECTOR`）、`DISTANCE`（别名 `VECTOR_DISTANCE`，度量 `COSINE`/`DOT`/`EUCLIDEAN`）；VECTOR 仅支持相等比较，不支持其它比较。

**现状**：`AttrType::VECTORS` 已存在；lexer 有 `VECTOR` token；yacc type 产生式支持 `VECTOR`（**不带长度**）；`VectorType` 是**桩**（`compare` 恒 `INT32_MAX`、`to_string` 返回 `UNIMPLEMENTED`）。

**实现思路**

1. **语法**：yacc 支持 `VECTOR '(' NUMBER ')'`，把长度写入 `AttrInfoSqlNode`；无括号时默认 2048；`VECTOR()` 需报语法错误。
2. **类型实现**：`VectorType` 补齐
   - `to_string`：`[1,2,3]` 格式
   - `compare`：仅相等比较
   - 长度校验：插入/建表时元素个数必须等于声明长度
3. **存储**：`N * 4` 字节定长；`Value` 增加向量分支（`std::vector<float>` 或裸缓冲）。
4. **标量函数**：`STRING_TO_VECTOR`（解析 `[...]`，支持科学计数法、容忍空白）、`VECTOR_TO_STRING`、`DISTANCE`（三种度量）。需在语法中支持函数调用形式，并在执行期接入函数求值。
5. **输出**：`VECTOR_TO_STRING` 结果最大 262128 字节，注意缓冲。

**知识点**：向量类型的存储布局、浮点精度与输出格式、距离度量（L2 / 余弦 / 内积）的数值稳定性、函数扩展机制

**验证**：平台 `vector-basic` 用例；官方 `ann-benchmarks` fork 可做性能对照

---

### 5.17 vector-search ⬜

**要求**：向量检索（最近邻）。题目不限制算法，可自研或借助第三方库。

**现状**：无向量索引，无距离算子。

**实现思路**

1. **基线（先跑通）**：全表扫描 + 逐行算距离 + 排序取 top-k。简单但 O(n·d)，大数据量下受 350MB 内存限制需分批。
2. **进阶（提性能）**：IVF-Flat（聚类分桶）或 HNSW 图索引；也可引入 SIMD 加速距离计算。
3. 索引落盘：需要与现有 B+ 树索引框架并存或独立管理。

**知识点**：ANN 近似最近邻、IVF/HNSW 原理、向量距离计算的 SIMD 优化、内存与召回率的权衡

**验证**：平台 `vector-search` 用例

---

### 5.18 full-text-index ⬜

**要求**：全文索引（题面未展开，参考 2025 赛题说明：分词器、倒排索引、BM25 打分，以及混合检索）。

**现状**：无 `TEXT` 类型（前置）、无分词器、无倒排索引。仓库 `deps/3rd/cppjieba` 已内置 **cppjieba** 分词库。

**实现思路**

1. **前置**：先完成 TEXT 类型。
2. **分词**：接入 `cppjieba`（`JiebaTokenizer`），英文按空格/标点切分。
3. **倒排索引**：`term -> posting list(记录id 列表, 词频)`；可持久化为独立文件，或用 B+ 树承载 `term -> posting`。
4. **打分**：BM25 需要 `df`（文档频率）、`tf`、文档长度、平均文档长度。
5. **查询**：`MATCH ... AGAINST` 或题目指定语法，返回按相关性排序的结果。
6. **混合检索**：全文过滤 + 向量重排（稀疏 + 稠密融合）。

**知识点**：分词原理、倒排索引结构、TF-IDF / BM25、相关性排序、混合检索（粗排 + 精排）

**验证**：平台 `full-text-index` 用例

---

### 5.19 update-mvcc ⬜

**要求**：在 MVCC/事务语义下实现 UPDATE（前置：UNIQUE INDEX + UPDATE）。`create unique index` 后更新不冲突，且并发/回滚场景正确。

**现状**：`CONCURRENCY` 编译选项默认 `OFF`；UPDATE 与 UNIQUE 均未实现。

**实现思路**

1. 先完成 UNIQUE + UPDATE 的基础功能。
2. 更新流程中保证：旧版本可见性、新版本写入、索引键的旧删新增在同一事务内原子完成。
3. 回滚：失败时恢复旧记录与旧索引键（题目提示「唯一索引 + 更新失败回滚不完全」是常见失分点）。
4. 注意 `unique_table4` 类用例：插入/更新失败后表内容不能被污染。

**知识点**：MVCC 多版本可见性、事务原子性与回滚、唯一约束与并发冲突

**验证**：平台 `update-mvcc` 用例

---

## 六、数据库内核知识点索引

按模块汇总本实验涉及的知识点，便于复习与自查。

### 6.1 SQL 解析
- 词法分析（flex）：关键字 token 化、字符串/数字字面量
- 语法分析（bison/yacc）：产生式、语义动作、AST 节点构造
- `ParsedSqlNode` → `Stmt` 的语义绑定（元数据校验、字段/表解析）

### 6.2 类型系统
- `AttrType` 枚举 + `DataType` 基类 + 类型工厂的注册机制
- 新增类型的完整链路：lexer → yacc → 工厂 → `Value` → 存储 → 输出
- 类型转换与提升（整数 ↔ 浮点、字符串 → 日期）
- 数值输出规范（浮点两位小数、去多余 0；日期补零）

### 6.3 存储引擎
- 页式存储、缓冲池（`disk_buffer_pool`）、LRU 淘汰
- 定长记录管理（`record_manager`）、记录在页内的布局
- B+ 树索引：插入/删除/查找、索引与记录的映射
- 大字段对单页容量的影响

### 6.4 查询执行
- 逻辑计划 → 物理计划
- 火山模型（`PhysicalOperator::open/next/current_tuple/close`）
- 算子：表扫描、索引扫描、谓词、投影、连接（NLJ / Hash）、聚合、排序
- 谓词下推与执行顺序

### 6.5 事务与并发
- 原子性：更新失败的回滚
- MVCC：多版本可见性
- 唯一约束与并发冲突

### 6.6 向量检索
- 稠密向量表示、距离度量（L2 / 余弦 / 内积）
- ANN：IVF-Flat、HNSW
- SIMD 优化

### 6.7 全文检索
- 中文分词（cppjieba）
- 倒排索引、posting list
- BM25 打分
- 混合检索（稀疏 + 稠密）

### 6.8 工程约束
- 350MB 内存上限 → 避免全量 materialize、分批处理、流式输出
- 输出规范（`SUCCESS`/`FAILURE`、列名规则、分隔符）
- 元数据校验与错误处理

---

## 七、开发与提测流程

### 7.1 开发循环

```bash
# 1) 改代码后编译
cd ~/miniob-work/miniob
bash build.sh debug -DENABLE_ASAN=OFF -DWITH_MEMTRACER=ON --make -j4

# 2) 本地回归（先清数据目录）
rm -rf /tmp/miniob/data
cd test/case && python3 miniob_test.py --test-cases=basic

# 3) 提交
cd ~/miniob-work/miniob
git add -A && git commit -m "feat: 说明做了什么"
git push origin competition-2025
git rev-parse HEAD        # 拿到新 commit id
```

### 7.2 提测表单

| 字段 | 值 |
|---|---|
| 代码仓库地址 | `https://oranger2006:<token>@github.com/oranger2006/miniob-2025` |
| Commit id | 上一步 `git rev-parse HEAD` 的结果 |
| Branch | `competition-2025` |

> **仓库必须是私有仓库**，公开仓库会被拒（`public repo not allowed`）。地址需带 `用户名:token@`。
> **Token 安全**：只放本地，**不要提交进仓库、不要贴到聊天或截图**，泄露后立即到 https://github.com/settings/tokens 作废并重发。

### 7.3 纪律
- 每次改动后**必须**跑 `basic` 回归，防止踩坏基础功能
- 禁止 `--report-only`（会覆盖期望结果）
- 评测等待可能超过 1 小时，属正常排队

---

## 八、更新日志

| 日期 | 内容 |
|---|---|
| 2026-10-15 | 建立基线：提测 10.000（basic 通过，23 项失败）；完成代码现状核查与依赖分析；新增 `tools/linux-setup.sh` 自测脚本；撰写本报告初版 |
| 2026-10-22 | **实现 DATE 类型**：`AttrType::DATES` + `DateType`（`YYYYMMDD` 整数编码）+ `Value` 分支 + lexer/yacc 关键字 + `CharType` 向日期隐式转换；索引与输出走通用路径。⏳ 待编译验证 |
