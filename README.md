## iouxx

现代 C++26 风格的 [liburing](https://github.com/axboe/liburing) 轻量级包装库。提供类型安全、可组合的 io_uring 操作抽象（operation），支持回调、多次触发 (multishot) 超时、取消、以及 IPv4/IPv6 地址解析与格式化等实用组件。

> 状态: 早期开发中 (WIP)。接口仍可能调整，请关注后续版本。
> 此文档由 AI 协助生成。

### ✨ 特性概览

- C++26 语法：使用 concepts / `std::expected` / Ranges / 显式对象形参 (`this Self&`) 等特性。
- RAII 封装：`io_uring_xx` 自动初始化与清理，移动安全。
- Operation 抽象：统一的 `operation_base` + CRTP 风格派生，回调擦除，用户持有对象即生命周期受控。
- 回调模型：错误以 `std::error_code` 传递；正值结果由具体 operation 自行解释。
- 超时支持：一次性 `timeout_operation` 与多次触发 `multishot_timeout_operation`，支持 steady / system / boottime 三种时钟。
- 取消机制：按 user data (`operation_identifier`) 或文件描述符批量/单个取消，返回取消数量。
- IPv4 / IPv6：零分配解析（失败返回 `std::expected`），格式化支持多种表示；提供编译期字面量 `_ipv4` / `_ipv6`。
- `boottime_clock`：包装 Linux `CLOCK_BOOTTIME`，可在系统挂起期间继续计时。
- 纯头文件公共接口（当前阶段）；依赖外部 `liburing`。

### 🧱 设计要点

- “最薄”封装：不隐藏 io_uring 核心语义，只在安全性/可读性处提供现代 C++ 抽象。
- 错误策略：系统调用返回值 < 0 转换为 `std::error_code`；非真正错误的特殊码由具体 operation 内部语义化（如 `-ETIME` 被视为正常超时）。
- 生命周期显式：用户必须保证 `operation` 对象在完成前存活；提交后内核通过 user_data 回调定位对象。
- 可扩展：新增操作仅需派生 `operation_base`，实现 `build()` 与 `do_callback()`。

### 📦 依赖与环境

| 需求 | 说明 |
|------|------|
| OS | Linux (io_uring 支持的内核，建议 >= 6.0) |
| 库 | 已安装 `liburing` 开发包 |
| 编译器 | 支持 C++26（clang >= 20 / gcc >= 15 建议） |
| 构建 | 使用 [xmake](https://xmake.io) |

### 🚀 构建与测试

```bash
# 配置 (可选：选择 debug/release)
xmake f -m debug
# 构建所有目标
xmake
# 运行测试
xmake test
```

### 🕹️ 快速示例

#### 1. Noop 操作
```cpp
iouxx::io_uring_xx ring(64);
iouxx::noop_operation op(ring, [](std::error_code ec){ /* ... */ });
op.submit();
auto r = ring.wait_for_result().value();
r(); // 触发回调
```

#### 2. 一次性超时
```cpp
using namespace std::chrono_literals;
iouxx::timeout_operation t(ring, [](std::error_code ec){ /* 超时后执行 */ });
t.wait_for(50ms).submit();
ring.wait_for_result().value()();
```

#### 3. 多次触发 (multishot) 超时
```cpp
iouxx::multishot_timeout_operation mt(ring,
		[](std::error_code ec, bool more){ /* more=true 表示仍有后续 */ });
mt.wait_for(10ms).repeat(5).submit();
while (true) {
		auto res = ring.wait_for_result().value();
		res();
		if (res.result() == 0 /* 回调里可记录 more=false */) { /* 视语义终止条件 */ }
}
```

#### 4. 取消操作
```cpp
iouxx::timeout_operation timer(ring, [](std::error_code){ /* ... */ });
timer.wait_for(std::chrono::seconds(5)).submit();
auto id = timer.identifier();
iouxx::cancel_operation cancel(ring, [](std::error_code ec, std::size_t n){ /* n=取消个数 */ });
cancel.target(id).cancel_one().submit();
ring.wait_for_result().value()(); // 处理取消完成
```

#### 5. 网络操作
```cpp
// TODO
```

### 🧪 当前测试覆盖

- `test_noop.cpp`: 基础 noop 提交与回调
- `test_timeout.cpp`: 一次性超时与 multishot 超时
- `test_ip_utils.cpp`: IPv4/IPv6 解析、格式化与字面量

### 📂 目录结构（核心）

```
include/
	iouringxx.hpp         # 核心 io_uring 封装与 operation 基础设施
	iouxx.hpp             # 统一聚合头
	boottime_clock.hpp    # CLOCK_BOOTTIME 时钟
	iouops/               # 各类操作 (noop/timeout/cancel/...)
		network/ip.hpp      # IPv4/IPv6 工具
src/
	main.cpp              # 示例入口（占位）
test/                   # 单元/示例测试
xmake.lua               # 构建脚本
```

### 🛣️ Roadmap / TODO 摘要

短期：
- File IO 操作实现 (`fileio.hpp`)
- Socket IO 操作实现 (`socketio.hpp`)
- Cancel 操作测试完善

中期：
- C++ Modules 支持
- 移除 chrono 回退（等待 libc++ 更新）
- CI/CD (构建 + 测试 + 格式检查)

长期：
- 更丰富的 network / file 语义操作（读写、accept、connect 等）
- 与协程/执行器整合示例
- 文档站点 & Benchmark

详见 `TODO.md`。

### 🔌 扩展自定义 Operation

1. 继承 `operation_base`，构造时传入 `op_tag<Derived>` 与 `ring.native()`。
2. 实现 `void build(io_uring_sqe*) & noexcept` 填充 SQE。
3. 实现 `void do_callback(int ev, std::int32_t flags)` 解析结果并调用用户回调。
4. 用户持有实例并调用 `submit()`；完成后通过 `operation_result` 调用其 `operator()` 触发回调。

### ⚠️ 注意事项

TODO

### 📄 许可证

见 `LICENSE`

### 🤝 贡献

TODO
