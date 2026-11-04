// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlyingControllerHid.h"

#if PLATFORM_WINDOWS

#include "Windows/WindowsHWrapper.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include "Windows/HideWindowsPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogFlyingControllerHid, Log, All);

namespace
{
	constexpr uint16 kGenericDesktopPage = 0x01;
	constexpr uint16 kButtonPage = 0x09;
	constexpr uint16 kUsageJoystick = 0x04;
	constexpr uint16 kUsageGamepad = 0x05;
	constexpr uint16 kUsageMultiAxis = 0x08;

	/** {4D1E55B2-F16F-11CF-88CB-001111000030}，HID 设备接口 GUID（SDK 头里只声明，这里自带一份）。 */
	const GUID HidDeviceInterfaceGuid = { 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

	bool IsControllerUsage(uint16 UsagePage, uint16 Usage)
	{
		return UsagePage == kGenericDesktopPage
			&& (Usage == kUsageJoystick || Usage == kUsageGamepad || Usage == kUsageMultiAxis);
	}

	FString WideToFString(const wchar_t* In)
	{
		return In ? FString(In) : FString();
	}
}

/** Windows 相关的全部状态。 */
struct FFlyingControllerHid::FImpl
{
	HANDLE DeviceHandle = INVALID_HANDLE_VALUE;
	PHIDP_PREPARSED_DATA PreparsedData = nullptr;
	HIDP_CAPS Caps = {};
	OVERLAPPED Overlapped = {};
	HANDLE ReadEvent = nullptr;

	TArray<uint8> ReportBuffer;
	TArray<uint8> ButtonBuffer;

	/** key = (UsagePage << 16) | Usage → 逻辑范围与所在 link collection。 */
	TMap<uint32, TPair<FFlyingControllerAxisRange, USHORT>> AxisCaps;

	~FImpl()
	{
		CloseDevice();
	}

	void CloseDevice()
	{
		if (PreparsedData)
		{
			HidD_FreePreparsedData(PreparsedData);
			PreparsedData = nullptr;
		}
		if (DeviceHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(DeviceHandle);
			DeviceHandle = INVALID_HANDLE_VALUE;
		}
		if (ReadEvent)
		{
			CloseHandle(ReadEvent);
			ReadEvent = nullptr;
		}
		AxisCaps.Reset();
		ReportBuffer.Reset();
		ButtonBuffer.Reset();
	}
};

FFlyingControllerHid::FFlyingControllerHid()
	: Impl(MakeUnique<FImpl>())
{
}

FFlyingControllerHid::~FFlyingControllerHid() = default;

TArray<FFlyingControllerHidDeviceInfo> FFlyingControllerHid::EnumerateDevices()
{
	TArray<FFlyingControllerHidDeviceInfo> Result;

	HDEVINFO DeviceInfoSet = SetupDiGetClassDevs(&HidDeviceInterfaceGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (DeviceInfoSet == INVALID_HANDLE_VALUE)
	{
		UE_LOG(LogFlyingControllerHid, Warning, TEXT("SetupDiGetClassDevs(HID) 失败，错误码 %u"), GetLastError());
		return Result;
	}

	SP_DEVICE_INTERFACE_DATA InterfaceData = {};
	InterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	for (DWORD Index = 0; SetupDiEnumDeviceInterfaces(DeviceInfoSet, nullptr, &HidDeviceInterfaceGuid, Index, &InterfaceData); ++Index)
	{
		DWORD RequiredSize = 0;
		SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &InterfaceData, nullptr, 0, &RequiredSize, nullptr);
		if (RequiredSize == 0)
		{
			continue;
		}

		TArray<uint8> DetailStorage;
		DetailStorage.SetNumZeroed(RequiredSize);
		SP_DEVICE_INTERFACE_DETAIL_DATA* Detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(DetailStorage.GetData());
		Detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (!SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &InterfaceData, Detail, RequiredSize, nullptr, nullptr))
		{
			continue;
		}

		HANDLE Handle = CreateFile(Detail->DevicePath,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			nullptr);

		if (Handle == INVALID_HANDLE_VALUE)
		{
			continue;
		}

		HIDD_ATTRIBUTES Attributes = {};
		Attributes.Size = sizeof(HIDD_ATTRIBUTES);

		PHIDP_PREPARSED_DATA Preparsed = nullptr;
		if (HidD_GetAttributes(Handle, &Attributes) && HidD_GetPreparsedData(Handle, &Preparsed) && Preparsed)
		{
			HIDP_CAPS Caps = {};
			if (HidP_GetCaps(Preparsed, &Caps) == HIDP_STATUS_SUCCESS && IsControllerUsage(Caps.UsagePage, Caps.Usage))
			{
				FFlyingControllerHidDeviceInfo Info;
				Info.DevicePath = WideToFString(Detail->DevicePath);
				Info.VendorId = Attributes.VendorID;
				Info.ProductId = Attributes.ProductID;
				Info.UsagePage = Caps.UsagePage;
				Info.Usage = Caps.Usage;

				wchar_t ProductName[128] = {};
				if (HidD_GetProductString(Handle, ProductName, sizeof(ProductName)))
				{
					Info.ProductName = WideToFString(ProductName);
				}
				Result.Add(Info);
			}

			HidD_FreePreparsedData(Preparsed);
		}

		CloseHandle(Handle);
	}

