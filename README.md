# Introduction of Bluetooth Protocol Stack

## Overview

This project is a fully self-developed Bluetooth protocol stack implemented in standard C++. It is not bound to any specific operating system kernel and features excellent cross-platform portability. It can be ported to any platform with a compliant C++ compiler, and currently supports Windows, Android, Linux, and various RTOS systems.

Adopting a modular architecture and task scheduling powered by a thread pool, the protocol stack achieves clear decoupling between modules. It supports feature tailoring, independent unit testing and incremental iterative development.

## Core Architecture

### 1. Cross-platform C++ Abstraction Layer

All core protocol logic is written in standard C++. Operating-system-specific primitives such as thread management, sleep, memory allocation, timer and HCI I/O are encapsulated inside a separated platform adaptation layer. No OS-specific hardcoded logic exists in upper-layer protocol modules. Only a small set of adaptation APIs need to be implemented for porting to new targets.
The stack supports flexible configuration: BR/EDR-only, BLE-only or full dual-mode deployment according to product requirements.

### 2. Modular Design

The stack is split into independent modules following Bluetooth specification layers, including HCI, L2CAP, SDP, RFCOMM, HID, A2DP, LE Audio, etc.

- Direct access to internal member variables across different modules is forbidden.
- Inter-module interaction is implemented exclusively via task or message delivery.
- Modules can be configured with serial execution mode: the framework guarantees that all tasks belonging to one module will never run concurrently. Worker threads may vary between task invocations, but parallel execution of module code is eliminated, which reduces lock contention and improves runtime performance.
- Global device information (device name, controller parameters, etc.) is managed by a dedicated information management module, which acts like a protected database and provides thread-safe query and modification interfaces.

### 3. Thread Pool Based Task Scheduler

All protocol events, incoming HCI packets and state machine transitions are wrapped as tasks and dispatched by the built-in thread pool.

- State machines (ACL state machine, L2CAP channel state machine, etc.) run entirely within task contexts, avoiding complex logic inside interrupt context and simplifying debugging and signal sequence tracing.
- Unified task model for all operations, such as pending packet cleanup tasks, L2CAP signaling parsing, ACL connection management.

## Key Features

- Dual-mode support for BR/EDR and BLE.
- Full L2CAP signaling implementation: CONNECTION_REQ, configuration negotiation, QoS, extended flow specification, retransmission & flow control option parsing.
- Robust packet parser with malformed packet validation, truncated payload detection and proper rejection for invalid signaling to defend against abnormal remote packets.
- Fine-grained feature trimming to reduce memory footprint for resource-limited embedded targets.
- Clear separation between protocol business logic and platform-dependent code, lowering maintenance cost for multi-product mass deployment.

## License
Bluethread is dual-licensed:
1. **GNU General Public License v3.0 (GPLv3)** — Free for open-source projects.
   See the [LICENSE](./LICENSE) file for full GPLv3 terms.
2. **Commercial Proprietary License** — For closed-source commercial product integration.
   If you intend to embed Bluethread into closed-source firmware/software for commercial sale, please contact the author to obtain a commercial license.

## Disclaimer
This software is provided as-is, without warranty of any kind, express or implied.
The author shall not be liable for any damages arising from the use of this stack.

## Contributing
By submitting pull requests to this repository, you agree that your contributions are licensed under both GPLv3 and the commercial proprietary license of Bluethread.

Note: GPLv3 has copyleft requirements. If you statically link this stack into your product and distribute it, your full product must comply with GPLv3.

Copyright (C) 2026 wangfei <kingpin58@163.com>