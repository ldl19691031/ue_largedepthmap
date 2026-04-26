#include "LargeDepthMapWidget.h"

#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SLargeDepthMapWidget.h"

TSharedRef<SWidget> ULargeDepthMapWidget::RebuildWidget()
{
	MyDepthMapWidget = SNew(SLargeDepthMapWidget).OwnerWidget(this);
	return MyDepthMapWidget.ToSharedRef();
}

void ULargeDepthMapWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDepthMapWidget.Reset();
	TileResourceCache.Empty();
	TileGpuBytes.Empty();
	TileLastRetainedSeconds.Empty();
	ResidentTileKeys.Empty();
	PooledTextures.Empty();
	ReleasedTextures.Empty();
	MaterialRawTextures.Empty();
	AppliedTextureFilters.Empty();
	LoadingTileKeys.Empty();
	MissingTileKeys.Empty();
	PendingTileRequests.Empty();
	CurrentGpuBytes = 0;
	MaxGpuBytes = 0;
	AverageGpuBytes = 0.0;
	GpuSampleCount = 0;
}

UObject* ULargeDepthMapWidget::GetOrLoadTileResource(
	const FLargeDepthMapTile& Tile,
	ELargeDepthMapTileRequestPriority Priority)
{
	if (!DepthMap)
	{
		return nullptr;
	}

	++TotalTileRequests;
	const FString Key = MakeTileKey(Tile);
	if (TObjectPtr<UObject>* CachedResource = TileResourceCache.Find(Key))
	{
		FindCachedTileResource(Key);
		++TileCacheHits;
		return CachedResource->Get();
	}

	if (LoadingTileKeys.Contains(Key) || MissingTileKeys.Contains(Key))
	{
		return nullptr;
	}

	if (FPendingTileRequest* PendingRequest = PendingTileRequests.Find(Key))
	{
		if (uint8(Priority) < uint8(PendingRequest->Priority))
		{
			PendingRequest->Priority = Priority;
		}
		return nullptr;
	}

	FPendingTileRequest& PendingRequest = PendingTileRequests.Add(Key);
	PendingRequest.Tile = Tile;
	PendingRequest.Priority = Priority;
	PendingRequest.Sequence = ++TileRequestSequence;
	return nullptr;
}

void ULargeDepthMapWidget::StartTileLoad(const FString& Key, const FLargeDepthMapTile& Tile)
{
	LoadingTileKeys.Add(Key);
	++TileLoadStarts;

	TWeakObjectPtr<ULargeDepthMapWidget> WeakThis(this);
	TWeakObjectPtr<ULargeDepthMapAsset> WeakAsset(DepthMap);
	const FLargeDepthMapTile TileCopy = Tile;
	Async(EAsyncExecution::ThreadPool, [WeakThis, WeakAsset, Key, TileCopy]()
	{
		TArray64<uint8> TileBytes;
		const bool bLoaded = WeakAsset.IsValid() && WeakAsset->ReadTileBytes(TileCopy, TileBytes);

		AsyncTask(ENamedThreads::GameThread, [WeakThis, Key, TileCopy, bLoaded, TileBytes = MoveTemp(TileBytes)]() mutable
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			if (!bLoaded)
			{
				WeakThis->LoadingTileKeys.Remove(Key);
				WeakThis->MissingTileKeys.Add(Key);
				return;
			}

			WeakThis->FinishTileLoad(Key, TileCopy, MoveTemp(TileBytes));
		});
	});

}

UObject* ULargeDepthMapWidget::FindCachedTileResource(const FString& Key)
{
	if (const TObjectPtr<UObject>* CachedResource = TileResourceCache.Find(Key))
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(CachedResource->Get()))
		{
			ApplyTextureSampling(Texture);
		}
		else if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(CachedResource->Get()))
		{
			if (ApplyTextureSampling(MaterialRawTextures.FindRef(Material)))
			{
				Material->RecacheUniformExpressions(false);
			}
		}
		return CachedResource->Get();
	}

	return nullptr;
}

bool ULargeDepthMapWidget::IsTileReady(const FString& Key) const
{
	return TileResourceCache.Contains(Key);
}

