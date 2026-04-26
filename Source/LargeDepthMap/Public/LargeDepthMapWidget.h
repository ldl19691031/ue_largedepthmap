#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "LargeDepthMapAsset.h"
#include "Materials/MaterialInterface.h"
#include "LargeDepthMapWidget.generated.h"

class SLargeDepthMapWidget;

enum class ELargeDepthMapTileRequestPriority : uint8
{
	Visible = 0,
	Fallback = 1,
	Resident = 2,
	Prefetch = 3
};

UENUM(BlueprintType)
enum class ELargeDepthMapSamplingFilter : uint8
{
	Point,
	Bilinear
};

USTRUCT()
struct FLargeDepthMapReleasedTexture
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Texture;

	double ReleaseSeconds = 0.0;
};

UCLASS(meta = (DisplayName = "Large Depth Map View"))
class LARGEDEPTHMAP_API ULargeDepthMapWidget : public UWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	TObjectPtr<ULargeDepthMapAsset> DepthMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	FLinearColor MissingTileTint = FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	int32 MaxCachedTiles = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	int32 MaxPooledTextures = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	float TexturePoolReuseDelaySeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	bool bEvictTilesOutsideView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	int32 PrefetchTileRadius = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	int32 MaxTileLoadStartsPerFrame = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	float TileReleaseDelaySeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	float TileScreenOverlapPixels = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	ELargeDepthMapSamplingFilter SamplingFilter = ELargeDepthMapSamplingFilter::Bilinear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Mip Selection")
	float MipRefineScreenPixels = 384.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Mip Selection")
	float MipCoarsenScreenPixels = 192.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	bool bEnableResidentFallbackMip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	int32 ResidentFallbackMip = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map")
	bool bShowStatusOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Debug")
	bool bShowTileDebugBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Debug")
	bool bShowTileMipOverlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Debug")
	bool bShowTileDebugLabels = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Debug")
	float TileMipOverlayAlpha = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Display")
	FLinearColor LowDepthColor = FLinearColor(0.05f, 0.05f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Display")
	FLinearColor HighDepthColor = FLinearColor(1.0f, 0.18f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Display")
	float DepthRangeMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Display")
	float DepthRangeMax = 65535.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Material")
	TObjectPtr<UMaterialInterface> TileMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Large Depth Map|Material")
	FName TileTextureParameterName = TEXT("DepthTexture");

	UObject* GetOrLoadTileResource(
		const FLargeDepthMapTile& Tile,
		ELargeDepthMapTileRequestPriority Priority = ELargeDepthMapTileRequestPriority::Visible);
	UObject* FindCachedTileResource(const FString& Key);
	bool IsTileReady(const FString& Key) const;
	bool IsTileLoading(const FString& Key) const;
	bool IsTileMissing(const FString& Key) const;
	static FString MakeTileKey(const FLargeDepthMapTile& Tile);
	static FString MakeTileKey(int32 Mip, int32 TileX, int32 TileY);
	void RequestResidentFallbackTiles(TSet<FString>& RetainedTileKeys);
	void ProcessPendingTileRequests();
	void SetCurrentViewStats(
		int32 Mip,
		int32 VisibleTileCount,
		int32 CandidateTileCount,
		int32 ReadyTileCount,
		const TSet<FString>& RetainedTileKeys);
	FString GetStatusText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	struct FPendingTileRequest
	{
		FLargeDepthMapTile Tile;
		ELargeDepthMapTileRequestPriority Priority = ELargeDepthMapTileRequestPriority::Prefetch;
		uint64 Sequence = 0;
	};

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UObject>> TileResourceCache;

	TMap<FString, int64> TileGpuBytes;
	TMap<FString, double> TileLastRetainedSeconds;
	TSet<FString> ResidentTileKeys;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> PooledTextures;

	UPROPERTY(Transient)
	TArray<FLargeDepthMapReleasedTexture> ReleasedTextures;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UMaterialInstanceDynamic>, TObjectPtr<UTexture2D>> MaterialRawTextures;

	TMap<TObjectKey<UTexture2D>, TEnumAsByte<TextureFilter>> AppliedTextureFilters;
	TSet<FString> LoadingTileKeys;
	TSet<FString> MissingTileKeys;
	TMap<FString, FPendingTileRequest> PendingTileRequests;
	TSharedPtr<SLargeDepthMapWidget> MyDepthMapWidget;
	int32 TotalTileRequests = 0;
	int32 TileCacheHits = 0;
	int32 TileLoadStarts = 0;
	int32 TileLoadCompletes = 0;
	int32 TextureCreateCount = 0;
	int32 TextureReuseCount = 0;
	int32 CurrentMip = 0;
	int32 CurrentVisibleTiles = 0;
	int32 CurrentCandidateTiles = 0;
	int32 CurrentReadyTiles = 0;
	int32 CurrentRetainedTiles = 0;
	int32 CurrentResidentTiles = 0;
	int32 CurrentPendingTiles = 0;
	int64 CurrentGpuBytes = 0;
	int64 MaxGpuBytes = 0;
	double AverageGpuBytes = 0.0;
	uint64 GpuSampleCount = 0;
	uint64 TileRequestSequence = 0;

	UObject* CreateTileResource(const FLargeDepthMapTile& Tile, const TArray64<uint8>& Bytes);
	UTexture2D* CreateRawTileTexture(const FLargeDepthMapTile& Tile, const TArray64<uint8>& Bytes);
	UTexture2D* CreateVisualTileTexture(const FLargeDepthMapTile& Tile, const TArray64<uint8>& Bytes);
	UTexture2D* AcquireTexture(int32 Width, int32 Height, EPixelFormat PixelFormat);
	bool ApplyTextureSampling(UTexture2D* Texture);
	TextureFilter GetTextureFilter() const;
	void PromoteReleasedTextures();
	void StartTileLoad(const FString& Key, const FLargeDepthMapTile& Tile);
	void ReleaseTileResource(const FString& Key, UObject* Resource);
	int64 EstimateTileGpuBytes(const FLargeDepthMapTile& Tile) const;
	void FinishTileLoad(const FString& Key, const FLargeDepthMapTile& Tile, TArray64<uint8>&& Bytes);
};
