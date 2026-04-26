#include "SLargeDepthMapWidget.h"

#include "Brushes/SlateImageBrush.h"
#include "Input/Events.h"
#include "LargeDepthMapAsset.h"
#include "LargeDepthMapWidget.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SLargeDepthMapWidget::Construct(const FArguments& InArgs)
{
	OwnerWidget = InArgs._OwnerWidget;
}

int32 SLargeDepthMapWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	ULargeDepthMapWidget* Owner = OwnerWidget.Get();
	if (!Owner || !Owner->DepthMap)
	{
		return LayerId;
	}

	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	EnsureInitialView(LocalSize);
	const int32 Mip = ChooseMip();
	const FVector2D ViewTopLeft = ViewCenter - LocalSize / (2.0f * Zoom);
	const int32 TileSize = Owner->DepthMap->TileSize;
	TSet<FString> RetainedTileKeys;
	int32 VisibleTileCount = 0;
	int32 CandidateTileCount = 0;
	int32 ReadyTileCount = 0;
	const int32 PrefetchRadius = FMath::Max(0, Owner->PrefetchTileRadius);
	Owner->RequestResidentFallbackTiles(RetainedTileKeys);

	const int32 MipScale = 1 << Mip;
	const FVector2D MipTopLeft = ViewTopLeft / MipScale;
	const FVector2D MipBottomRight = (ViewTopLeft + LocalSize / Zoom) / MipScale;
	const int32 MinTileX = FMath::FloorToInt(MipTopLeft.X / TileSize);
	const int32 MinTileY = FMath::FloorToInt(MipTopLeft.Y / TileSize);
	const int32 MaxTileX = FMath::FloorToInt(MipBottomRight.X / TileSize);
	const int32 MaxTileY = FMath::FloorToInt(MipBottomRight.Y / TileSize);
	for (int32 TileY = MinTileY; TileY <= MaxTileY; ++TileY)
	{
		for (int32 TileX = MinTileX; TileX <= MaxTileX; ++TileX)
		{
			const FLargeDepthMapTile* Tile = Owner->DepthMap->FindTile(Mip, TileX, TileY);
			if (!Tile)
			{
				continue;
			}

			const FString Key = ULargeDepthMapWidget::MakeTileKey(*Tile);
			RetainedTileKeys.Add(Key);
			Owner->GetOrLoadTileResource(*Tile, ELargeDepthMapTileRequestPriority::Visible);
			++CandidateTileCount;
			if (Owner->IsTileReady(Key))
			{
				++ReadyTileCount;
			}
		}
	}

	const bool bTargetMipReady = CandidateTileCount > 0 && CandidateTileCount == ReadyTileCount;
	const int32 DrawMip = bTargetMipReady
		? Mip
		: FMath::Min(Mip + 1, Owner->DepthMap->MipCount - 1);
	const int32 DrawMipScale = 1 << DrawMip;
	const FVector2D DrawMipTopLeft = ViewTopLeft / DrawMipScale;
	const FVector2D DrawMipBottomRight = (ViewTopLeft + LocalSize / Zoom) / DrawMipScale;
	const int32 DrawMinTileX = FMath::FloorToInt(DrawMipTopLeft.X / TileSize);
	const int32 DrawMinTileY = FMath::FloorToInt(DrawMipTopLeft.Y / TileSize);
	const int32 DrawMaxTileX = FMath::FloorToInt(DrawMipBottomRight.X / TileSize);
	const int32 DrawMaxTileY = FMath::FloorToInt(DrawMipBottomRight.Y / TileSize);
	for (int32 TileY = DrawMinTileY; TileY <= DrawMaxTileY; ++TileY)
	{
		for (int32 TileX = DrawMinTileX; TileX <= DrawMaxTileX; ++TileX)
		{
			const FLargeDepthMapTile* Tile = Owner->DepthMap->FindTile(DrawMip, TileX, TileY);
			if (!Tile)
			{
				continue;
			}

			RetainedTileKeys.Add(ULargeDepthMapWidget::MakeTileKey(*Tile));
			Owner->GetOrLoadTileResource(
				*Tile,
				bTargetMipReady
					? ELargeDepthMapTileRequestPriority::Visible
					: ELargeDepthMapTileRequestPriority::Fallback);
			DrawSingleTileWithFallback(
				*Owner,
				AllottedGeometry,
				OutDrawElements,
				LayerId,
				InWidgetStyle,
				*Tile,
				ViewTopLeft,
				VisibleTileCount,
				RetainedTileKeys);
		}
	}

	for (int32 TileY = MinTileY - PrefetchRadius; TileY <= MaxTileY + PrefetchRadius; ++TileY)
	{
		for (int32 TileX = MinTileX - PrefetchRadius; TileX <= MaxTileX + PrefetchRadius; ++TileX)
		{
			if (TileX >= MinTileX && TileX <= MaxTileX && TileY >= MinTileY && TileY <= MaxTileY)
			{
				continue;
			}

			const FLargeDepthMapTile* Tile = Owner->DepthMap->FindTile(Mip, TileX, TileY);
			if (!Tile)
			{
				continue;
			}

			const FString Key = ULargeDepthMapWidget::MakeTileKey(Mip, TileX, TileY);
			RetainedTileKeys.Add(Key);
			Owner->GetOrLoadTileResource(*Tile, ELargeDepthMapTileRequestPriority::Prefetch);
		}
	}

	Owner->ProcessPendingTileRequests();
	Owner->SetCurrentViewStats(
		Mip,
		VisibleTileCount,
		CandidateTileCount,
		ReadyTileCount,
		RetainedTileKeys);

	int32 ResultLayer = LayerId + 1;
	if (Owner->bShowStatusOverlay)
	{
		const FString StatusText = Owner->GetStatusText();
		const FVector2D StatusPosition(8.0f, 8.0f);
		const FVector2D StatusSize(
			FMath::Max(1.0f, FMath::Min(LocalSize.X - 16.0f, 760.0f)),
			78.0f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			ResultLayer,
			AllottedGeometry.ToPaintGeometry(StatusSize, FSlateLayoutTransform(StatusPosition)),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FLinearColor(0.08f, 0.08f, 0.08f, 0.58f));
		FSlateDrawElement::MakeText(
			OutDrawElements,
			ResultLayer + 1,
			AllottedGeometry.ToPaintGeometry(
				StatusSize - FVector2D(12.0f, 8.0f),
				FSlateLayoutTransform(StatusPosition + FVector2D(6.0f, 4.0f))),
			StatusText,
			FCoreStyle::GetDefaultFontStyle("Regular", 10),
			ESlateDrawEffect::None,
			FLinearColor::White);
		ResultLayer += 2;
	}

	return ResultLayer;
}