bool ULargeDepthMapWidget::IsTileLoading(const FString& Key) const
{
	return LoadingTileKeys.Contains(Key);
}

bool ULargeDepthMapWidget::IsTileMissing(const FString& Key) const
{
	return MissingTileKeys.Contains(Key);
}

FString ULargeDepthMapWidget::MakeTileKey(const FLargeDepthMapTile& Tile)
{
	return MakeTileKey(Tile.Mip, Tile.TileX, Tile.TileY);
}

FString ULargeDepthMapWidget::MakeTileKey(int32 Mip, int32 TileX, int32 TileY)
{
	return FString::Printf(TEXT("%d/%d/%d"), Mip, TileX, TileY);
}

FString ULargeDepthMapWidget::GetStatusText() const
{
	const int64 PackedBytes = DepthMap ? DepthMap->GetPackedTileDataSize() : 0;
	return FString::Printf(
		TEXT("LargeDepthMap  mip:%d  visible:%d  candidate:%d  ready:%d\n")
		TEXT("tiles  retained:%d  resident:%d  active:%d  free:%d  pending:%d  loading:%d  missing:%d\n")
		TEXT("gpu MiB  current:%.2f  max:%.2f  avg:%.2f  packed:%.2f\n")
		TEXT("requests:%d  hits:%d  textures created:%d  reused:%d"),
		CurrentMip,
		CurrentVisibleTiles,
		CurrentCandidateTiles,
		CurrentReadyTiles,
		CurrentRetainedTiles,
		CurrentResidentTiles,
		TileResourceCache.Num(),
		PooledTextures.Num(),
		CurrentPendingTiles,
		LoadingTileKeys.Num(),
		MissingTileKeys.Num(),
		double(CurrentGpuBytes) / (1024.0 * 1024.0),
		double(MaxGpuBytes) / (1024.0 * 1024.0),
		AverageGpuBytes / (1024.0 * 1024.0),
		double(PackedBytes) / (1024.0 * 1024.0),
		TotalTileRequests,
		TileCacheHits,
		TextureCreateCount,
		TextureReuseCount);
}

void ULargeDepthMapWidget::SetCurrentViewStats(
	int32 Mip,
	int32 VisibleTileCount,
	int32 CandidateTileCount,
	int32 ReadyTileCount,
	const TSet<FString>& RetainedTileKeys)
{
	CurrentMip = Mip;
	CurrentVisibleTiles = VisibleTileCount;
	CurrentCandidateTiles = CandidateTileCount;
	CurrentReadyTiles = ReadyTileCount;
	CurrentRetainedTiles = RetainedTileKeys.Num();
	CurrentPendingTiles = PendingTileRequests.Num();

	const double NowSeconds = FPlatformTime::Seconds();
	for (const FString& Key : RetainedTileKeys)
	{
		TileLastRetainedSeconds.Add(Key, NowSeconds);
	}
	for (const FString& Key : ResidentTileKeys)
	{
		TileLastRetainedSeconds.Add(Key, NowSeconds);
	}

	if (bEvictTilesOutsideView)
	{
		for (auto It = TileResourceCache.CreateIterator(); It; ++It)
		{
			const double* LastRetainedSeconds = TileLastRetainedSeconds.Find(It.Key());
			const double AgeSeconds = LastRetainedSeconds
				? NowSeconds - *LastRetainedSeconds
				: TileReleaseDelaySeconds;
			if (!RetainedTileKeys.Contains(It.Key()) && AgeSeconds >= TileReleaseDelaySeconds)
			{
				ReleaseTileResource(It.Key(), It.Value());
				It.RemoveCurrent();
			}
		}
	}

	CurrentGpuBytes = 0;
	for (const TPair<FString, int64>& Pair : TileGpuBytes)
	{
		CurrentGpuBytes += Pair.Value;
	}
	MaxGpuBytes = FMath::Max(MaxGpuBytes, CurrentGpuBytes);
	++GpuSampleCount;
	AverageGpuBytes += (double(CurrentGpuBytes) - AverageGpuBytes) / double(GpuSampleCount);
}

