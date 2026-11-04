# FlyingController

把 **FS-SM600**（USB 飞行手柄 / 遥控器）接进 UE 的输入插件。Windows / UE 5.8。

UE 只内置 XInput 和少数主机手柄的 HID 实现，**通用 USB 飞行手柄读不到**——全引擎搜不到一处 `HidP_*` 调用。所以这里自己枚举 HID 设备、读输入报告，再把轴和按键翻译成插件自己的输入键（`FlyingController_*`），供 Enhanced Input 绑定。

## 工作方式

```
FS-SM600 ──USB HID──> FlyingControllerHid（SetupAPI 枚举 + hid.dll 读报告 + 解码 usage）
                              │
                              ▼
                    FlyingControllerDevice（标定 / 归一化 / 死区 / 指数 / 反向）
                              │
                              ▼
              MessageHandler->OnControllerAnalog(FlyingController_*) / OnControllerButtonPressed
                              │
                              ▼
        Enhanced Input / UMG / 任何监听 Gamepad 键的代码
```

- 作为 `IInputDeviceModule` 注册，编辑器或游戏启动时创建一台输入设备。
- 每 2 秒重试连接（可配），支持热插拔；拔掉后自动回到未连接状态。
- 轴按**实测量程**归一化，不依赖 HID 逻辑范围（这台设备的实际行程和 0~255 差很多）。

## 实测设备信息（2026-11-02）

| 项 | 值 |
| --- | --- |
| VID / PID | `0x1781 / 0x0E56` |
| 产品名 | `RC Simulator - G4.5+G5+G5.5+G6 Controller` |
| HID 用途 | Usage Page `0x01`，Usage `0x04`（Joystick） |
| 报告长度 | 8 字节 |
| 轴描述 | 6 条（展开后 6 个轴）：`0x30 X`、`0x31 Y`、`0x32 Z`、`0x33 Rx`、`0x34 Ry`、`0x41` |
| 按键描述 | 1 条（按钮 1~8 映射手柄面键） |

**`0x33 Rx` 是闲置通道**（恒在 4 左右），`0x41` 恒为 0。有效的是 X / Y / Z / Ry 四根。

## 映射（已实测确认）

| 通道 | 摇杆 | HID usage | 原始行程（实测） | 输出键（Enhanced Input 可绑） |
| --- | --- | --- | --- | --- |
| 横滚 Roll | 右摇杆 左右 | `0x30` X | 4 ~ 242 | `FlyingController_Roll` |
| 俯仰 Pitch | 右摇杆 前后 | `0x31` Y | 27 ~ 233 | `FlyingController_Pitch` |
| 油门 Throttle | 左摇杆 前后 | `0x32` Z | 4 ~ 202 | `FlyingController_Throttle`（0~1 映射到 -1~+1） |
| 偏航 Yaw | 左摇杆 左右 | `0x34` Ry | 4 ~ 244 | `FlyingController_Yaw` |

> 偏航是 **`0x34`（Ry）不是 `0x35`（Rz）**。通用手柄常见 Rz，这台设备没有 Rz。

## 为什么不用 Gamepad_* 键

早期版本把输入发成 `Gamepad_LeftX` / `Gamepad_RightY` 这类标准手柄键，结果是**编辑器会把这些输入当成手柄**：一推杆，编辑器视口/UI 导航跟着动，等于手柄接管了编辑器控制。

现在改成插件自己注册的 `FlyingController_*` 键：

- 编辑器 / Slate 不认识这些键，不会触发任何视口或 UI 导航，**编辑态下推杆没有任何副作用**（只有打了日志）。
- 这些键注册为 `Axis1D`，Enhanced Input 能直接绑定并拿到连续值。
- 真正的手柄（Xbox 等）不受影响，仍然占用 `Gamepad_*`。

如果确实需要复用现成的 Gamepad 绑定，可在工程设置里打开 `bEmitLegacyGamepadKeys`，此时会**额外**再发一份 `Gamepad_*`（代价是编辑器又会把手柄当游戏手柄）。

## Enhanced Input 接入

1. Content Browser 新建 **Input Action**：`IA_Throttle`，Value Type 选 **Axis1D (float)**。
2. 新建/打开一个 **Input Mapping Context**（如 `IMC_Flight`），加一条 mapping：`IA_Throttle` ← 键位选 **Flying Controller → 飞行手柄 油门**（键名 `FlyingController_Throttle`）。
3. 同样的方式建 `IA_Yaw` / `IA_Pitch` / `IA_Roll`，分别绑 `FlyingController_Yaw` / `_Pitch` / `_Roll`。
4. 在 PlayerController / Character 的 `BeginPlay` 里 `AddMappingContext(IMC_Flight, 0)`，然后用 `EnhancedInputComponent::BindAction` 接值。

按键键同理：`FlyingController_Button1` ~ `_Button8`，是普通数字键，可用 `Triggered` / `Started` / `Completed`。

> 键位是运行时注册的，编辑器启动时插件已加载，所以 Input Action 的键位列表里能直接看到；如果列表里没有，重启一次编辑器。

## 工程设置

