// Copyright Epic Games, Inc. All Rights Reserved.

#include "SFlyingControllerMonitor.h"

#include "FlyingControllerState.h"

#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FlyingControllerMonitor"

namespace
{
	/** 参考「RC 手柄学习」界面的配色。 */
	const FLinearColor CircleColor(0.55f, 0.55f, 0.55f, 1.0f);
	const FLinearColor CrossColor(0.72f, 0.72f, 0.72f, 1.0f);
	const FLinearColor DotColor(0.12f, 0.55f, 1.0f, 1.0f);

	/** 轴 usage 定义，和 FlyingControllerSettings 的默认值保持一致。 */
	constexpr uint16 UsageX = 0x30;
	constexpr uint16 UsageY = 0x31;
	constexpr uint16 UsageZ = 0x32;
	constexpr uint16 UsageRx = 0x33;
	constexpr uint16 UsageRy = 0x34;
	constexpr uint16 UsageS0 = 0x41;

	float RawToUnit(const TMap<uint16, uint32>& RawAxes, uint16 Usage)
	{
		if (const uint32* Value = RawAxes.Find(Usage))
		{
			return static_cast<float>(*Value) / 255.0f;
		}
		return 0.0f;
	}

	FText MakeLabel(const FText& Text)
	{
		return Text;
	}

	TSharedRef<STextBlock> DirectionLabel(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Justification(ETextJustify::Center)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	}
}

int32 SStickView::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = LocalSize * 0.5;
	const float Radius = FMath::Min(static_cast<float>(LocalSize.X), static_cast<float>(LocalSize.Y)) * 0.5f - 6.0f;

	if (Radius > 4.0f)
	{
		// 外圈
		TArray<FVector2d> Circle;
		const int32 Segments = 64;
		Circle.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const float Angle = (2.0f * PI * Index) / Segments;
			Circle.Add(FVector2d(Center.X + Radius * FMath::Cos(Angle), Center.Y + Radius * FMath::Sin(Angle)));
		}

		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Circle,
			ESlateDrawEffect::None, CircleColor, true, 1.5f);

		// 十字轴
		TArray<FVector2d> Horizontal;
		Horizontal.Add(FVector2d(Center.X - Radius, Center.Y));
		Horizontal.Add(FVector2d(Center.X + Radius, Center.Y));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Horizontal,
			ESlateDrawEffect::None, CrossColor, true, 1.0f);

		TArray<FVector2d> Vertical;
		Vertical.Add(FVector2d(Center.X, Center.Y - Radius));
		Vertical.Add(FVector2d(Center.X, Center.Y + Radius));
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Vertical,
			ESlateDrawEffect::None, CrossColor, true, 1.0f);

		// 当前位置（方块，和参考图一致）
		const FVector2D StickValue = this->Value.Get();
		const FVector2D DotCenter = Center + FVector2D(StickValue.X, -StickValue.Y) * Radius;
		const float DotSize = 12.0f;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2D(DotSize, DotSize),
				FSlateLayoutTransform(FVector2D(DotCenter.X - DotSize * 0.5, DotCenter.Y - DotSize * 0.5))),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			DotColor);
	}

	return LayerId + 1;
}

void SFlyingControllerMonitor::Construct(const FArguments& InArgs)
{
	UE_LOG(LogTemp, Log, TEXT("FlyingController: 构建摇杆面板控件"));

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetStatusText(); })
				.AutoWrapText(true)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 24.0f, 0.0f)
				[
					MakeStickPanel(/*bLeftStick=*/true)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					MakeStickPanel(/*bLeftStick=*/false)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetRawText(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetChannelText(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return GetButtonText(); })
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
			]
		]
	];

	// 每帧刷新，摇杆位置才会跟手。
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SFlyingControllerMonitor::Refresh));
}

EActiveTimerReturnType SFlyingControllerMonitor::Refresh(double InCurrentTime, float InDeltaTime)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