	SetupDiDestroyDeviceInfoList(DeviceInfoSet);
	return Result;
}

bool FFlyingControllerHid::Open(const FFlyingControllerHidDeviceInfo& Info)
{
	Close();
	DeviceInfo = Info;

	HANDLE Handle = CreateFile(*Info.DevicePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		nullptr);

	if (Handle == INVALID_HANDLE_VALUE)
	{
		UE_LOG(LogFlyingControllerHid, Warning, TEXT("打开设备失败（%u）：%s"), GetLastError(), *Info.DevicePath);
		return false;
	}

	Impl->DeviceHandle = Handle;

	if (!HidD_GetPreparsedData(Handle, &Impl->PreparsedData) || !Impl->PreparsedData)
	{
		UE_LOG(LogFlyingControllerHid, Warning, TEXT("HidD_GetPreparsedData 失败：%s"), *Info.ProductName);
		Impl->CloseDevice();
		return false;
	}

	if (HidP_GetCaps(Impl->PreparsedData, &Impl->Caps) != HIDP_STATUS_SUCCESS)
	{
		UE_LOG(LogFlyingControllerHid, Warning, TEXT("HidP_GetCaps 失败：%s"), *Info.ProductName);
		Impl->CloseDevice();
		return false;
	}

	// 收集所有输入轴的逻辑范围，后面归一化要用。
	if (Impl->Caps.NumberInputValueCaps > 0)
	{
		TArray<HIDP_VALUE_CAPS> ValueCaps;
		ValueCaps.SetNumZeroed(Impl->Caps.NumberInputValueCaps);
		USHORT Length = Impl->Caps.NumberInputValueCaps;

		if (HidP_GetValueCaps(HidP_Input, ValueCaps.GetData(), &Length, Impl->PreparsedData) == HIDP_STATUS_SUCCESS)
		{
			for (USHORT i = 0; i < Length; ++i)
			{
				const HIDP_VALUE_CAPS& Cap = ValueCaps[i];

				FFlyingControllerAxisRange Range;
				Range.LogicalMin = Cap.LogicalMin;
				Range.LogicalMax = Cap.LogicalMax;

				// 有的设备把一组轴写在一条范围描述里（UsageMin..UsageMax），
				// 这时要把区间展开成一个个 usage，否则一个轴都注册不上。
				const USAGE FirstUsage = Cap.IsRange ? Cap.Range.UsageMin : Cap.NotRange.Usage;
				const USAGE LastUsage = Cap.IsRange ? Cap.Range.UsageMax : Cap.NotRange.Usage;

				for (uint32 CapUsage = FirstUsage; CapUsage <= LastUsage; ++CapUsage)
				{
					const uint32 Key = (static_cast<uint32>(Cap.UsagePage) << 16) | CapUsage;
					Impl->AxisCaps.Add(Key, TPair<FFlyingControllerAxisRange, USHORT>(Range, Cap.LinkCollection));
				}
			}
		}
	}

	UE_LOG(LogFlyingControllerHid, Log, TEXT("设备能力：%d 条轴描述（展开后 %d 个轴）、%d 条按键描述、报告长度 %d"),
		Impl->Caps.NumberInputValueCaps, Impl->AxisCaps.Num(), Impl->Caps.NumberInputButtonCaps, Impl->Caps.InputReportByteLength);

	Impl->ReadEvent = CreateEvent(nullptr, true, false, nullptr);
	if (!Impl->ReadEvent)
	{
		UE_LOG(LogFlyingControllerHid, Warning, TEXT("CreateEvent 失败"));
		Impl->CloseDevice();
		return false;
	}

	Impl->ReportBuffer.SetNumZeroed(FMath::Max<int32>(Impl->Caps.InputReportByteLength, 2));
	Impl->ButtonBuffer.SetNumZeroed(FMath::Max<int32>(HidP_MaxUsageListLength(HidP_Input, kButtonPage, Impl->PreparsedData), 1) * sizeof(USAGE));

	// 缓冲更多报告，避免高刷新率手柄丢帧。
	HidD_SetNumInputBuffers(Handle, 32);

	UE_LOG(LogFlyingControllerHid, Log, TEXT("已打开飞行手柄：%s，报告长度 %d"), *Info.ToString(), Impl->Caps.InputReportByteLength);
	return true;
}