FVector2D SLargeDepthMapWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(512.0f, 512.0f);
}

FReply SLargeDepthMapWidget::OnMouseButtonDown(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	return FReply::Unhandled();
}

FReply SLargeDepthMapWidget::OnMouseButtonUp(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

FReply SLargeDepthMapWidget::OnMouseMove(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (!bDragging || Zoom <= 0.0f)
	{
		return FReply::Unhandled();
	}

	ViewCenter -= MouseEvent.GetCursorDelta() / Zoom;
	return FReply::Handled();
}

FReply SLargeDepthMapWidget::OnMouseWheel(
	const FGeometry& MyGeometry,
	const FPointerEvent& MouseEvent)
{
	const float Scale = FMath::Pow(1.2f, MouseEvent.GetWheelDelta());
	Zoom = FMath::Clamp(Zoom * Scale, 0.001f, 64.0f);
	return FReply::Handled();
}

void SLargeDepthMapWidget::ReleaseBrushesForResource(const UObject* Resource) const
{
	if (!Resource)
	{
		return;
	}

	for (auto It = TileBrushes.CreateIterator(); It; ++It)
	{
		const TSharedPtr<FSlateBrush>& Brush = It.Value();
		if (Brush.IsValid() && Brush->GetResourceObject() == Resource)
		{
			It.RemoveCurrent();
		}
	}
}

void SLargeDepthMapWidget::EnsureInitialView(const FVector2D& LocalSize) const
{
	ULargeDepthMapWidget* Owner = OwnerWidget.Get();
	if (!Owner || !Owner->DepthMap || Zoom > 0.0f)
	{
		return;
	}

	ViewCenter = FVector2D(Owner->DepthMap->Width, Owner->DepthMap->Height) * 0.5f;
	const float FitX = LocalSize.X / FMath::Max(1, Owner->DepthMap->Width);
	const float FitY = LocalSize.Y / FMath::Max(1, Owner->DepthMap->Height);
	Zoom = FMath::Max(0.001f, FMath::Min(FitX, FitY));
}

int32 SLargeDepthMapWidget::ChooseMip() const
{
	ULargeDepthMapWidget* Owner = OwnerWidget.Get();
	if (!Owner || !Owner->DepthMap || Owner->DepthMap->MipCount <= 0)
	{
		return 0;
	}

	const float SourcePixelsPerScreenPixel = Zoom >= 1.0f
		? 1.0f
		: 1.0f / FMath::Max(Zoom, 0.001f);
	const int32 WantedMip = FMath::FloorToInt(FMath::Log2(SourcePixelsPerScreenPixel));
	const int32 ClampedWantedMip = FMath::Clamp(WantedMip, 0, Owner->DepthMap->MipCount - 1);
	if (StableMip == INDEX_NONE)
	{
		StableMip = ClampedWantedMip;
	}

	StableMip = FMath::Clamp(StableMip, 0, Owner->DepthMap->MipCount - 1);
	const float RefinePixels = FMath::Max(1.0f, Owner->MipRefineScreenPixels);
	const float CoarsenPixels = FMath::Max(1.0f, Owner->MipCoarsenScreenPixels);
	while (StableMip > ClampedWantedMip
		&& Owner->DepthMap->TileSize * Zoom * float(1 << StableMip) > RefinePixels)
	{
		--StableMip;
	}
	while (StableMip < ClampedWantedMip
		&& Owner->DepthMap->TileSize * Zoom * float(1 << StableMip) < CoarsenPixels)
	{
		++StableMip;
	}

	return StableMip;
}

FSlateBrush* SLargeDepthMapWidget::GetBrushForTile(
	const FString& Key,
	UObject* Resource,
	const FBox2f& UvRegion) const
{
	if (TSharedPtr<FSlateBrush>* ExistingBrush = TileBrushes.Find(Key))
	{
		if (Resource && (*ExistingBrush)->GetResourceObject() != Resource)
		{
			(*ExistingBrush)->SetResourceObject(Resource);
		}
		(*ExistingBrush)->SetUVRegion(UvRegion);
		return ExistingBrush->Get();
	}

	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Resource);
	Brush->SetUVRegion(UvRegion);
	Brush->ImageSize = FVector2D(1.0f);
	TileBrushes.Add(Key, Brush);
	return Brush.Get();
}

