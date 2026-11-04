// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 枚举到的一台 HID 设备的基本信息。 */
struct FFlyingControllerHidDeviceInfo
{
	FString DevicePath;
	FString ProductName;
	uint16 VendorId = 0;
	uint16 ProductId = 0;
	uint16 UsagePage = 0;
	uint16 Usage = 0;

	/** 便于日志阅读的设备摘要。 */
	FString ToString() const
	{
		return FString::Printf(TEXT("VID_%04X&PID_%04X \"%s\" (usage %04X:%04X) [%s]"),
			VendorId, ProductId, *ProductName, UsagePage, Usage, *DevicePath);
	}
};

/** 单个轴的取值范围，用于把原始值归一化。 */
struct FFlyingControllerAxisRange
{
	int32 LogicalMin = 0;
	int32 LogicalMax = 255;
};

/**
 * FS-SM600 这类 USB 飞行手柄的 HID 读写封装。
 *
 * 只依赖 Windows HID API（hid.dll / setupapi.lib）。所有 Windows 类型都藏在 Impl 里，
 * 头文件本身保持平台无关，方便以后加别的平台实现。
 */
class FFlyingControllerHid
{
public:
	FFlyingControllerHid();
	~FFlyingControllerHid();

	FFlyingControllerHid(const FFlyingControllerHid&) = delete;
	FFlyingControllerHid& operator=(const FFlyingControllerHid&) = delete;

	/** 枚举系统里所有「摇杆 / 手柄」类 HID 设备（Usage Page 0x01，Usage 0x04/0x05/0x08）。 */
	static TArray<FFlyingControllerHidDeviceInfo> EnumerateDevices();

	/** 打开指定设备。成功后可以 PollReport / ReadValue。 */
	bool Open(const FFlyingControllerHidDeviceInfo& Info);
	void Close();
	bool IsOpen() const;

	/** 读一条最新输入报告；没有新数据时返回 false（不阻塞）。 */
	bool PollReport();

	/** 读某个轴在当前报告里的原始值。 */
	bool ReadValue(uint16 UsagePage, uint16 Usage, uint32& OutValue, FFlyingControllerAxisRange& OutRange) const;

	/** 当前按下的所有按钮（HID Usage Page 0x09 的 usage id）。 */
	void ReadPressedButtons(TArray<uint16>& OutButtons) const;

	/** 某个轴是否在设备的能力列表里。 */
	bool HasAxis(uint16 UsagePage, uint16 Usage) const;

	/** 用于把能力列表打到日志里，方便第一次接设备时核对轴编号。 */
	FString DescribeCaps() const;

	/** 把当前报告里所有轴的原始值拼成一行，用于现场判定「哪根杆对应哪个 usage」。 */
	FString GetRawValuesString() const;

	const FFlyingControllerHidDeviceInfo& GetInfo() const { return DeviceInfo; }

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
	FFlyingControllerHidDeviceInfo DeviceInfo;
};
