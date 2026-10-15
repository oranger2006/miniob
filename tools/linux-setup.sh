#!/usr/bin/env bash
# =============================================================================
# MiniOB 2025 初赛 - Linux 侧环境搭建与 basic 基线自测
#
# 用法（在 Docker / WSL2 / 虚拟机里执行，不要用 sudo）：
#     bash linux-setup.sh
#
# 分步执行（推荐，便于看中间结果）：
#     bash linux-setup.sh 1    # 拉代码（含换行符修复）
#     bash linux-setup.sh 2    # 编译依赖 + 编译 MiniOB
#     bash linux-setup.sh 3    # 跑 basic 基线自测
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# 可改配置
# ---------------------------------------------------------------------------
FORK_URL="https://github.com/oranger2006/miniob.git"   # 你的 fork
UPSTREAM_URL="https://github.com/oceanbase/miniob.git" # 官方，只用来拉 patch
BRANCH="competition-2025"
ROOT="${HOME}/miniob-work"
SRC="${ROOT}/miniob"
JOBS="$(nproc 2>/dev/null || echo 4)"

# Windows 上必须有这个用户名（WSL2 场景）；原生 Linux 上没有此用户也不影响
WIN_USER="11912"
WIN_MNT="/mnt/c/Users/${WIN_USER}"