bool SLargeDepthMapWidget::DrawTile(
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
	const FString& DebugLabel) const
{
	if (!Resource)
	{
		return false;
	}

	const int32 TileSize = Owner.DepthMap ? Owner.DepthMap->TileSize : 0;
	const FVector2D TileSourcePosition(Tile.TileX * TileSize, Tile.TileY * TileSize);
	const FVector2D PaintPosition = (TileSourcePosition - MipTopLeft) * MipZoom;
	const FVector2D PaintSize(Tile.Width * MipZoom, Tile.Height * MipZoom);
	const float Overlap = FMath::Max(0.0f, Owner.TileScreenOverlapPixels);
	const FVector2D DrawPosition = PaintPosition - FVector2D(Overlap);
	const FVector2D DrawSize = PaintSize + FVector2D(Overlap * 2.0f);
	FSlateBrush* Brush = GetBrushForTile(Key, Resource, UvRegion);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(DrawPosition)),
		Brush,
		ESlateDrawEffect::None,
		InWidgetStyle.GetColorAndOpacityTint());
	if (Owner.bShowTileDebugBounds)
	{
		DrawTileDebugBounds(AllottedGeometry, OutDrawElements, LayerId + 1, PaintPosition, PaintSize);
	}
	if (Owner.bShowTileMipOverlay || Owner.bShowTileDebugLabels)
	{
		DrawTileDebugOverlay(
			AllottedGeometry,
			OutDrawElements,
			LayerId + 2,
			Tile,
			PaintPosition,
			PaintSize,
			DebugLabel);
	}
	return true;
}