Project Settings → Plugins → Flying Controller（`/Script/FlyingController.FlyingControllerSettings`）：设备过滤（VID/PID/名称/序号）、四轴 usage 映射、死区/指数/反向、油门是否自回中、兼容 Gamepad 输出、调试日志开关。

## 标定（为什么不能按 0~255 硬算）

两个实测偏差让硬归一化不可用：

1. **静息中位是 122，不是 127.5** —— 按逻辑中位算会得到恒定 -0.04 的偏置，越过死区就是持续漂移。
2. **物理行程不满**：油门 4~202、俯仰 27~233 —— 按 0~255 算，油门永远只能到 78%，俯仰下拉只能到 -0.92。

处理方式：

- 首次读数定中位（刚连上时摇杆通常回中），之后只在「接近中位」时缓慢跟随，不会把极限位置误当中位。
- 行程只扩不缩地学：推到底一次就记住。
- 回中轴**按方向分别归一化**（`Raw ≥ 中位` 用上行行程，`< 中位` 用下行行程），解决前后行程不对称。
- 标定是内存状态，断开会清空；换设备或觉得中位偏了，跑一次 `FlyingController.Calibrate` 重新学。

## 工程设置

Project Settings → Plugins → Flying Controller（`/Script/FlyingController.FlyingControllerSettings`）：设备过滤（VID/PID/名称/序号）、四轴 usage 映射、死区/指数/反向、油门是否自回中、调试日志开关。

## 编辑器面板：摇杆状态监控

关卡编辑器工具栏上有一个 **「手柄」** 按钮，点开浮动窗口，实时显示：

- 设备状态（名称 / VID:PID，或「未连接」）
- 左摇杆圆盘：偏航（左右）+ 油门（上下），带四方向文字标注
- 右摇杆圆盘：横滚（左右）+ 俯仰（上下）
- 原始轴读数 `X / Y / Z / Rx / Ry / S0`（按 0~255 归一化）
- 四通道值 `THR / YAW / PIT / ROL` 与当前按下的按键

![面板截图](../../../docs/images/flyingcontroller_panel.png)

数据来自运行期模块的 `FlyingControllerState::Get()`（每帧由设备写入），面板按帧重绘。

控制台也可打开：`FlyingController.OpenMonitor`。

> 调试用：加 `-FlyingControllerAutoOpenMonitor` 启动参数，编辑器加载约 20 秒后自动打开（开机早期用 `-ExecCmds` 打开会被随后的布局恢复关掉，所以走延迟路径）。

## 回调事件：设备连接 / 断开

插件在检测到手柄接入或掉线时各广播一个事件，C++ 和蓝图两条路都可用。

### C++

```cpp
#include "FlyingControllerEvents.h"

void UMyClass::BeginPlay()
{
    // 检测到设备连接
    ConnectedHandle = FFlyingControllerEvents::OnDeviceConnected().AddUObject(this, &UMyClass::HandleStickConnected);
    // 检测到设备断开
    DisconnectedHandle = FFlyingControllerEvents::OnDeviceDisconnected().AddUObject(this, &UMyClass::HandleStickDisconnected);
}

void UMyClass::EndPlay(...)
{
    FFlyingControllerEvents::OnDeviceConnected().Remove(ConnectedHandle);
    FFlyingControllerEvents::OnDeviceDisconnected().Remove(DisconnectedHandle);
}

void UMyClass::HandleStickConnected(const FLyingControllerDeviceInfo& Info)
{
    UE_LOG(LogTemp, Log, TEXT("手柄接入：%s (%04X:%04X)"), *Info.ProductName, Info.VendorId, Info.ProductId);
}

void UMyClass::HandleStickDisconnected(const FLyingControllerDeviceInfo& Info)
{
    UE_LOG(LogTemp, Warning, TEXT("手柄断开：%s"), *Info.ProductName);
}
```

- 事件在游戏线程广播（设备 Tick 所在线程）。
- 断开事件覆盖三种来源：**拔线 / 读错误 / 手动 `FlyingController.Reconnect`**。
- 断线后插件每 2 秒重试，重新插上会再次触发「连接」事件，无需重启。
- 回调参数 `FLyingControllerDeviceInfo`：`ProductName`、`VendorId`、`ProductId`、`DevicePath`、`bConnected`。

### 蓝图

`UFlyingControllerSubsystem`（Game Instance Subsystem，蓝图里 `Get Game Instance → Get Subsystem → FlyingController`）：

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `OnDeviceConnected` | 事件（BlueprintAssignable） | 检测到连接 |
| `OnDeviceDisconnected` | 事件（BlueprintAssignable） | 检测到断开 |
| `IsDeviceConnected` | 纯函数 | 当前是否连接（**实时读取运行期状态**，不依赖事件是否收到过） |
| `GetConnectedDevice` | 纯函数 | 当前（或最近一次）的设备信息 |

`IsDeviceConnected` 直接读 `FlyingControllerState::Get().bConnected`：手柄经常在 GameInstance（子系统）创建之前就连上了，那一次「连接」事件收不到，如果只靠事件缓存结果会永远是 false。子系统初始化时还会按当前状态补广播一次 `OnDeviceConnected`，保证蓝图订阅者拿到正确的初始状态。