void ULargeDepthMapWidget::RequestResidentFallbackTiles(TSet<FString>& RetainedTileKeys)
{
	ResidentTileKeys.Empty();
	if (!bEnableResidentFallbackMip || !DepthMap || DepthMap->MipCount <= 0)
	{
		CurrentResidentTiles = 0;
		return;
	}

	const int32 TargetMip = FMath::Clamp(ResidentFallbackMip, 0, DepthMap->MipCount - 1);
	for (const FLargeDepthMapTile& Tile : DepthMap->Tiles)
	{
		if (Tile.Mip != TargetMip)
		{
			continue;
		}

		const FString Key = MakeTileKey(Tile);
		ResidentTileKeys.Add(Key);
		RetainedTileKeys.Add(Key);
		GetOrLoadTileResource(Tile, ELargeDepthMapTileRequestPriority::Resident);
	}

	CurrentResidentTiles = ResidentTileKeys.Num();
}

void ULargeDepthMapWidget::ProcessPendingTileRequests()
{
	const int32 MaxStarts = FMath::Max(0, MaxTileLoadStartsPerFrame);
	if (MaxStarts <= 0 || PendingTileRequests.IsEmpty())
	{
		CurrentPendingTiles = PendingTileRequests.Num();
		return;
	}

	TArray<TPair<FString, FPendingTileRequest>> Requests;
	Requests.Reserve(PendingTileRequests.Num());
	for (const TPair<FString, FPendingTileRequest>& Pair : PendingTileRequests)
	{
		Requests.Add(Pair);
	}

	Requests.Sort([](
		const TPair<FString, FPendingTileRequest>& A,
		const TPair<FString, FPendingTileRequest>& B)
	{
		const uint8 PriorityA = uint8(A.Value.Priority);
		const uint8 PriorityB = uint8(B.Value.Priority);
		return PriorityA == PriorityB
			? A.Value.Sequence < B.Value.Sequence
			: PriorityA < PriorityB;
	});

	int32 StartedCount = 0;
	for (const TPair<FString, FPendingTileRequest>& Pair : Requests)
	{
		if (StartedCount >= MaxStarts)
		{
			break;
		}
		if (LoadingTileKeys.Contains(Pair.Key) || MissingTileKeys.Contains(Pair.Key))
		{
			PendingTileRequests.Remove(Pair.Key);
			continue;
		}
		PendingTileRequests.Remove(Pair.Key);
		StartTileLoad(Pair.Key, Pair.Value.Tile);
		++StartedCount;
	}

	CurrentPendingTiles = PendingTileRequests.Num();
}

UObject* ULargeDepthMapWidget::CreateTileResource(
	const FLargeDepthMapTile& Tile,
	const TArray64<uint8>& Bytes)
{
	if (!TileMaterial)
	{
		return CreateVisualTileTexture(Tile, Bytes);
	}

	UTexture2D* RawTexture = CreateRawTileTexture(Tile, Bytes);
	if (!RawTexture)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(TileMaterial, this);
	if (!Material)
	{
		return nullptr;
	}

	Material->SetTextureParameterValue(TileTextureParameterName, RawTexture);
	Material->SetScalarParameterValue(TEXT("DepthRangeMin"), DepthRangeMin);
	Material->SetScalarParameterValue(TEXT("DepthRangeMax"), DepthRangeMax);
	MaterialRawTextures.Add(Material, RawTexture);
	return Material;
}