bool SLargeDepthMapWidget::DrawStableTileGroup(
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
	TSet<FString>& RetainedTileKeys) const
{
	if (!Owner.DepthMap)
	{
		return false;
	}

	if (Mip >= Owner.DepthMap->MipCount - 1)
	{
		const FLargeDepthMapTile* Tile = Owner.DepthMap->FindTile(Mip, ParentTileX, ParentTileY);
		if (!Tile)
		{
			return false;
		}

		const FString Key = ULargeDepthMapWidget::MakeTileKey(*Tile);
		RetainedTileKeys.Add(Key);
		++CandidateTileCount;
		if (Owner.IsTileReady(Key))
		{
			++ReadyTileCount;
		}
		Owner.GetOrLoadTileResource(*Tile, ELargeDepthMapTileRequestPriority::Visible);
		return DrawSingleTileWithFallback(
			Owner,
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			InWidgetStyle,
			*Tile,
			ViewTopLeft,
			VisibleTileCount,
			RetainedTileKeys);
	}

	TArray<const FLargeDepthMapTile*, TInlineAllocator<4>> ChildTiles;
	bool bAllChildrenReady = true;
	for (int32 ChildY = 0; ChildY < 2; ++ChildY)
	{
		for (int32 ChildX = 0; ChildX < 2; ++ChildX)
		{
			const int32 TileX = ParentTileX * 2 + ChildX;
			const int32 TileY = ParentTileY * 2 + ChildY;
			const FLargeDepthMapTile* ChildTile = Owner.DepthMap->FindTile(Mip, TileX, TileY);
			if (!ChildTile)
			{
				continue;
			}

			const FString ChildKey = ULargeDepthMapWidget::MakeTileKey(*ChildTile);
			RetainedTileKeys.Add(ChildKey);
			Owner.GetOrLoadTileResource(*ChildTile, ELargeDepthMapTileRequestPriority::Visible);
			++CandidateTileCount;
			if (Owner.IsTileReady(ChildKey))
			{
				++ReadyTileCount;
			}
			else
			{
				bAllChildrenReady = false;
			}
			ChildTiles.Add(ChildTile);
		}
	}

	if (ChildTiles.Num() > 0 && bAllChildrenReady)
	{
		return DrawReadyChildTiles(
			Owner,
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			InWidgetStyle,
			ChildTiles,
			ViewTopLeft,
			VisibleTileCount);
	}

	return DrawParentForTileGroup(
		Owner,
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		Mip + 1,
		ParentTileX,
		ParentTileY,
		ViewTopLeft,
		VisibleTileCount,
		RetainedTileKeys);
}

bool SLargeDepthMapWidget::DrawSingleTileWithFallback(
	ULargeDepthMapWidget& Owner,
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const FLargeDepthMapTile& Tile,
	const FVector2D& ViewTopLeft,
	int32& VisibleTileCount,
	TSet<FString>& RetainedTileKeys) const
{
	FBox2f UvRegion = Owner.TileMaterial
		? FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f))
		: GetTileContentUv(Tile);
	FString Key = ULargeDepthMapWidget::MakeTileKey(Tile);
	UObject* Resource = Owner.FindCachedTileResource(Key);
	if (!Resource)
	{
		FString FallbackKey;
		FString FallbackCacheKey;
		Resource = FindFallbackResource(
			Owner,
			Tile,
			FallbackKey,
			FallbackCacheKey,
			UvRegion);
		if (Resource)
		{
			Key = FallbackKey;
			RetainedTileKeys.Add(FallbackCacheKey);
		}
	}

	const int32 MipScale = 1 << Tile.Mip;
	if (!DrawTile(
		Owner,
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		Tile,
		ViewTopLeft / MipScale,
		Zoom * MipScale,
		Key,
		Resource,
		UvRegion,
		FString::Printf(TEXT("m%d %d,%d %s"), Tile.Mip, Tile.TileX, Tile.TileY, *Key)))
	{
		return false;
	}

	++VisibleTileCount;
	return true;
}

