#pragma once

#include "CoreMinimal.h"
#include "LargeDepthMapTypes.h"
#include "Widgets/SLeafWidget.h"

class ULargeDepthMapWidget;

class SLargeDepthMapWidget final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SLargeDepthMapWidget)
		{
		}
		SLATE_ARGUMENT(ULargeDepthMapWidget*, OwnerWidget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void ReleaseBrushesForResource(const UObject* Resource) const;

private:
	TWeakObjectPtr<ULargeDepthMapWidget> OwnerWidget;
	mutable TMap<FString, TSharedPtr<FSlateBrush>> TileBrushes;
	mutable FVector2D ViewCenter = FVector2D::ZeroVector;
	mutable float Zoom = 0.0f;
	mutable int32 StableMip = INDEX_NONE;
	bool bDragging = false;

	void EnsureInitialView(const FVector2D& LocalSize) const;
	int32 ChooseMip() const;
	FBox2f GetTileContentUv(const FLargeDepthMapTile& Tile) const;
	FSlateBrush* GetBrushForTile(const FString& Key, UObject* Resource, const FBox2f& UvRegion) const;
	bool DrawTile(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const FLargeDepthMapTile& Tile,
		const FVector2D& MipTopLeft,
		float MipZoom,
		const FString& Key,
		UObject* Resource,
		const FBox2f& UvRegion,
		const FString& DebugLabel) const;
	bool DrawStableTileGroup(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		int32 Mip,
		int32 ParentTileX,
		int32 ParentTileY,
		const FVector2D& ViewTopLeft,
		int32& VisibleTileCount,
		int32& CandidateTileCount,
		int32& ReadyTileCount,
		TSet<FString>& RetainedTileKeys) const;
	bool DrawSingleTileWithFallback(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const FLargeDepthMapTile& Tile,
		const FVector2D& ViewTopLeft,
		int32& VisibleTileCount,
		TSet<FString>& RetainedTileKeys) const;
	bool DrawReadyChildTiles(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		const TArray<const FLargeDepthMapTile*, TInlineAllocator<4>>& ChildTiles,
		const FVector2D& ViewTopLeft,
		int32& VisibleTileCount) const;
	bool DrawParentForTileGroup(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		int32 ParentMip,
		int32 ParentTileX,
		int32 ParentTileY,
		const FVector2D& ViewTopLeft,
		int32& VisibleTileCount,
		TSet<FString>& RetainedTileKeys) const;
	bool DrawChildFallbackResources(
		ULargeDepthMapWidget& Owner,
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		int32 Mip,
		int32 TileX,
		int32 TileY,
		const FVector2D& PaintPosition,
		const FVector2D& PaintSize,
		TSet<FString>& VisibleTileKeys) const;
	void DrawTileDebugBounds(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& PaintPosition,
		const FVector2D& PaintSize) const;
	void DrawTileDebugOverlay(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FLargeDepthMapTile& Tile,
		const FVector2D& PaintPosition,
		const FVector2D& PaintSize,
		const FString& DebugLabel) const;
	FLinearColor GetMipDebugColor(int32 Mip) const;
	UObject* FindFallbackResource(
		ULargeDepthMapWidget& Owner,
		const FLargeDepthMapTile& Tile,
		FString& OutKey,
		FString& OutCacheKey,
		FBox2f& OutUvRegion) const;
};