void FFlyingControllerHid::Close()
{
	Impl->CloseDevice();
}

bool FFlyingControllerHid::IsOpen() const
{
	return Impl->DeviceHandle != INVALID_HANDLE_VALUE;
}

bool FFlyingControllerHid::PollReport()
{
	if (!IsOpen() || Impl->ReportBuffer.Num() == 0)
	{
		return false;
	}

	ResetEvent(Impl->ReadEvent);
	Impl->Overlapped = {};
	Impl->Overlapped.hEvent = Impl->ReadEvent;

	DWORD BytesRead = 0;
	BOOL bRead = ReadFile(Impl->DeviceHandle, Impl->ReportBuffer.GetData(), Impl->ReportBuffer.Num(), &BytesRead, &Impl->Overlapped);

	if (!bRead)
	{
		const DWORD Error = GetLastError();
		if (Error == ERROR_IO_PENDING)
		{
			// 非阻塞：这一帧没数据就直接返回。
			if (WaitForSingleObject(Impl->ReadEvent, 0) != WAIT_OBJECT_0)
			{
				return false;
			}

			if (!GetOverlappedResult(Impl->DeviceHandle, &Impl->Overlapped, &BytesRead, false))
			{
				return false;
			}
		}
		else if (Error == ERROR_DEVICE_NOT_CONNECTED || Error == ERROR_HANDLE_EOF)
		{
			UE_LOG(LogFlyingControllerHid, Warning, TEXT("设备已断开：%s"), *DeviceInfo.ProductName);
			Close();
			return false;
		}
		else
		{
			return false;
		}
	}

	return BytesRead > 0;
}

bool FFlyingControllerHid::ReadValue(uint16 UsagePage, uint16 Usage, uint32& OutValue, FFlyingControllerAxisRange& OutRange) const
{
	const uint32 Key = (static_cast<uint32>(UsagePage) << 16) | Usage;
	const TPair<FFlyingControllerAxisRange, USHORT>* Found = Impl->AxisCaps.Find(Key);
	if (!Found || !Impl->PreparsedData || Impl->ReportBuffer.Num() == 0)
	{
		return false;
	}

	ULONG Value = 0;
	const NTSTATUS Status = HidP_GetUsageValue(
		HidP_Input,
		UsagePage,
		Found->Value,
		Usage,
		&Value,
		Impl->PreparsedData,
		reinterpret_cast<PCHAR>(Impl->ReportBuffer.GetData()),
		Impl->ReportBuffer.Num());

	if (Status != HIDP_STATUS_SUCCESS)
	{
		return false;
	}

	OutValue = static_cast<uint32>(Value);
	OutRange = Found->Key;
	return true;
}