bool SLargeDepthMapWidget::DrawReadyChildTiles(
	ULargeDepthMapWidget& Owner,
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const TArray<const FLargeDepthMapTile*, TInlineAllocator<4>>& ChildTiles,
	const FVector2D& ViewTopLeft,
	int32& VisibleTileCount) const
{
	for (const FLargeDepthMapTile* ChildTile : ChildTiles)
	{
		const FString Key = ULargeDepthMapWidget::MakeTileKey(*ChildTile);
		UObject* Resource = Owner.FindCachedTileResource(Key);
		const FBox2f UvRegion = Owner.TileMaterial
			? FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f))
			: GetTileContentUv(*ChildTile);
		const int32 MipScale = 1 << ChildTile->Mip;
		if (DrawTile(
			Owner,
			AllottedGeometry,
			OutDrawElements,
			LayerId,
			InWidgetStyle,
			*ChildTile,
			ViewTopLeft / MipScale,
			Zoom * MipScale,
			Key,
			Resource,
			UvRegion,
			FString::Printf(
				TEXT("m%d %d,%d %s"),
				ChildTile->Mip,
				ChildTile->TileX,
				ChildTile->TileY,
				*Key)))
		{
			++VisibleTileCount;
		}
	}
	return true;
}

bool SLargeDepthMapWidget::DrawParentForTileGroup(
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
	TSet<FString>& RetainedTileKeys) const
{
	const FLargeDepthMapTile* ParentTile =
		Owner.DepthMap ? Owner.DepthMap->FindTile(ParentMip, ParentTileX, ParentTileY) : nullptr;
	if (!ParentTile)
	{
		return false;
	}

	const FString Key = ULargeDepthMapWidget::MakeTileKey(*ParentTile);
	RetainedTileKeys.Add(Key);
	Owner.GetOrLoadTileResource(*ParentTile, ELargeDepthMapTileRequestPriority::Fallback);
	return DrawSingleTileWithFallback(
		Owner,
		AllottedGeometry,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		*ParentTile,
		ViewTopLeft,
		VisibleTileCount,
		RetainedTileKeys);
}

FBox2f SLargeDepthMapWidget::GetTileContentUv(const FLargeDepthMapTile& Tile) const
{
	const int32 StoredWidth = Tile.StoredWidth > 0 ? Tile.StoredWidth : Tile.Width;
	const int32 StoredHeight = Tile.StoredHeight > 0 ? Tile.StoredHeight : Tile.Height;
	if (Tile.GutterPixels <= 0 || StoredWidth <= 0 || StoredHeight <= 0)
	{
		return FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f));
	}

	const float MinU = float(Tile.GutterPixels) / float(StoredWidth);
	const float MinV = float(Tile.GutterPixels) / float(StoredHeight);
	const float MaxU = float(Tile.GutterPixels + Tile.Width) / float(StoredWidth);
	const float MaxV = float(Tile.GutterPixels + Tile.Height) / float(StoredHeight);
	return FBox2f(FVector2f(MinU, MinV), FVector2f(MaxU, MaxV));
}

