# AI 函数增强

代码基线：`digod111/seekdb` 的 `2026-competition` 分支，提交
`5bce9e06d1281347ad61344e4b01fab9158c58a2`。

## 实现范围

- `AI_PROMPT`：读取 TEXT/LONGTEXT，保留 JSON 参数，展开嵌套 Prompt；重复占位符按实际输出追加，不再按参数长度预估固定缓冲区。
- 模板使用从 0 开始的 `{index}`；`{{`、`}}` 输出字面花括号。越界、负数、非数字及溢出下标报错。参数里的花括号不再次解析。嵌套深度限制为 64。
- `AI_COMPLETE`：支持 SQL NULL 配置、原生 JSON 配置以及 JSON 文本配置；验证 temperature/max_tokens；要求响应是非空字符串，返回原始文本而非 JSON 引号包裹的文本。
- Ollama：Endpoint Provider 不区分大小写，允许空 access_key；原生生成协议强制 stream=false，标准 temperature/max_tokens 映射到 options.temperature/options.num_predict；原生 Embedding 响应执行向量检查。
- `AI_EMBED`：正整数维度校验、数值与有限性校验、返回数量和维度检查；OpenAI 响应按 index 恢复请求顺序并拒绝重复或非法索引。
- 跨行 Embedding：表达式 batch 回调每次最多合并 64 条输入，按 model/dimension 分组，跳过 skip 和已计算行；SQL NULL 不发请求，按原始行号写回。失败向上传播。
- `AI_RERANK`：字符串和对象输入共用 20 条一批的请求路径，每批显式请求 top_n 等于输入条数；校验返回数量、索引唯一性和有限数值分数，转换为全局 index 后按分数降序、原始 index 升序排序；对象模式从原始文档映射结果，保留 ID。

- HTTP 层：检查响应 JSON 根节点类型，复制请求体后再交给 libcurl，避免临时缓冲区生命周期影响传输；重试前清理上一次响应，重试耗尽保留失败，初始化失败释放已有资源，reset 在清理完毕后才丢弃 allocator。

## 已执行的验证

- 独立 CMake/CTest：3 个目标均通过，其中包括 7 个原生 GoogleTest 测试、9 个 Python Mock HTTP 协议测试，以及 SQL 用例选择和诊断输出的 9 个测试。
- 原生算法使用 AddressSanitizer 和 UndefinedBehaviorSanitizer：Prompt 覆盖 19 项场景与 4096 个短模板扫描；生产代码共用的响应索引映射覆盖 1～8 条输入的全部 46233 个排列，以及重复、缺项、越界索引。
- 独立 libcurl 实验：128000 字节的请求体（包含 NUL）在原始缓冲区销毁后传输长度及 SHA-256 一致。这验证 libcurl 选项的传输行为，不等同于运行 seekdb 的 HTTP 客户端。
- `git diff --check` 通过。

独立测试入口（需要 CMake、C++17 编译器、Python 和 GoogleTest 源码；本次使用 CMake 3.31.6）：

当前云环境的 CMake 位于 `/workspace/.venvs/seekdb-ai-tests/bin`，GoogleTest 源码位于
`/tmp/seekdb-googletest`；可先激活前者，再将下列源码路径参数替换为后者。

```bash
source /workspace/.venvs/seekdb-ai-tests/bin/activate
cmake -S unittest/sql/engine/expr -B /tmp/seekdb-ai-core-build \
  -DGTEST_SOURCE_DIR=/absolute/path/to/googletest -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/seekdb-ai-core-build -j2
ctest --test-dir /tmp/seekdb-ai-core-build --output-on-failure
```

只编译 Prompt 测试也可以：

```bash
# 在仓库根目录执行，GTEST_SOURCE_DIR 指向 GoogleTest 源码根目录。
g++ -std=c++17 -Wall -Wextra -Werror -pthread \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -Isrc -I"$GTEST_SOURCE_DIR/googletest/include" -I"$GTEST_SOURCE_DIR/googletest" \
  unittest/sql/engine/expr/ob_expr_ai_prompt_test.cpp \
  "$GTEST_SOURCE_DIR/googletest/src/gtest-all.cc" \
  "$GTEST_SOURCE_DIR/googletest/src/gtest_main.cc" \
  -o /tmp/seekdb_ai_prompt_gtest
/tmp/seekdb_ai_prompt_gtest
```

本次使用 GoogleTest v1.16.0（`6910c9d9165801d8827d628cb72eb7ea9dd538c5`）。

## 尚待完成的验证

提交 `f71f2164d2808e82c8d10b08e69f02273427d5e2` 已在 GitHub Actions 上完成
seekdb release 全量编译。首次验收随后在独立测试阶段中止；本地复现发现测试断言全部通过后，
第二次验收的完整诊断确认，独立测试误用了构建步骤加入 PATH 的 OceanBase 链接器，因找不到
Ubuntu 启动对象而未能配置。工作流已改为对独立测试明确使用系统 CMake、GCC 和 G++，并补充
完整阶段日志和隐藏目录 artifact 上传，以便后续运行保留精确证据。

`test_ob_openai_utils.cpp` 的 8 个 Provider/嵌套 JSON Prompt 回归测试，以及
`test_ob_ai_func_client.cpp` 的 6 个 HTTP 客户端集成测试均依赖 seekdb 运行时，尚未编译或运行，
不属于独立 CMake 目标。HTTP 集成测试默认标记为 DISABLED，需要准备运行时和服务后显式启用，
不能把它们计入已通过测试。官方 `ai_function_enhancement.test` 及数据库回归测试仍待修正后的
Actions 运行。

## 本地 Mock AI 服务

```bash
python3 unittest/sql/engine/expr/ai_mock_service.py --port 8000
```

该服务只是本地协议测试夹具，并非组委会官方服务，不需要模型密钥，默认向量维度为 768。
支持 OpenAI completion/embedding/rerank 和 Ollama generate/embed；`GET /__requests` 可查看请求，
便于后续核对跨行合并和 NULL 行跳过。端点可加 `?fault=reverse`、`duplicate_index`、`missing_index`、
`wrong_dimension`、`bad_number`、`non_object`、`invalid_json`、`http_error`、`empty_completion`
或 `null_completion` 注入异常。

完整运行时构建后，要单独运行 HTTP 客户端测试，需要设置 `SEEKDB_AI_MOCK_URL` 为该服务的 loopback 地址，
并在 GoogleTest 命令中明确指定：

```text
--gtest_also_run_disabled_tests --gtest_filter=AIFuncClientTest.*
```

本地夹具验证不替代组委会评测或实际 SQL 执行。

原有 `ai_prompt.test` 中把 JSON/TEXT 判为不支持的历史预期与本赛题新增能力冲突；未修改这些
官方文件，需要在实际数据库上确认比赛要求的更新预期。

## 完整 CI 验收

`.github/workflows/ai-function-acceptance.yml` 在实现分支 push 或手动触发时执行：固定工具链安装、
修改后的 seekdb 完整构建、独立核心测试、官方 `ai_function_enhancement`、模型/Endpoint DDL 回归，
以及 `ai_function_enhancement_regression`。最后一项使用本地 Mock，覆盖 140 行输入的乱序向量回填、
NULL/过滤/LIMIT、42 条文档的跨批次重排、长 Prompt 和错误恢复；工作流还检查实际请求包含多行且不超过 64 条。

这些完整编译和 SQL 结果以 Actions 对具体提交的实际执行为准，不能由本地核心测试替代。
失败时，关键日志末尾会附到公开 check 注释；完整日志保存在该次运行的 artifact。