子系统会自动订阅/退订底层 C++ 事件，不需要手动清理。

## 控制台命令

| 命令 | 作用 |
| --- | --- |
| `FlyingController.ListDevices` | 列出系统里所有摇杆类 HID（VID/PID/名称/usage） |
| `FlyingController.Status` | 连接状态 + 四个通道当前值 |
| `FlyingController.Calibrate`（别名 `Recenter`） | 清空标定并重新学习 |
| `FlyingController.Reconnect` | 断开重连 |
| `FlyingController.Debug 1` / `0` | 实时打印四个通道值（0.2 秒节流） |
| `FlyingController.Raw 1` / `0` | 打印每个轴的原始值，排查映射用 |

## 验证记录（2026-11-02，实机）

| 项 | 结果 |
| --- | --- |
| 设备识别 | `VID_1781&PID_0E56`，6 条轴描述、1 条按键描述、报告 8 字节 |
| 静息中位 | Yaw / Pitch / Roll 均为 `+0.000`，无漂移 |
| 油门 | `0.000`（最下）~ `1.000`（最上），回中 `0.49` |
| 偏航 | `-1.000 ~ +1.000` |
| 俯仰 | `-1.000 ~ +1.000`（按方向归一化后） |
| 横滚 | `-1.000 ~ +1.000` |
| 连接事件 | 启动时广播 `OnDeviceConnected`（日志 `广播事件 OnDeviceConnected`） |
| 断开事件 | 拔线后 1 秒内广播 `OnDeviceDisconnected`；重新插上约 6 秒后再次广播 `OnDeviceConnected` |
| 输入键注册 | 模块加载日志：`已注册 4 个轴键与 8 个按键键`；连接时日志打印输出键名，兼容 Gamepad 输出默认「关」 |

**尚未实测**：在 Input Action 里绑定 `FlyingController_*` 后的实际取值（需要 PIE + 一个绑定了这些键的 IMC）；按键键的对应关系同样待按。

## 目录

```
Plugins/FlyingController/
├── FlyingController.uplugin
├── README.md
└── Source/FlyingController/
    ├── FlyingController.Build.cs
    ├── Public/
    │   ├── FlyingControllerSettings.h      # UDeveloperSettings
    │   ├── FlyingControllerKeys.h          # 专用输入键（FlyingController_*）
    │   ├── FlyingControllerEvents.h        # 连接 / 断开 C++ 事件
    │   ├── FlyingControllerTypes.h         # FLyingControllerDeviceInfo
    │   └── FlyingControllerSubsystem.h     # 蓝图事件与状态查询
    └── Private/
        ├── FlyingControllerModule.cpp/.h   # IInputDeviceModule 入口（注册输入键）
        ├── FlyingControllerDevice.cpp/.h   # IInputDevice：标定、归一化、发事件、控制台命令
        ├── FlyingControllerKeys.cpp        # 键注册 / 注销
        ├── FlyingControllerEvents.cpp      # 静态委托实例
        ├── FlyingControllerSubsystem.cpp   # 蓝图桥接
        └── FlyingControllerHid.cpp/.h      # Windows HID：枚举 / 打开 / 读报告 / 解码 usage
```

## 踩过的坑（改之前先看）

- 设备用**范围型** HID 描述（`UsageMin..UsageMax`），不展开就一个轴都注册不上。
- **自注册的键必须同时调 `EKeys::AddMenuCategoryDisplayInfo`**：`GetMenuCategoryDisplayName` 对未注册分类会回退成 `Keyboard` 的显示名，键会混进键盘分类里，看起来像没注册。
- **键位选择器的搜索只匹配显示名**，所以显示名里要带键名（`FlyingController Throttle（油门）`），否则搜 `FlyingController` 一无所获。
- **别把飞行手柄输入发成 `Gamepad_*`**：编辑器会把它当游戏手柄，视口/UI 导航跟着动。用插件自己的键（`Axis1D`）才干净。
- 刚打开设备时报告缓冲区全 0，若拿它当首个标定样本，中位会被记成 0，真实值 122 会算成 `+0.99`。必须等读到真实报告再更新通道。
- 行程未学完时不要回退到 0~255：物理行程只有 4~202，会让油门最大只到 0.78。
- `GUID_DEVINTERFACE_HID` 在 SDK 头里只有声明，需自带定义；`TRUE/FALSE` 被 UE 的 `HideWindowsPlatformTypes` 撤掉，要用小写。
- **关机时不要在 `Disconnect()` 里回调引擎**：`Internal_MapInputDeviceToUser(Disconnected)` 会触发引擎侧监听（`InputDebuggingEditor`、输入设备子系统等），关机时它们已析构，会 access violation（读到 `0x138`）。用 `IsEngineExitRequested()` 判断，关机路径只关 HID、不注销、不广播。

## 已知局限

- 只有 Windows 实现（`Build.cs` 在其它平台直接报错）。
- 不做力反馈（`SetChannelValue` 空实现）。
- 标定不持久化（每次连接重新学）；设备固定绑主用户，暂不支持多人多设备。
