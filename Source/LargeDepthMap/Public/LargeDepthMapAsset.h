#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HAL/CriticalSection.h"
#include "LargeDepthMapTypes.h"
#include "Serialization/BulkData.h"
#include "LargeDepthMapAsset.generated.h"

UCLASS(BlueprintType)
class LARGEDEPTHMAP_API ULargeDepthMapAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 TileSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 MipCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	ELargeDepthMapPixelFormat PixelFormat = ELargeDepthMapPixelFormat::Gray16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	TArray<FLargeDepthMapTile> Tiles;

	const FLargeDepthMapTile* FindTile(int32 Mip, int32 TileX, int32 TileY) const;
	int32 GetBytesPerPixel() const;
	bool ReadTileBytes(const FLargeDepthMapTile& Tile, TArray64<uint8>& OutBytes) const;
	void ReplacePackedTileData(const TArray64<uint8>& SourceBytes);
	int64 GetPackedTileDataSize() const;

private:
	FByteBulkData PackedTileData;
	mutable FCriticalSection PackedTileDataMutex;
};