TSharedRef<SWidget> SFlyingControllerMonitor::MakeStickPanel(bool bLeftStick)
{
	TSharedRef<SStickView> Stick = SNew(SStickView)
		.Value_Lambda([bLeftStick]()
		{
			const FLyingControllerInputState& State = FlyingControllerState::Get();
			if (bLeftStick)
			{
				// 左摇杆：X = 偏航，Y = 油门（0~1 映射到 -1~1，向上为正）
				return FVector2D(State.Yaw, State.Throttle * 2.0f - 1.0f);
			}
			// 右摇杆：X = 横滚，Y = 俯仰
			return FVector2D(State.Roll, State.Pitch);
		});

	const TSharedRef<SWidget> StickBox = SNew(SBox)
		.WidthOverride(200.0f)
		.HeightOverride(200.0f)
		[
			Stick
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 4.0f)
		[
			DirectionLabel(bLeftStick ? LOCTEXT("LeftTitle", "左") : LOCTEXT("RightTitle", "右"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			DirectionLabel(bLeftStick ? LOCTEXT("Up", "上升") : LOCTEXT("Forward", "水平前"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				DirectionLabel(bLeftStick ? LOCTEXT("YawLeft", "左转（原地转）") : LOCTEXT("Left", "水平左"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				StickBox
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				DirectionLabel(bLeftStick ? LOCTEXT("YawRight", "右转（原地转）") : LOCTEXT("Right", "水平右"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			DirectionLabel(bLeftStick ? LOCTEXT("Down", "下降") : LOCTEXT("Backward", "水平后"))
		];
}

FText SFlyingControllerMonitor::GetStatusText() const
{
	const FLyingControllerInputState& State = FlyingControllerState::Get();
	if (!State.bConnected)
	{
		return LOCTEXT("Disconnected", "设备：未连接（插上手柄后自动重连）");
	}

	return FText::FromString(FString::Printf(TEXT("设备：已连接  %s  (%04X:%04X)"),
		*State.Device.ProductName, State.Device.VendorId, State.Device.ProductId));
}

FText SFlyingControllerMonitor::GetRawText() const
{
	const FLyingControllerInputState& State = FlyingControllerState::Get();
	if (!State.bConnected)
	{
		return LOCTEXT("NoRaw", "原始轴  —");
	}

	return FText::FromString(FString::Printf(
		TEXT("原始轴  X %.2f  Y %.2f  Z %.2f  Rx %.2f  Ry %.2f  S0 %.2f"),
		RawToUnit(State.RawAxes, UsageX),
		RawToUnit(State.RawAxes, UsageY),
		RawToUnit(State.RawAxes, UsageZ),
		RawToUnit(State.RawAxes, UsageRx),
		RawToUnit(State.RawAxes, UsageRy),
		RawToUnit(State.RawAxes, UsageS0)));
}

FText SFlyingControllerMonitor::GetChannelText() const
{
	const FLyingControllerInputState& State = FlyingControllerState::Get();

	// 连接时四通道都已标定可用，用 √ 表示（和参考图一致）。
	const TCHAR* Mark = State.bConnected ? TEXT("√") : TEXT("—");

	return FText::FromString(FString::Printf(
		TEXT("通道  THR %+.2f %s    YAW %+.2f %s    PIT %+.2f %s    ROL %+.2f %s"),
		State.Throttle, Mark,
		State.Yaw, Mark,
		State.Pitch, Mark,
		State.Roll, Mark));
}

FText SFlyingControllerMonitor::GetButtonText() const
{
	const FLyingControllerInputState& State = FlyingControllerState::Get();
	if (!State.bConnected)
	{
		return LOCTEXT("NoButton", "按键  —");
	}

	if (State.PressedButtons.Num() == 0)
	{
		return LOCTEXT("NoButtonPressed", "按键  （无）");
	}

	FString Buttons;
	for (uint16 Usage : State.PressedButtons)
	{
		Buttons += FString::Printf(TEXT("%u "), Usage);
	}

	return FText::FromString(FString::Printf(TEXT("按键  %s"), *Buttons.TrimEnd()));
}

#undef LOCTEXT_NAMESPACE
