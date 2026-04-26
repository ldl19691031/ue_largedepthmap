#if WITH_DEV_AUTOMATION_TESTS

#include "LargeDepthMapAsset.h"
#include "LargeDepthMapWidget.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLargeDepthMapAssetTest,
	"UE57TestProj.LargeDepthMap.Asset.TileLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLargeDepthMapAssetTest::RunTest(const FString& Parameters)
{
	ULargeDepthMapAsset* Asset = NewObject<ULargeDepthMapAsset>();
	Asset->Width = 1024;
	Asset->Height = 1024;
	Asset->TileSize = 512;
	Asset->MipCount = 2;
	Asset->PixelFormat = ELargeDepthMapPixelFormat::Gray16;

	FLargeDepthMapTile Tile;
	Tile.Mip = 1;
	Tile.TileX = 0;
	Tile.TileY = 1;
	Tile.Width = 512;
	Tile.Height = 256;
	Tile.RelativePath = TEXT("mips/1/0_1.r16");
	Tile.DataOffset = 1;
	Tile.DataSize = 2;
	Asset->Tiles.Add(Tile);

	TArray64<uint8> PackedBytes;
	PackedBytes.Add(0);
	PackedBytes.Add(42);
	PackedBytes.Add(43);
	Asset->ReplacePackedTileData(PackedBytes);

	TArray64<uint8> TileBytes;
	const bool bReadTile = Asset->ReadTileBytes(Tile, TileBytes);

	TestEqual(TEXT("Gray16 byte width"), Asset->GetBytesPerPixel(), 2);
	TestNotNull(TEXT("Existing tile can be found"), Asset->FindTile(1, 0, 1));
	TestNull(TEXT("Missing tile is not fabricated"), Asset->FindTile(0, 0, 1));
	TestTrue(TEXT("Packed tile bytes can be read"), bReadTile);
	TestEqual(TEXT("Packed tile byte count"), TileBytes.Num(), int64(2));
	TestEqual(TEXT("Packed tile first byte"), TileBytes[0], uint8(42));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLargeDepthMapWidgetTileKeyTest,
	"UE57TestProj.LargeDepthMap.Widget.TileKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLargeDepthMapWidgetTileKeyTest::RunTest(const FString& Parameters)
{
	FLargeDepthMapTile Tile;
	Tile.Mip = 3;
	Tile.TileX = 12;
	Tile.TileY = 7;

	TestEqual(
		TEXT("Tile key from struct"),
		ULargeDepthMapWidget::MakeTileKey(Tile),
		FString(TEXT("3/12/7")));
	TestEqual(
		TEXT("Tile key from coordinates"),
		ULargeDepthMapWidget::MakeTileKey(3, 12, 7),
		FString(TEXT("3/12/7")));
	return true;
}

#endif
