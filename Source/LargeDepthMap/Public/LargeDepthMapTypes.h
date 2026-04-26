#pragma once

#include "CoreMinimal.h"
#include "LargeDepthMapTypes.generated.h"

UENUM(BlueprintType)
enum class ELargeDepthMapPixelFormat : uint8
{
	Gray8,
	Gray16
};

USTRUCT(BlueprintType)
struct FLargeDepthMapTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 Mip = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 TileX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 TileY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 Width = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 Height = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 StoredWidth = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 StoredHeight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int32 GutterPixels = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	FString RelativePath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int64 DataOffset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Large Depth Map")
	int64 DataSize = 0;
};