bool SLargeDepthMapWidget::DrawChildFallbackResources(
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
	TSet<FString>& VisibleTileKeys) const
{
	if (!Owner.DepthMap || Mip <= 0)
	{
		return false;
	}

	const int32 ChildMip = Mip - 1;
	const int32 ChildTileBaseX = TileX * 2;
	const int32 ChildTileBaseY = TileY * 2;
	struct FChildFallbackDraw
	{
		FString Key;
		UObject* Resource = nullptr;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
		FBox2f UvRegion;
	};

	TArray<FChildFallbackDraw, TInlineAllocator<4>> Draws;
	int32 ExpectedChildCount = 0;

	for (int32 ChildY = 0; ChildY < 2; ++ChildY)
	{
		for (int32 ChildX = 0; ChildX < 2; ++ChildX)
		{
			const int32 ChildTileX = ChildTileBaseX + ChildX;
			const int32 ChildTileY = ChildTileBaseY + ChildY;
			const FLargeDepthMapTile* ChildTile =
				Owner.DepthMap->FindTile(ChildMip, ChildTileX, ChildTileY);
			if (!ChildTile)
			{
				continue;
			}
			++ExpectedChildCount;

			const FString ChildKey =
				ULargeDepthMapWidget::MakeTileKey(ChildMip, ChildTileX, ChildTileY);
			UObject* ChildResource = Owner.FindCachedTileResource(ChildKey);
			if (!ChildResource)
			{
				continue;
			}

			FChildFallbackDraw& Draw = Draws.AddDefaulted_GetRef();
			Draw.Key = ChildKey;
			Draw.Resource = ChildResource;
			Draw.Position = PaintPosition
				+ FVector2D(ChildX * PaintSize.X * 0.5f, ChildY * PaintSize.Y * 0.5f);
			Draw.Size = PaintSize * 0.5f;
			Draw.UvRegion = Owner.TileMaterial
				? FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f))
				: GetTileContentUv(*ChildTile);
		}
	}

	if (ExpectedChildCount <= 0 || Draws.Num() != ExpectedChildCount)
	{
		return false;
	}

	for (const FChildFallbackDraw& Draw : Draws)
	{
		VisibleTileKeys.Add(Draw.Key);
		const float Overlap = FMath::Max(0.0f, Owner.TileScreenOverlapPixels);
		const FVector2D DrawPosition = Draw.Position - FVector2D(Overlap);
		const FVector2D DrawSize = Draw.Size + FVector2D(Overlap * 2.0f);
		FSlateBrush* Brush = GetBrushForTile(Draw.Key, Draw.Resource, Draw.UvRegion);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(DrawPosition)),
			Brush,
			ESlateDrawEffect::None,
			InWidgetStyle.GetColorAndOpacityTint());
	}

	return true;
}

void SLargeDepthMapWidget::DrawTileDebugBounds(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FVector2D& PaintPosition,
	const FVector2D& PaintSize) const
{
	TArray<FVector2f> Points;
	Points.Reserve(5);
	Points.Add(FVector2f(PaintPosition.X, PaintPosition.Y));
	Points.Add(FVector2f(PaintPosition.X + PaintSize.X, PaintPosition.Y));
	Points.Add(FVector2f(PaintPosition.X + PaintSize.X, PaintPosition.Y + PaintSize.Y));
	Points.Add(FVector2f(PaintPosition.X, PaintPosition.Y + PaintSize.Y));
	Points.Add(FVector2f(PaintPosition.X, PaintPosition.Y));
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		FLinearColor::Green,
		false,
		1.0f);
}

void SLargeDepthMapWidget::DrawTileDebugOverlay(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FLargeDepthMapTile& Tile,
	const FVector2D& PaintPosition,
	const FVector2D& PaintSize,
	const FString& DebugLabel) const
{
	ULargeDepthMapWidget* Owner = OwnerWidget.Get();
	if (!Owner)
	{
		return;
	}

	if (Owner->bShowTileMipOverlay)
	{
		FLinearColor MipColor = GetMipDebugColor(Tile.Mip);
		MipColor.A = FMath::Clamp(Owner->TileMipOverlayAlpha, 0.0f, 1.0f);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(PaintSize, FSlateLayoutTransform(PaintPosition)),
			FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			MipColor);
	}

	if (Owner->bShowTileDebugLabels)
	{
		FSlateDrawElement::MakeText(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(
				FVector2D(FMath::Max(180.0f, PaintSize.X), 20.0f),
				FSlateLayoutTransform(PaintPosition + FVector2D(4.0f, 4.0f))),
			DebugLabel,
			FCoreStyle::GetDefaultFontStyle("Regular", 9),
			ESlateDrawEffect::None,
			FLinearColor::White);
	}
}

FLinearColor SLargeDepthMapWidget::GetMipDebugColor(int32 Mip) const
{
	static const FLinearColor Colors[] =
	{
		FLinearColor(1.0f, 0.05f, 0.05f, 1.0f),
		FLinearColor(0.05f, 0.8f, 1.0f, 1.0f),
		FLinearColor(0.0f, 1.0f, 0.2f, 1.0f),
		FLinearColor(1.0f, 0.95f, 0.0f, 1.0f),
		FLinearColor(1.0f, 0.0f, 1.0f, 1.0f),
		FLinearColor(0.1f, 0.2f, 1.0f, 1.0f),
		FLinearColor(1.0f, 0.45f, 0.0f, 1.0f),
		FLinearColor(0.0f, 1.0f, 0.85f, 1.0f)
	};
	return Colors[FMath::Abs(Mip) % UE_ARRAY_COUNT(Colors)];
}

