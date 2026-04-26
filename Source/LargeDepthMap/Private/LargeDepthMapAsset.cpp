#include "LargeDepthMapAsset.h"

void ULargeDepthMapAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	PackedTileData.Serialize(Ar, this);
}

const FLargeDepthMapTile* ULargeDepthMapAsset::FindTile(
	int32 Mip,
	int32 TileX,
	int32 TileY) const
{
	for (const FLargeDepthMapTile& Tile : Tiles)
	{
		if (Tile.Mip == Mip && Tile.TileX == TileX && Tile.TileY == TileY)
		{
			return &Tile;
		}
	}

	return nullptr;
}

int32 ULargeDepthMapAsset::GetBytesPerPixel() const
{
	return PixelFormat == ELargeDepthMapPixelFormat::Gray8 ? 1 : 2;
}

bool ULargeDepthMapAsset::ReadTileBytes(
	const FLargeDepthMapTile& Tile,
	TArray64<uint8>& OutBytes) const
{
	if (Tile.DataOffset < 0 || Tile.DataSize <= 0)
	{
		return false;
	}

	const int64 PackedSize = PackedTileData.GetBulkDataSize();
	if (Tile.DataOffset + Tile.DataSize > PackedSize)
	{
		return false;
	}

	FScopeLock Lock(&PackedTileDataMutex);
	const uint8* SourceBytes = static_cast<const uint8*>(PackedTileData.LockReadOnly());
	if (!SourceBytes)
	{
		return false;
	}

	OutBytes.SetNumUninitialized(Tile.DataSize);
	FMemory::Memcpy(OutBytes.GetData(), SourceBytes + Tile.DataOffset, Tile.DataSize);
	PackedTileData.Unlock();
	return true;
}

void ULargeDepthMapAsset::ReplacePackedTileData(const TArray64<uint8>& SourceBytes)
{
	FScopeLock Lock(&PackedTileDataMutex);
	PackedTileData.Lock(LOCK_READ_WRITE);
	void* Destination = PackedTileData.Realloc(SourceBytes.Num());
	if (SourceBytes.Num() > 0)
	{
		FMemory::Memcpy(Destination, SourceBytes.GetData(), SourceBytes.Num());
	}
	PackedTileData.Unlock();
}

int64 ULargeDepthMapAsset::GetPackedTileDataSize() const
{
	return PackedTileData.GetBulkDataSize();
}
