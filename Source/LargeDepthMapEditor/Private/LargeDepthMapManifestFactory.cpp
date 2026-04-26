#include "LargeDepthMapManifestFactory.h"

#include "Dom/JsonObject.h"
#include "LargeDepthMapAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ULargeDepthMapManifestFactory::ULargeDepthMapManifestFactory()
{
	SupportedClass = ULargeDepthMapAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	Formats.Add(TEXT("json;Large Depth Map Manifest (*.ldm.json)"));
}

UObject* ULargeDepthMapManifestFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;
	if (!Filename.EndsWith(TEXT(".ldm.json")))
	{
		return nullptr;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Filename))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return nullptr;
	}

	ULargeDepthMapAsset* Asset = NewObject<ULargeDepthMapAsset>(InParent, InClass, InName, Flags);
	Asset->Width = RootObject->GetIntegerField(TEXT("width"));
	Asset->Height = RootObject->GetIntegerField(TEXT("height"));
	Asset->TileSize = RootObject->GetIntegerField(TEXT("tile_size"));
	Asset->MipCount = RootObject->GetIntegerField(TEXT("mip_count"));

	const FString Format = RootObject->GetStringField(TEXT("format")).ToLower();
	Asset->PixelFormat = Format == TEXT("gray8")
		? ELargeDepthMapPixelFormat::Gray8
		: ELargeDepthMapPixelFormat::Gray16;

	FString TileDataRoot;
	if (!RootObject->TryGetStringField(TEXT("tile_data_root"), TileDataRoot))
	{
		TileDataRoot = FPaths::GetPath(Filename);
	}

	const FString ResolvedTileDataRoot = FPaths::IsRelative(TileDataRoot)
		? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TileDataRoot)
		: TileDataRoot;

	const TArray<TSharedPtr<FJsonValue>>* TileValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("tiles"), TileValues))
	{
		return Asset;
	}

	TArray64<uint8> PackedBytes;
	for (const TSharedPtr<FJsonValue>& TileValue : *TileValues)
	{
		const TSharedPtr<FJsonObject> TileObject = TileValue->AsObject();
		if (!TileObject.IsValid())
		{
			continue;
		}

		FLargeDepthMapTile Tile;
		Tile.Mip = TileObject->GetIntegerField(TEXT("mip"));
		Tile.TileX = TileObject->GetIntegerField(TEXT("x"));
		Tile.TileY = TileObject->GetIntegerField(TEXT("y"));
		Tile.Width = TileObject->GetIntegerField(TEXT("width"));
		Tile.Height = TileObject->GetIntegerField(TEXT("height"));
		Tile.StoredWidth = TileObject->HasTypedField<EJson::Number>(TEXT("stored_width"))
			? TileObject->GetIntegerField(TEXT("stored_width"))
			: Tile.Width;
		Tile.StoredHeight = TileObject->HasTypedField<EJson::Number>(TEXT("stored_height"))
			? TileObject->GetIntegerField(TEXT("stored_height"))
			: Tile.Height;
		Tile.GutterPixels = TileObject->HasTypedField<EJson::Number>(TEXT("gutter"))
			? TileObject->GetIntegerField(TEXT("gutter"))
			: 0;
		Tile.RelativePath = TileObject->GetStringField(TEXT("path"));
		Tile.DataOffset = PackedBytes.Num();

		TArray64<uint8> TileBytes;
		const FString TilePath = FPaths::Combine(ResolvedTileDataRoot, Tile.RelativePath);
		if (!FFileHelper::LoadFileToArray(TileBytes, *TilePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to import large depth tile: %s"), *TilePath);
			continue;
		}

		Tile.DataSize = TileBytes.Num();
		PackedBytes.Append(TileBytes);
		Asset->Tiles.Add(Tile);
	}

	Asset->ReplacePackedTileData(PackedBytes);
	return Asset;
}