UObject* SLargeDepthMapWidget::FindFallbackResource(
	ULargeDepthMapWidget& Owner,
	const FLargeDepthMapTile& Tile,
	FString& OutKey,
	FString& OutCacheKey,
	FBox2f& OutUvRegion) const
{
	OutKey = FString::Printf(TEXT("missing/%d/%d/%d"), Tile.Mip, Tile.TileX, Tile.TileY);
	OutCacheKey.Empty();
	if (!Owner.DepthMap)
	{
		return nullptr;
	}

	for (int32 ParentMip = Tile.Mip + 1; ParentMip < Owner.DepthMap->MipCount; ++ParentMip)
	{
		const int32 Shift = ParentMip - Tile.Mip;
		const int32 ParentTileX = Tile.TileX >> Shift;
		const int32 ParentTileY = Tile.TileY >> Shift;
		const FLargeDepthMapTile* ParentTile =
			Owner.DepthMap->FindTile(ParentMip, ParentTileX, ParentTileY);
		if (!ParentTile)
		{
			continue;
		}

		const FString ParentKey =
			ULargeDepthMapWidget::MakeTileKey(ParentMip, ParentTileX, ParentTileY);
		UObject* ParentResource =
			Owner.GetOrLoadTileResource(*ParentTile, ELargeDepthMapTileRequestPriority::Fallback);
		if (!ParentResource)
		{
			continue;
		}

		const FBox2f ParentContentUv = Owner.TileMaterial
			? FBox2f(FVector2f(0.0f, 0.0f), FVector2f(1.0f, 1.0f))
			: GetTileContentUv(*ParentTile);
		const FVector2f ParentUvSize = ParentContentUv.Max - ParentContentUv.Min;
		const float ParentScale = float(1 << Shift);
		const FVector2f ParentOrigin(
			float(ParentTile->TileX * Owner.DepthMap->TileSize),
			float(ParentTile->TileY * Owner.DepthMap->TileSize));
		const FVector2f ChildMinInParentMip(
			float(Tile.TileX * Owner.DepthMap->TileSize) / ParentScale,
			float(Tile.TileY * Owner.DepthMap->TileSize) / ParentScale);
		const FVector2f ChildMaxInParentMip(
			float(Tile.TileX * Owner.DepthMap->TileSize + Tile.Width) / ParentScale,
			float(Tile.TileY * Owner.DepthMap->TileSize + Tile.Height) / ParentScale);
		const FVector2f ParentSize(
			FMath::Max(1.0f, float(ParentTile->Width)),
			FMath::Max(1.0f, float(ParentTile->Height)));
		const FVector2f LocalMin(
			FMath::Clamp((ChildMinInParentMip.X - ParentOrigin.X) / ParentSize.X, 0.0f, 1.0f),
			FMath::Clamp((ChildMinInParentMip.Y - ParentOrigin.Y) / ParentSize.Y, 0.0f, 1.0f));
		const FVector2f LocalMax(
			FMath::Clamp((ChildMaxInParentMip.X - ParentOrigin.X) / ParentSize.X, 0.0f, 1.0f),
			FMath::Clamp((ChildMaxInParentMip.Y - ParentOrigin.Y) / ParentSize.Y, 0.0f, 1.0f));
		OutKey = FString::Printf(
			TEXT("fallback/%s/%d/%d/%d"),
			*ParentKey,
			Tile.Mip,
			Tile.TileX,
			Tile.TileY);
		OutCacheKey = ParentKey;
		OutUvRegion = FBox2f(
			ParentContentUv.Min + LocalMin * ParentUvSize,
			ParentContentUv.Min + LocalMax * ParentUvSize);
		return ParentResource;
	}

	return nullptr;
}
