// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

/** 一个摇杆圆盘：画外圈、十字轴、当前位置方块。Value 为 -1~1（X 右正、Y 上正）。 */
class SStickView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SStickView) {}
		SLATE_ATTRIBUTE(FVector2D, Value)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Value = InArgs._Value; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(220.0f, 220.0f); }

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	TAttribute<FVector2D> Value;
};

/**
 * 飞行手柄实时状态面板：两个摇杆圆盘 + 原始轴 + 四通道值 + 按键。
 *
 * 数据来自 FlyingController 模块的 FlyingControllerState::Get()。
 */
class SFlyingControllerMonitor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFlyingControllerMonitor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	EActiveTimerReturnType Refresh(double InCurrentTime, float InDeltaTime);

	FText GetStatusText() const;
	FText GetRawText() const;
	FText GetChannelText() const;
	FText GetButtonText() const;

	/** 左侧 / 右侧摇杆面板（含四方向文字）。 */
	TSharedRef<SWidget> MakeStickPanel(bool bLeftStick);
};