void FFlyingControllerHid::ReadPressedButtons(TArray<uint16>& OutButtons) const
{
	OutButtons.Reset();
	if (!Impl->PreparsedData || Impl->ReportBuffer.Num() == 0 || Impl->ButtonBuffer.Num() == 0)
	{
		return;
	}

	ULONG UsageLength = static_cast<ULONG>(Impl->ButtonBuffer.Num() / sizeof(USAGE));
	USAGE* Usages = reinterpret_cast<USAGE*>(Impl->ButtonBuffer.GetData());

	if (HidP_GetUsages(HidP_Input, kButtonPage, 0, Usages, &UsageLength, Impl->PreparsedData,
		reinterpret_cast<PCHAR>(Impl->ReportBuffer.GetData()), Impl->ReportBuffer.Num()) == HIDP_STATUS_SUCCESS)
	{
		for (ULONG i = 0; i < UsageLength; ++i)
		{
			OutButtons.Add(static_cast<uint16>(Usages[i]));
		}
	}
}

bool FFlyingControllerHid::HasAxis(uint16 UsagePage, uint16 Usage) const
{
	const uint32 Key = (static_cast<uint32>(UsagePage) << 16) | Usage;
	return Impl->AxisCaps.Contains(Key);
}

FString FFlyingControllerHid::DescribeCaps() const
{
	FString Result;
	for (const TPair<uint32, TPair<FFlyingControllerAxisRange, USHORT>>& Pair : Impl->AxisCaps)
	{
		const uint16 Page = static_cast<uint16>(Pair.Key >> 16);
		const uint16 Usage = static_cast<uint16>(Pair.Key & 0xFFFF);
		Result += FString::Printf(TEXT("\n    轴 usage %04X:%04X  范围 [%d, %d]  collection %d"),
			Page, Usage, Pair.Value.Key.LogicalMin, Pair.Value.Key.LogicalMax, Pair.Value.Value);
	}
	return Result;
}

FString FFlyingControllerHid::GetRawValuesString() const
{
	if (!Impl->PreparsedData || Impl->ReportBuffer.Num() == 0)
	{
		return TEXT("(无数据)");
	}

	// 按键 usage 从小到大排，日志才好对照。
	TArray<uint32> Keys;
	Impl->AxisCaps.GetKeys(Keys);
	Keys.Sort();

	FString Result;
	for (uint32 Key : Keys)
	{
		const uint16 Page = static_cast<uint16>(Key >> 16);
		const uint16 Usage = static_cast<uint16>(Key & 0xFFFF);
		const TPair<FFlyingControllerAxisRange, USHORT>& Cap = Impl->AxisCaps.FindChecked(Key);

		ULONG Value = 0;
		if (HidP_GetUsageValue(HidP_Input, Page, Cap.Value, Usage, &Value, Impl->PreparsedData,
			reinterpret_cast<PCHAR>(Impl->ReportBuffer.GetData()), Impl->ReportBuffer.Num()) == HIDP_STATUS_SUCCESS)
		{
			Result += FString::Printf(TEXT("%04X=%3u "), Usage, static_cast<uint32>(Value));
		}
	}
	return Result;
}

void FFlyingControllerHid::CollectRawValues(TMap<uint16, uint32>& OutValues) const
{
	OutValues.Reset();
	if (!Impl->PreparsedData || Impl->ReportBuffer.Num() == 0)
	{
		return;
	}

	for (const TPair<uint32, TPair<FFlyingControllerAxisRange, USHORT>>& Pair : Impl->AxisCaps)
	{
		const uint16 Page = static_cast<uint16>(Pair.Key >> 16);
		const uint16 Usage = static_cast<uint16>(Pair.Key & 0xFFFF);

		ULONG Value = 0;
		if (HidP_GetUsageValue(HidP_Input, Page, Pair.Value.Value, Usage, &Value, Impl->PreparsedData,
			reinterpret_cast<PCHAR>(Impl->ReportBuffer.GetData()), Impl->ReportBuffer.Num()) == HIDP_STATUS_SUCCESS)
		{
			OutValues.Add(Usage, static_cast<uint32>(Value));
		}
	}
}

#endif // PLATFORM_WINDOWS