say()  { printf '\n\033[1;36m=== %s ===\033[0m\n' "$*"; }
warn() { printf '\033[1;33m[警告] %s\033[0m\n' "$*"; }
die()  { printf '\033[1;31m[失败] %s\033[0m\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# 第 0 步：环境自检
# ---------------------------------------------------------------------------
step_check() {
  say "环境自检"
  echo "当前目录 : $(pwd)"
  echo "系统信息 : $(uname -a)"
  if [[ -r /etc/os-release ]]; then . /etc/os-release; echo "发行版   : ${PRETTY_NAME:-unknown}"; fi

  local ok=1
  # cmake >= 3.13
  if command -v cmake >/dev/null 2>&1; then
    local cv; cv="$(cmake --version | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)"
    echo "cmake    : $(cmake --version | head -1)"
    awk -v v="$cv" 'BEGIN{ if (v+0 < 3.13) exit 1 }' || { warn "cmake 低于 3.13"; ok=0; }
  else warn "缺少 cmake"; ok=0; fi

  # gcc >= 11
  if command -v g++ >/dev/null 2>&1; then
    local gv; gv="$(g++ -dumpversion)"
    echo "g++      : $gv"
    awk -v v="$gv" 'BEGIN{ if (v+0 < 11) exit 1 }' || { warn "g++ 低于 11，MiniOB 需要 C++20"; ok=0; }
  else warn "缺少 g++"; ok=0; fi

  # flex / bison
  command -v flex  >/dev/null 2>&1 && echo "flex     : $(flex --version 2>&1 | head -1)"  || { warn "缺少 flex（词法分析，必需）"; ok=0; }
  command -v bison >/dev/null 2>&1 && echo "bison    : $(bison --version 2>&1 | head -1)" || { warn "缺少 bison（语法分析，必需）"; ok=0; }

  # python3
  command -v python3 >/dev/null 2>&1 && echo "python3  : $(python3 --version 2>&1)" || { warn "缺少 python3（自测脚本需要）"; ok=0; }

  echo "make -j  : ${JOBS}"

  if [[ "$ok" -ne 1 ]]; then
    cat <<'EOF'

缺失的依赖用下面命令安装（Ubuntu / Debian）：

  sudo apt-get update
  sudo apt-get install -y cmake g++ flex bison python3 \
                          build-essential git libssl-dev

如果 apt 太慢，可换镜像源后再装。

EOF
    die "请先补齐上面列出的依赖，再重新运行本脚本。"
  fi
  say "环境自检通过"
}

# ---------------------------------------------------------------------------
# 第 1 步：拉代码
# ---------------------------------------------------------------------------
step_clone() {
  say "第 1 步：拉取代码"

  # 关键：换行符。Windows 上 autocrlf=true 会把 build.sh 检出成 CRLF，
  # 那个副本在 Linux 上跑会报 build.sh: line 2: $'\r': command not found。
  git config --global core.autocrlf false
  echo "core.autocrlf = $(git config --global --get core.autocrlf)"

  mkdir -p "$ROOT"
  cd "$ROOT"

  if [[ -d "$SRC/.git" ]]; then
    say "已存在 $SRC，跳过 clone（如需重来请先删除该目录）"
    git -C "$SRC" remote -v
  else
    # 网络受限时 git 的 openssl/curl 助手可能起不来，按需降级
    local cli=(clone --branch "$BRANCH" "$FORK_URL" "$SRC")
    if ! git "${cli[@]}"; then
      warn "普通 clone 失败，改用 http.sslBackend=openssl 重试"
      rm -rf "$SRC"
      git -c http.sslBackend=openssl "${cli[@]}"
    fi
  fi

  cd "$SRC"

  # upstream 只读参考，方便以后合官方 patch
  git remote get-url upstream >/dev/null 2>&1 \
    && git remote set-url upstream "$UPSTREAM_URL" \
    || git remote add upstream "$UPSTREAM_URL"
  git remote set-url origin "$FORK_URL"

  echo
  echo "--- remote ---"
  git remote -v
  echo "--- 当前分支 ---"
  git branch -vv
  echo "--- 换行符自检 ---"
  if grep -q $'\r' build.sh; then
    die "build.sh 仍是 CRLF。请执行 git config --global core.autocrlf false 后删除 $SRC 重新 clone。"
  fi
  echo "build.sh 是 LF，安全"
}

# ---------------------------------------------------------------------------
# 第 2 步：编译
# ---------------------------------------------------------------------------
step_build() {
  say "第 2 步：编译"
  [[ -d "$SRC/.git" ]] || die "请先执行 bash linux-setup.sh 1"
  cd "$SRC"

  if [[ ! -d deps/3rd/usr/local ]]; then
    say "编译第三方依赖（较慢，可能几分钟）"
    bash build.sh init
  else
    say "deps/3rd/usr/local 已存在，跳过 init"
  fi

  # 默认 build.sh 开 ASAN 且开 memtracer —— 官方 FAQ 第 3 条指出二者不能共存：
  # ASAN 会抢先进驻，导致 memtracer 统计不到内存。所以显式关掉 ASAN。
  say "编译 MiniOB（debug，关闭 ASAN 以启用 memtracer）"
  bash build.sh debug -DENABLE_ASAN=OFF -DWITH_MEMTRACER=ON --make "-j${JOBS}"

  say "编译产物"
  ls -lh build/bin/observer 2>/dev/null || die "没有找到 build/bin/observer"
  ls -lh build/lib/libmemtracer.so 2>/dev/null || warn "没有找到 libmemtracer.so（不影响功能开发）"
}

# ---------------------------------------------------------------------------
# 第 3 步：basic 基线自测
# ---------------------------------------------------------------------------
step_test() {
  say "第 3 步：basic 基线自测"
  [[ -x "$SRC/build/bin/observer" ]] || die "请先执行 bash linux-setup.sh 2"

  # 关键：miniob_test.py 每个用例结束后只 stop_server，不删数据目录，
  # 所以第二次起的运行会带着上一次的库跑，结果会漂移。每次先清干净。
  rm -rf /tmp/miniob/data

  cd "$SRC/test/case"
  echo "运行目录 : $(pwd)"
  echo "期望结果 : $SRC/test/case/result/basic.result"
  echo

  local rc=0
  python3 miniob_test.py --test-cases=basic || rc=$?

  echo
  if [[ "$rc" -eq 0 ]]; then
    say "basic 基线通过 ✅"
  else
    say "basic 未通过 ❌ (exit=$rc)"
    cat <<EOF

排查顺序：
  1) 数据目录残留：rm -rf /tmp/miniob/data 后重跑
  2) 看逐行差异：
       diff /tmp/miniob/result_output/basic.result.tmp \\
            $SRC/test/case/result/basic.result | head -50
  3) 确认没有误用 --report-only（那会覆盖期望结果文件 basic.result，把基线弄丢）

EOF
  fi
  exit "$rc"
}

# ---------------------------------------------------------------------------
main() {
  local step="${1:-all}"
  case "$step" in
    0|check) step_check ;;
    1|clone) step_clone ;;
    2|build) step_build ;;
    3|test)  step_test ;;
    all)    step_check; step_clone; step_build; step_test ;;
    *)      die "未知参数 '$step'，可用：0|1|2|3|all" ;;
  esac
}
main "$@"