UTexture2D* ULargeDepthMapWidget::CreateRawTileTexture(
	const FLargeDepthMapTile& Tile,
	const TArray64<uint8>& Bytes)
{
	const int32 BytesPerPixel = DepthMap ? DepthMap->GetBytesPerPixel() : 0;
	const int32 StoredWidth = Tile.StoredWidth > 0 ? Tile.StoredWidth : Tile.Width;
	const int32 StoredHeight = Tile.StoredHeight > 0 ? Tile.StoredHeight : Tile.Height;
	const int64 ExpectedSize = int64(StoredWidth) * int64(StoredHeight) * BytesPerPixel;
	if (ExpectedSize <= 0
		|| Bytes.Num() < ExpectedSize
		|| Tile.Width <= 0
		|| Tile.Height <= 0
		|| Tile.GutterPixels < 0
		|| Tile.GutterPixels + Tile.Width > StoredWidth
		|| Tile.GutterPixels + Tile.Height > StoredHeight)
	{
		return nullptr;
	}

	const EPixelFormat PixelFormat =
		DepthMap->PixelFormat == ELargeDepthMapPixelFormat::Gray8 ? PF_G8 : PF_G16;
	UTexture2D* Texture = AcquireTexture(Tile.Width, Tile.Height, PixelFormat);
	if (!Texture || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	Texture->NeverStream = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	uint8* TextureData = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	const int64 DestinationStride = int64(Tile.Width) * BytesPerPixel;
	const int64 SourceStride = int64(StoredWidth) * BytesPerPixel;
	const uint8* SourceBase = Bytes.GetData()
		+ (int64(Tile.GutterPixels) * StoredWidth + Tile.GutterPixels) * BytesPerPixel;
	for (int32 Row = 0; Row < Tile.Height; ++Row)
	{
		FMemory::Memcpy(
			TextureData + int64(Row) * DestinationStride,
			SourceBase + int64(Row) * SourceStride,
			DestinationStride);
	}
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UTexture2D* ULargeDepthMapWidget::CreateVisualTileTexture(
	const FLargeDepthMapTile& Tile,
	const TArray64<uint8>& Bytes)
{
	const int32 BytesPerPixel = DepthMap ? DepthMap->GetBytesPerPixel() : 0;
	const int32 StoredWidth = Tile.StoredWidth > 0 ? Tile.StoredWidth : Tile.Width;
	const int32 StoredHeight = Tile.StoredHeight > 0 ? Tile.StoredHeight : Tile.Height;
	const int32 PixelCount = StoredWidth * StoredHeight;
	if (PixelCount <= 0 || Bytes.Num() < PixelCount * BytesPerPixel)
	{
		return nullptr;
	}

	UTexture2D* Texture = AcquireTexture(StoredWidth, StoredHeight, PF_B8G8R8A8);
	if (!Texture || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		return nullptr;
	}

	TArray<FColor> VisualPixels;
	VisualPixels.SetNumUninitialized(PixelCount);
	const float Range = FMath::Max(1.0f, DepthRangeMax - DepthRangeMin);

	for (int64 Index = 0; Index < PixelCount; ++Index)
	{
		const float RawValue = DepthMap->PixelFormat == ELargeDepthMapPixelFormat::Gray8
			? float(Bytes[Index])
			: float(uint16(Bytes[Index * 2]) | (uint16(Bytes[Index * 2 + 1]) << 8));
		const float Alpha = FMath::Clamp((RawValue - DepthRangeMin) / Range, 0.0f, 1.0f);
		VisualPixels[Index] = FLinearColor::LerpUsingHSV(
			LowDepthColor,
			HighDepthColor,
			Alpha).ToFColor(false);
	}

	Texture->NeverStream = true;
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(TextureData, VisualPixels.GetData(), VisualPixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

void ULargeDepthMapWidget::FinishTileLoad(
	const FString& Key,
	const FLargeDepthMapTile& Tile,
	TArray64<uint8>&& Bytes)
{
	LoadingTileKeys.Remove(Key);
	UObject* Resource = CreateTileResource(Tile, Bytes);
	if (!Resource)
	{
		MissingTileKeys.Add(Key);
		return;
	}

	TileResourceCache.Add(Key, Resource);
	TileGpuBytes.Add(Key, EstimateTileGpuBytes(Tile));
	++TileLoadCompletes;
}

UTexture2D* ULargeDepthMapWidget::AcquireTexture(
	int32 Width,
	int32 Height,
	EPixelFormat PixelFormat)
{
	PromoteReleasedTextures();
	for (int32 Index = 0; Index < PooledTextures.Num(); ++Index)
	{
		UTexture2D* Texture = PooledTextures[Index];
		if (!Texture || !Texture->GetPlatformData())
		{
			continue;
		}

		if (Texture->GetSizeX() == Width
			&& Texture->GetSizeY() == Height
			&& Texture->GetPixelFormat() == PixelFormat)
		{
			PooledTextures.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			Texture->NeverStream = true;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			ApplyTextureSampling(Texture);
			++TextureReuseCount;
			return Texture;
		}
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PixelFormat);
	if (Texture)
	{
		Texture->NeverStream = true;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		ApplyTextureSampling(Texture);
		++TextureCreateCount;
	}

	return Texture;
}

bool ULargeDepthMapWidget::ApplyTextureSampling(UTexture2D* Texture)
{
	if (!Texture)
	{
		return false;
	}

	const TextureFilter DesiredFilter = GetTextureFilter();
	const TObjectKey<UTexture2D> TextureKey(Texture);
	const TEnumAsByte<TextureFilter>* AppliedFilter = AppliedTextureFilters.Find(TextureKey);
	if (AppliedFilter && AppliedFilter->GetValue() == DesiredFilter && Texture->Filter == DesiredFilter)
	{
		return false;
	}

	Texture->Filter = DesiredFilter;
	Texture->RefreshSamplerStates();
	AppliedTextureFilters.Add(TextureKey, DesiredFilter);
	return true;
}

TextureFilter ULargeDepthMapWidget::GetTextureFilter() const
{
	return SamplingFilter == ELargeDepthMapSamplingFilter::Point
		? TF_Nearest
		: TF_Bilinear;
}

void ULargeDepthMapWidget::PromoteReleasedTextures()
{
	const double NowSeconds = FPlatformTime::Seconds();
	const double ReuseDelaySeconds = FMath::Max(0.0f, TexturePoolReuseDelaySeconds);
	for (int32 Index = ReleasedTextures.Num() - 1; Index >= 0; --Index)
	{
		FLargeDepthMapReleasedTexture& ReleasedTexture = ReleasedTextures[Index];
		if (!ReleasedTexture.Texture
			|| NowSeconds - ReleasedTexture.ReleaseSeconds >= ReuseDelaySeconds)
		{
			if (ReleasedTexture.Texture && PooledTextures.Num() < FMath::Max(0, MaxPooledTextures))
			{
				PooledTextures.Add(ReleasedTexture.Texture);
			}
			ReleasedTextures.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}
}

void ULargeDepthMapWidget::ReleaseTileResource(const FString& Key, UObject* Resource)
{
	TileGpuBytes.Remove(Key);
	TileLastRetainedSeconds.Remove(Key);
	if (MyDepthMapWidget.IsValid())
	{
		MyDepthMapWidget->ReleaseBrushesForResource(Resource);
	}
	UTexture2D* Texture = Cast<UTexture2D>(Resource);
	if (!Texture)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Resource))
		{
			if (TObjectPtr<UTexture2D>* RawTexture = MaterialRawTextures.Find(Material))
			{
				Texture = RawTexture->Get();
				MaterialRawTextures.Remove(Material);
			}
		}
	}

	if (Texture && PooledTextures.Num() < FMath::Max(0, MaxPooledTextures))
	{
		FLargeDepthMapReleasedTexture& ReleasedTexture = ReleasedTextures.AddDefaulted_GetRef();
		ReleasedTexture.Texture = Texture;
		ReleasedTexture.ReleaseSeconds = FPlatformTime::Seconds();
	}
}

int64 ULargeDepthMapWidget::EstimateTileGpuBytes(const FLargeDepthMapTile& Tile) const
{
	const int32 BytesPerPixel = TileMaterial ? DepthMap->GetBytesPerPixel() : 4;
	const int32 StoredWidth = Tile.StoredWidth > 0 ? Tile.StoredWidth : Tile.Width;
	const int32 StoredHeight = Tile.StoredHeight > 0 ? Tile.StoredHeight : Tile.Height;
	const int32 TextureWidth = TileMaterial ? Tile.Width : StoredWidth;
	const int32 TextureHeight = TileMaterial ? Tile.Height : StoredHeight;
	return int64(TextureWidth) * int64(TextureHeight) * BytesPerPixel;
}
