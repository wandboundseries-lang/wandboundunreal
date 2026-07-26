#include "WBCharacterModelPipeline.h"

#include "WBCardDefinitionRepository.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <openssl/sha.h>

namespace
{
using FCondensedWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

void AddDiagnostic(
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics,
	const EWBCharacterDiagnosticSeverity Severity,
	const FString& Code,
	const FString& Message,
	const FString& ManifestPath = FString(),
	const FString& FieldPath = FString(),
	const FString& SourcePath = FString(),
	const FString& Action = FString())
{
	FWBCharacterManifestDiagnostic Diagnostic;
	Diagnostic.Severity = Severity;
	Diagnostic.Code = Code;
	Diagnostic.Message = Message;
	Diagnostic.ManifestPath = ManifestPath;
	Diagnostic.FieldPath = FieldPath;
	Diagnostic.SourceRelativePath = SourcePath;
	Diagnostic.RecommendedAction = Action;
	Diagnostics.Add(MoveTemp(Diagnostic));
}

bool HasError(const TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	return Diagnostics.ContainsByPredicate([](const FWBCharacterManifestDiagnostic& Diagnostic)
	{
		return Diagnostic.Severity == EWBCharacterDiagnosticSeverity::Error;
	});
}

FString SHA256Bytes(const uint8* Data, const int64 Size)
{
	if (Size < 0)
	{
		return FString();
	}
	uint8 Digest[SHA256_DIGEST_LENGTH] = {};
	if (SHA256(Data, static_cast<size_t>(Size), Digest) == nullptr)
	{
		return FString();
	}
	FString Result;
	Result.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (const uint8 Byte : Digest)
	{
		Result += FString::Printf(TEXT("%02x"), Byte);
	}
	return Result;
}

FString LowerExtension(const FString& Path)
{
	return FPaths::GetExtension(Path, true).ToLower();
}

bool IsSupportedTextureExtension(const FString& Extension)
{
	static const TSet<FString> Extensions = {
		TEXT(".png"), TEXT(".jpg"), TEXT(".jpeg"), TEXT(".tga"), TEXT(".tif"),
		TEXT(".tiff"), TEXT(".exr"), TEXT(".bmp")
	};
	return Extensions.Contains(Extension);
}

bool IsExecutableExtension(const FString& Extension)
{
	static const TSet<FString> Extensions = {
		TEXT(".exe"), TEXT(".dll"), TEXT(".com"), TEXT(".bat"), TEXT(".cmd"),
		TEXT(".ps1"), TEXT(".vbs"), TEXT(".js"), TEXT(".msi"), TEXT(".scr")
	};
	return Extensions.Contains(Extension);
}

FString SourceFormatName(const EWBCharacterSourceFormat Format)
{
	switch (Format)
	{
	case EWBCharacterSourceFormat::FBX: return TEXT("fbx");
	case EWBCharacterSourceFormat::GLB: return TEXT("glb");
	case EWBCharacterSourceFormat::GLTF: return TEXT("gltf");
	default: return TEXT("unknown");
	}
}

FString ModelTypeName(const EWBCharacterModelType Type)
{
	return Type == EWBCharacterModelType::Skeletal ? TEXT("skeletal") : TEXT("static");
}

FString SeverityName(const EWBCharacterDiagnosticSeverity Severity)
{
	return Severity == EWBCharacterDiagnosticSeverity::Error ? TEXT("error") : TEXT("warning");
}

bool TryParseSourceFormat(const FString& Value, EWBCharacterSourceFormat& OutFormat)
{
	if (Value.Equals(TEXT("fbx"), ESearchCase::IgnoreCase))
	{
		OutFormat = EWBCharacterSourceFormat::FBX;
		return true;
	}
	if (Value.Equals(TEXT("glb"), ESearchCase::IgnoreCase))
	{
		OutFormat = EWBCharacterSourceFormat::GLB;
		return true;
	}
	if (Value.Equals(TEXT("gltf"), ESearchCase::IgnoreCase))
	{
		OutFormat = EWBCharacterSourceFormat::GLTF;
		return true;
	}
	return false;
}

bool TryParseModelType(const FString& Value, EWBCharacterModelType& OutType)
{
	if (Value.Equals(TEXT("static"), ESearchCase::IgnoreCase))
	{
		OutType = EWBCharacterModelType::Static;
		return true;
	}
	if (Value.Equals(TEXT("skeletal"), ESearchCase::IgnoreCase))
	{
		OutType = EWBCharacterModelType::Skeletal;
		return true;
	}
	return false;
}

bool ReadRequiredString(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	const FString& FieldPath,
	const FString& ManifestPath,
	FString& OutValue,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue) || OutValue.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_required_field_missing"),
			FString::Printf(TEXT("Required string field '%s' is missing or empty."), *FieldPath),
			ManifestPath,
			FieldPath,
			FString(),
			TEXT("Provide a non-empty value."));
		return false;
	}
	OutValue.TrimStartAndEndInline();
	return true;
}

bool ReadRequiredBool(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	const FString& FieldPath,
	const FString& ManifestPath,
	bool& OutValue,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	if (!Object.IsValid() || !Object->TryGetBoolField(Field, OutValue))
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_required_field_missing"),
			FString::Printf(TEXT("Required boolean field '%s' is missing."), *FieldPath),
			ManifestPath,
			FieldPath);
		return false;
	}
	return true;
}

TSharedPtr<FJsonObject> ReadObject(
	const TSharedPtr<FJsonObject>& Parent,
	const FString& Field,
	const FString& FieldPath,
	const FString& ManifestPath,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics,
	const bool bRequired)
{
	const TSharedPtr<FJsonObject>* Value = nullptr;
	if (Parent.IsValid() && Parent->TryGetObjectField(Field, Value) && Value != nullptr && Value->IsValid())
	{
		return *Value;
	}
	if (bRequired)
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_required_object_missing"),
			FString::Printf(TEXT("Required object '%s' is missing."), *FieldPath),
			ManifestPath,
			FieldPath);
	}
	return nullptr;
}

void ValidateKnownFields(
	const TSharedPtr<FJsonObject>& Object,
	const TSet<FString>& AllowedFields,
	const FString& ObjectPath,
	const FString& ManifestPath,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	if (!Object.IsValid())
	{
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		if (!AllowedFields.Contains(Pair.Key))
		{
			AddDiagnostic(
				Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_unknown_field"),
				TEXT("The manifest contains a field that is not part of schema version 1."),
				ManifestPath,
				ObjectPath.IsEmpty() ? Pair.Key : ObjectPath + TEXT(".") + Pair.Key,
				FString(),
				TEXT("Remove the unknown field or update to a supported schema version."));
		}
	}
}

FVector ReadVector(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Field,
	const FString& FieldPath,
	const FString& ManifestPath,
	const FVector& DefaultValue,
	TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values))
	{
		return DefaultValue;
	}
	if (Values == nullptr || Values->Num() != 3)
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_transform_vector_invalid"),
			FString::Printf(TEXT("'%s' must contain exactly three finite numbers."), *FieldPath),
			ManifestPath,
			FieldPath);
		return DefaultValue;
	}
	FVector Result;
	Result.X = (*Values)[0]->AsNumber();
	Result.Y = (*Values)[1]->AsNumber();
	Result.Z = (*Values)[2]->AsNumber();
	if (!FMath::IsFinite(Result.X)
		|| !FMath::IsFinite(Result.Y)
		|| !FMath::IsFinite(Result.Z))
	{
		AddDiagnostic(
			Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_transform_not_finite"),
			FString::Printf(TEXT("'%s' contains NaN or infinity."), *FieldPath),
			ManifestPath,
			FieldPath);
		return DefaultValue;
	}
	return Result;
}

bool ContainsForbiddenField(
	const TSharedPtr<FJsonObject>& Object,
	FString& OutFieldPath,
	const FString& Prefix = FString())
{
	if (!Object.IsValid())
	{
		return false;
	}
	static const TSet<FString> Forbidden = {
		TEXT("deck"), TEXT("hand"), TEXT("discard"), TEXT("opponent_hand"),
		TEXT("concealed_marker"), TEXT("hidden_marker"), TEXT("game_state"),
		TEXT("player_state"), TEXT("private_card"), TEXT("credentials"),
		TEXT("token"), TEXT("password"), TEXT("access_key")
	};
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
	{
		const FString Key = Pair.Key.ToLower();
		const FString Path = Prefix.IsEmpty() ? Pair.Key : Prefix + TEXT(".") + Pair.Key;
		if (Forbidden.Contains(Key)
			|| Key.StartsWith(TEXT("private_"))
			|| Key.Contains(TEXT("concealed"))
			|| Key.Contains(TEXT("hidden_zone")))
		{
			OutFieldPath = Path;
			return true;
		}
		if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Object)
		{
			if (ContainsForbiddenField(Pair.Value->AsObject(), OutFieldPath, Path))
			{
				return true;
			}
		}
		else if (Pair.Value.IsValid() && Pair.Value->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Array = Pair.Value->AsArray();
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				if (Array[Index].IsValid() && Array[Index]->Type == EJson::Object
					&& ContainsForbiddenField(
						Array[Index]->AsObject(),
						OutFieldPath,
						FString::Printf(TEXT("%s[%d]"), *Path, Index)))
				{
					return true;
				}
			}
		}
	}
	return false;
}

FString SanitizePackageLeaf(const FString& Source)
{
	FString Result;
	for (const TCHAR Character : Source)
	{
		Result.AppendChar(FChar::IsAlnum(Character) ? FChar::ToLower(Character) : TEXT('_'));
	}
	while (Result.Contains(TEXT("__")))
	{
		Result.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Result.TrimCharInline(TEXT('_'), nullptr);
	return Result.IsEmpty() ? TEXT("asset") : Result;
}

FString RoleForInventoryPath(
	const FWBCharacterModelManifest& Manifest,
	const FString& RelativePath,
	bool& bOutRequired)
{
	bOutRequired = false;
	if (RelativePath.Equals(TEXT("character_manifest.json"), ESearchCase::IgnoreCase))
	{
		bOutRequired = true;
		return TEXT("manifest");
	}
	if (RelativePath.Equals(Manifest.Source.ModelPath, ESearchCase::IgnoreCase))
	{
		bOutRequired = true;
		return TEXT("primary_model");
	}
	for (const FWBCharacterTextureDefinition& Texture : Manifest.Source.Textures)
	{
		if (RelativePath.Equals(Texture.SourcePath, ESearchCase::IgnoreCase))
		{
			bOutRequired = Texture.bRequired;
			return TEXT("texture:") + Texture.Role;
		}
	}
	for (const FWBCharacterAnimationDefinition& Animation : Manifest.Source.Animations)
	{
		if (RelativePath.Equals(Animation.SourcePath, ESearchCase::IgnoreCase))
		{
			bOutRequired = Animation.bRequired;
			return TEXT("animation:") + Animation.Role;
		}
	}
	for (const TPair<FString, FString>& Preview : Manifest.PreviewPaths)
	{
		if (RelativePath.Equals(Preview.Value, ESearchCase::IgnoreCase))
		{
			return TEXT("preview:") + Preview.Key;
		}
	}
	if (RelativePath.StartsWith(TEXT("notes/"), ESearchCase::IgnoreCase))
	{
		return TEXT("notes");
	}
	return TEXT("undeclared_optional");
}

FString FileTypeForPath(const FString& RelativePath)
{
	const FString Extension = LowerExtension(RelativePath);
	if (Extension == TEXT(".fbx")) return TEXT("fbx");
	if (Extension == TEXT(".glb")) return TEXT("glb");
	if (Extension == TEXT(".gltf")) return TEXT("gltf");
	if (IsSupportedTextureExtension(Extension)) return TEXT("image");
	if (Extension == TEXT(".json")) return TEXT("json");
	if (Extension == TEXT(".md") || Extension == TEXT(".txt")) return TEXT("text");
	return Extension.IsEmpty() ? TEXT("unknown") : Extension.RightChop(1);
}

bool RunGit(
	const FString& ProjectRoot,
	const FString& Arguments,
	FString& OutStdOut,
	int32& OutReturnCode)
{
	FString StdErr;
	const FString FullArguments = FString::Printf(
		TEXT("-C \"%s\" %s"),
		*ProjectRoot,
		*Arguments);
	return FPlatformProcess::ExecProcess(
		TEXT("git"),
		*FullArguments,
		&OutReturnCode,
		&OutStdOut,
		&StdErr);
}

bool AtomicWrite(const FString& AbsolutePath, const FString& Contents)
{
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
	const FString TemporaryPath = AbsolutePath + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(
		Contents,
		*TemporaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return false;
	}
	return IFileManager::Get().Move(
		*AbsolutePath,
		*TemporaryPath,
		true,
		true,
		false,
		true);
}

void WriteDiagnostic(
	const TSharedRef<FCondensedWriter>& Writer,
	const FWBCharacterManifestDiagnostic& Diagnostic)
{
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("severity"), SeverityName(Diagnostic.Severity));
	Writer->WriteValue(TEXT("code"), Diagnostic.Code);
	Writer->WriteValue(TEXT("message"), Diagnostic.Message);
	Writer->WriteValue(TEXT("manifest_path"), Diagnostic.ManifestPath);
	Writer->WriteValue(TEXT("field_path"), Diagnostic.FieldPath);
	Writer->WriteValue(TEXT("source_relative_path"), Diagnostic.SourceRelativePath);
	Writer->WriteValue(TEXT("recommended_action"), Diagnostic.RecommendedAction);
	Writer->WriteObjectEnd();
}

void WriteStringArray(
	const TSharedRef<FCondensedWriter>& Writer,
	const FString& Field,
	const TArray<FString>& Values)
{
	Writer->WriteArrayStart(Field);
	for (const FString& Value : Values)
	{
		Writer->WriteValue(Value);
	}
	Writer->WriteArrayEnd();
}

void WriteStringMap(
	const TSharedRef<FCondensedWriter>& Writer,
	const FString& Field,
	const TMap<FString, FString>& Values)
{
	TArray<FString> Keys;
	Values.GetKeys(Keys);
	Keys.Sort();
	Writer->WriteObjectStart(Field);
	for (const FString& Key : Keys)
	{
		Writer->WriteValue(Key, Values[Key]);
	}
	Writer->WriteObjectEnd();
}

FString DiagnosticsToJson(const TArray<FWBCharacterManifestDiagnostic>& Diagnostics)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteArrayStart();
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Diagnostics)
	{
		WriteDiagnostic(Writer, Diagnostic);
	}
	Writer->WriteArrayEnd();
	Writer->Close();
	return Output;
}
}

bool FWBCharacterManifestValidationResult::IsValid() const
{
	return bParsed && ErrorCount() == 0;
}

int32 FWBCharacterManifestValidationResult::ErrorCount() const
{
	int32 Count = 0;
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Diagnostics)
	{
		Count += Diagnostic.Severity == EWBCharacterDiagnosticSeverity::Error ? 1 : 0;
	}
	return Count;
}

int32 FWBCharacterManifestValidationResult::WarningCount() const
{
	int32 Count = 0;
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Diagnostics)
	{
		Count += Diagnostic.Severity == EWBCharacterDiagnosticSeverity::Warning ? 1 : 0;
	}
	return Count;
}

bool FWBCharacterDestinationPlan::IsValid() const
{
	return !CharacterId.IsEmpty()
		&& !DestinationRoot.IsEmpty()
		&& !PrimaryMeshPackage.IsEmpty()
		&& !HasError(Diagnostics);
}

const TCHAR* WBCharacterModelPipeline::GetManifestSchemaRepositoryPath()
{
	return TEXT("Data/CharacterModels/CharacterModelManifest.schema.json");
}

const TCHAR* WBCharacterModelPipeline::GetCatalogRepositoryPath()
{
	return TEXT("Data/CharacterModels/CharacterModelCatalog.json");
}

const TCHAR* WBCharacterModelPipeline::GetCatalogSchemaRepositoryPath()
{
	return TEXT("Data/CharacterModels/CharacterModelCatalog.schema.json");
}

bool WBCharacterModelPipeline::IsSafeCharacterId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64 || !FChar::IsLower(Value[0]))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!(FChar::IsLower(Character) || FChar::IsDigit(Character) || Character == TEXT('_')))
		{
			return false;
		}
	}
	return !Value.Contains(TEXT("__"));
}

bool WBCharacterModelPipeline::IsSafeCardDefinitionId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 128 || !(FChar::IsAlpha(Value[0]) || Value[0] == TEXT('_')))
	{
		return false;
	}
	for (const TCHAR Character : Value)
	{
		if (!(FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-')))
		{
			return false;
		}
	}
	return true;
}

FString WBCharacterModelPipeline::NormalizeRepositoryPath(const FString& Value)
{
	FString Result = Value.TrimStartAndEnd();
	Result.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Result.StartsWith(TEXT("./")))
	{
		Result.RightChopInline(2);
	}
	return Result;
}

bool WBCharacterModelPipeline::IsSafeBundleRelativePath(const FString& Value)
{
	const FString Normalized = NormalizeRepositoryPath(Value);
	if (Normalized.IsEmpty()
		|| Normalized.StartsWith(TEXT("/"))
		|| Normalized.Contains(TEXT(":"))
		|| Normalized.Contains(TEXT("://"))
		|| FPaths::IsRelative(Normalized) == false)
	{
		return false;
	}
	TArray<FString> Segments;
	Normalized.ParseIntoArray(Segments, TEXT("/"), false);
	return !Segments.Contains(TEXT(".."))
		&& !Segments.Contains(TEXT("."))
		&& !Segments.ContainsByPredicate([](const FString& Segment)
		{
			return Segment.IsEmpty();
		});
}

FString WBCharacterModelPipeline::SHA256File(const FString& AbsolutePath)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsolutePath))
	{
		return FString();
	}
	return SHA256Bytes(Bytes.GetData(), Bytes.Num());
}

FString WBCharacterModelPipeline::SHA256String(const FString& Value)
{
	FTCHARToUTF8 UTF8(*Value);
	return SHA256Bytes(
		reinterpret_cast<const uint8*>(UTF8.Get()),
		UTF8.Length());
}

FWBCharacterManifestValidationResult
WBCharacterModelPipeline::ParseAndValidateManifestJson(
	const FString& Json,
	const FString& ManifestRepositoryPath,
	const FString& ProjectRoot,
	const bool bRequireFiles,
	const FWBCardDefinitionRepository* CardDefinitionRepository,
	const bool bRequireCardDefinition)
{
	FWBCharacterManifestValidationResult Result;
	Result.Manifest.ManifestRepositoryPath = NormalizeRepositoryPath(ManifestRepositoryPath);
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_json_invalid"),
			TEXT("The character manifest is not valid JSON."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("$"),
			FString(),
			TEXT("Fix the JSON syntax and validate again."));
		return Result;
	}
	Result.bParsed = true;
	ValidateKnownFields(
		Root,
		{
			TEXT("schema_version"), TEXT("character_id"), TEXT("display_name"),
			TEXT("card_definition_id"), TEXT("approval"), TEXT("source"),
			TEXT("presentation"), TEXT("import"), TEXT("previews"),
			TEXT("tags"), TEXT("notes")
		},
		FString(),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);

	FString ForbiddenField;
	if (ContainsForbiddenField(Root, ForbiddenField))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_private_data_forbidden"),
			TEXT("The manifest contains a private, gameplay, credential, or concealed-data field."),
			Result.Manifest.ManifestRepositoryPath,
			ForbiddenField,
			FString(),
			TEXT("Remove private or gameplay data from the presentation manifest."));
	}

	double SchemaVersion = 0.0;
	if (!Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| !FMath::IsNearlyEqual(SchemaVersion, 1.0))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_schema_version_unsupported"),
			TEXT("Only character manifest schema version 1 is supported."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("schema_version"));
	}
	else
	{
		Result.Manifest.SchemaVersion = 1;
	}

	ReadRequiredString(
		Root,
		TEXT("character_id"),
		TEXT("character_id"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Manifest.CharacterId,
		Result.Diagnostics);
	ReadRequiredString(
		Root,
		TEXT("display_name"),
		TEXT("display_name"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Manifest.DisplayName,
		Result.Diagnostics);
	ReadRequiredString(
		Root,
		TEXT("card_definition_id"),
		TEXT("card_definition_id"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Manifest.CardDefinitionId,
		Result.Diagnostics);

	if (!Result.Manifest.CharacterId.IsEmpty()
		&& !IsSafeCharacterId(Result.Manifest.CharacterId))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_character_id_invalid"),
			TEXT("Character IDs must use lowercase letters, digits, and single underscores."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("character_id"));
	}
	if (!Result.Manifest.CardDefinitionId.IsEmpty()
		&& !IsSafeCardDefinitionId(Result.Manifest.CardDefinitionId))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_card_definition_id_invalid"),
			TEXT("The public card definition ID contains unsupported characters."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("card_definition_id"));
	}

	const TSharedPtr<FJsonObject> Approval = ReadObject(
		Root,
		TEXT("approval"),
		TEXT("approval"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics,
		true);
	ValidateKnownFields(
		Approval,
		{ TEXT("approved_for_import"), TEXT("approved_by"), TEXT("approval_note") },
		TEXT("approval"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);
	ReadRequiredBool(
		Approval,
		TEXT("approved_for_import"),
		TEXT("approval.approved_for_import"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Manifest.bApprovedForImport,
		Result.Diagnostics);
	if (Approval.IsValid())
	{
		Approval->TryGetStringField(TEXT("approved_by"), Result.Manifest.ApprovedBy);
		Approval->TryGetStringField(TEXT("approval_note"), Result.Manifest.ApprovalNote);
	}
	if (!Result.Manifest.bApprovedForImport)
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_import_approval_missing"),
			TEXT("The bundle is not explicitly approved for import."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("approval.approved_for_import"),
			FString(),
			TEXT("Set approved_for_import only after the user approves this exact bundle."));
	}
	if (Result.Manifest.bApprovedForImport && Result.Manifest.ApprovedBy.TrimStartAndEnd().IsEmpty())
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_approver_missing"),
			TEXT("Approved bundles must identify who approved them."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("approval.approved_by"));
	}

	const TSharedPtr<FJsonObject> Source = ReadObject(
		Root,
		TEXT("source"),
		TEXT("source"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics,
		true);
	ValidateKnownFields(
		Source,
		{
			TEXT("model"), TEXT("model_format"), TEXT("model_type"),
			TEXT("textures_directory"), TEXT("animations_directory"),
			TEXT("textures"), TEXT("animations")
		},
		TEXT("source"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);
	FString Format;
	FString ModelType;
	ReadRequiredString(
		Source,
		TEXT("model"),
		TEXT("source.model"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Manifest.Source.ModelPath,
		Result.Diagnostics);
	ReadRequiredString(
		Source,
		TEXT("model_format"),
		TEXT("source.model_format"),
		Result.Manifest.ManifestRepositoryPath,
		Format,
		Result.Diagnostics);
	ReadRequiredString(
		Source,
		TEXT("model_type"),
		TEXT("source.model_type"),
		Result.Manifest.ManifestRepositoryPath,
		ModelType,
		Result.Diagnostics);
	if (!Format.IsEmpty() && !TryParseSourceFormat(Format, Result.Manifest.Source.Format))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_model_format_unsupported"),
			TEXT("Supported model formats are fbx, glb, and gltf."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source.model_format"));
	}
	if (!ModelType.IsEmpty() && !TryParseModelType(ModelType, Result.Manifest.Source.ModelType))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_model_type_invalid"),
			TEXT("Model type must be static or skeletal."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source.model_type"));
	}
	if (Source.IsValid())
	{
		Source->TryGetStringField(TEXT("textures_directory"), Result.Manifest.Source.TexturesDirectory);
		Source->TryGetStringField(TEXT("animations_directory"), Result.Manifest.Source.AnimationsDirectory);
	}
	if (!IsSafeBundleRelativePath(Result.Manifest.Source.TexturesDirectory)
		|| !IsSafeBundleRelativePath(Result.Manifest.Source.AnimationsDirectory))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_source_directory_unsafe"),
			TEXT("Texture and animation directories must remain inside the character bundle."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source"));
	}

	if (!Result.Manifest.Source.ModelPath.IsEmpty()
		&& !IsSafeBundleRelativePath(Result.Manifest.Source.ModelPath))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_source_path_unsafe"),
			TEXT("The source model path must remain inside the character bundle."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source.model"),
			Result.Manifest.Source.ModelPath);
	}
	const FString ModelExtension = LowerExtension(Result.Manifest.Source.ModelPath);
	if (!ModelExtension.IsEmpty()
		&& ModelExtension != TEXT(".") + SourceFormatName(Result.Manifest.Source.Format))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_model_extension_mismatch"),
			TEXT("The model extension does not match source.model_format."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source.model_format"),
			Result.Manifest.Source.ModelPath);
	}
	if (IsExecutableExtension(ModelExtension))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_executable_source_rejected"),
			TEXT("Executable files cannot be imported as models."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("source.model"),
			Result.Manifest.Source.ModelPath);
	}

	const TArray<TSharedPtr<FJsonValue>>* TextureValues = nullptr;
	if (Source.IsValid() && Source->TryGetArrayField(TEXT("textures"), TextureValues))
	{
		TSet<FString> Roles;
		for (int32 Index = 0; TextureValues != nullptr && Index < TextureValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Entry = (*TextureValues)[Index]->AsObject();
			ValidateKnownFields(
				Entry,
				{ TEXT("role"), TEXT("path"), TEXT("required") },
				FString::Printf(TEXT("source.textures[%d]"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Result.Diagnostics);
			FWBCharacterTextureDefinition Texture;
			ReadRequiredString(
				Entry,
				TEXT("role"),
				FString::Printf(TEXT("source.textures[%d].role"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Texture.Role,
				Result.Diagnostics);
			ReadRequiredString(
				Entry,
				TEXT("path"),
				FString::Printf(TEXT("source.textures[%d].path"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Texture.SourcePath,
				Result.Diagnostics);
			if (Entry.IsValid()) Entry->TryGetBoolField(TEXT("required"), Texture.bRequired);
			Texture.Role = Texture.Role.ToLower();
			Texture.SourcePath = NormalizeRepositoryPath(Texture.SourcePath);
			if (Roles.Contains(Texture.Role))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_duplicate_texture_role"),
					TEXT("Texture semantic roles must be unique."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.textures[%d].role"), Index));
			}
			Roles.Add(Texture.Role);
			static const TSet<FString> TextureRoles = {
				TEXT("base_color"), TEXT("normal"), TEXT("orm"), TEXT("roughness"),
				TEXT("metallic"), TEXT("opacity"), TEXT("emissive"), TEXT("additional")
			};
			if (!TextureRoles.Contains(Texture.Role))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_texture_role_invalid"),
					TEXT("The texture semantic role is not supported by schema version 1."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.textures[%d].role"), Index));
			}
			if (!IsSafeBundleRelativePath(Texture.SourcePath)
				|| !IsSupportedTextureExtension(LowerExtension(Texture.SourcePath)))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_texture_path_invalid"),
					TEXT("Texture paths must be safe bundle-relative paths with supported image extensions."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.textures[%d].path"), Index),
					Texture.SourcePath);
			}
			Result.Manifest.Source.Textures.Add(MoveTemp(Texture));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AnimationValues = nullptr;
	if (Source.IsValid() && Source->TryGetArrayField(TEXT("animations"), AnimationValues))
	{
		TSet<FString> Roles;
		for (int32 Index = 0; AnimationValues != nullptr && Index < AnimationValues->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Entry = (*AnimationValues)[Index]->AsObject();
			ValidateKnownFields(
				Entry,
				{ TEXT("role"), TEXT("path"), TEXT("required") },
				FString::Printf(TEXT("source.animations[%d]"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Result.Diagnostics);
			FWBCharacterAnimationDefinition Animation;
			ReadRequiredString(
				Entry,
				TEXT("role"),
				FString::Printf(TEXT("source.animations[%d].role"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Animation.Role,
				Result.Diagnostics);
			ReadRequiredString(
				Entry,
				TEXT("path"),
				FString::Printf(TEXT("source.animations[%d].path"), Index),
				Result.Manifest.ManifestRepositoryPath,
				Animation.SourcePath,
				Result.Diagnostics);
			if (Entry.IsValid()) Entry->TryGetBoolField(TEXT("required"), Animation.bRequired);
			Animation.Role = Animation.Role.ToLower();
			Animation.SourcePath = NormalizeRepositoryPath(Animation.SourcePath);
			if (Roles.Contains(Animation.Role))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_duplicate_animation_role"),
					TEXT("Animation semantic roles must be unique."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.animations[%d].role"), Index));
			}
			Roles.Add(Animation.Role);
			static const TSet<FString> AnimationRoles = {
				TEXT("idle"), TEXT("move"), TEXT("attack"), TEXT("hit"),
				TEXT("summon"), TEXT("death"), TEXT("activation")
			};
			if (!AnimationRoles.Contains(Animation.Role))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_animation_role_invalid"),
					TEXT("The animation semantic role is not supported by schema version 1."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.animations[%d].role"), Index));
			}
			if (!IsSafeBundleRelativePath(Animation.SourcePath)
				|| LowerExtension(Animation.SourcePath) != TEXT(".fbx"))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_animation_path_invalid"),
					TEXT("Animation paths must be safe bundle-relative FBX paths."),
					Result.Manifest.ManifestRepositoryPath,
					FString::Printf(TEXT("source.animations[%d].path"), Index),
					Animation.SourcePath);
			}
			Result.Manifest.Source.Animations.Add(MoveTemp(Animation));
		}
	}

	const TSharedPtr<FJsonObject> Import = ReadObject(
		Root,
		TEXT("import"),
		TEXT("import"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics,
		true);
	ValidateKnownFields(
		Import,
		{
			TEXT("import_materials"), TEXT("import_textures"), TEXT("import_animations"),
			TEXT("create_physics_asset"), TEXT("generate_collision"),
			TEXT("skeleton_policy"), TEXT("existing_skeleton_package"),
			TEXT("normal_policy")
		},
		TEXT("import"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);
	if (Import.IsValid())
	{
		Import->TryGetBoolField(TEXT("import_materials"), Result.Manifest.Import.bImportMaterials);
		Import->TryGetBoolField(TEXT("import_textures"), Result.Manifest.Import.bImportTextures);
		Import->TryGetBoolField(TEXT("import_animations"), Result.Manifest.Import.bImportAnimations);
		Import->TryGetBoolField(TEXT("create_physics_asset"), Result.Manifest.Import.bCreatePhysicsAsset);
		Import->TryGetBoolField(TEXT("generate_collision"), Result.Manifest.Import.bGenerateCollision);
		Import->TryGetStringField(TEXT("skeleton_policy"), Result.Manifest.Import.SkeletonPolicy);
		Import->TryGetStringField(TEXT("existing_skeleton_package"), Result.Manifest.Import.ExistingSkeletonPackage);
		Import->TryGetStringField(TEXT("normal_policy"), Result.Manifest.Import.NormalPolicy);
	}
	Result.Manifest.Import.SkeletonPolicy = Result.Manifest.Import.SkeletonPolicy.ToLower();
	Result.Manifest.Import.NormalPolicy = Result.Manifest.Import.NormalPolicy.ToLower();
	static const TSet<FString> NormalPolicies = {
		TEXT("compute_normals"), TEXT("import_normals"), TEXT("import_normals_and_tangents")
	};
	if (!NormalPolicies.Contains(Result.Manifest.Import.NormalPolicy))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_normal_policy_invalid"),
			TEXT("Normal policy must be compute_normals, import_normals, or import_normals_and_tangents."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("import.normal_policy"));
	}
	if (Result.Manifest.Source.ModelType == EWBCharacterModelType::Static)
	{
		if (Result.Manifest.Import.bImportAnimations || Result.Manifest.Source.Animations.Num() > 0)
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_static_animation_contradiction"),
				TEXT("Static models cannot request skeletal animation import."),
				Result.Manifest.ManifestRepositoryPath,
				TEXT("import.import_animations"));
		}
		if (Result.Manifest.Import.bCreatePhysicsAsset
			|| !Result.Manifest.Import.SkeletonPolicy.Equals(TEXT("none"), ESearchCase::IgnoreCase))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_static_skeleton_policy_contradiction"),
				TEXT("Static models must use skeleton_policy 'none' and cannot create a physics asset."),
				Result.Manifest.ManifestRepositoryPath,
				TEXT("import.skeleton_policy"));
		}
	}
	else
	{
		if (!(Result.Manifest.Import.SkeletonPolicy == TEXT("create")
			|| Result.Manifest.Import.SkeletonPolicy == TEXT("reuse")))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_skeleton_policy_invalid"),
				TEXT("Skeletal models must use skeleton_policy 'create' or 'reuse'."),
				Result.Manifest.ManifestRepositoryPath,
				TEXT("import.skeleton_policy"));
		}
		if (Result.Manifest.Import.SkeletonPolicy == TEXT("reuse")
			&& !FPackageName::IsValidObjectPath(Result.Manifest.Import.ExistingSkeletonPackage)
			&& !FPackageName::IsValidLongPackageName(Result.Manifest.Import.ExistingSkeletonPackage))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_reuse_skeleton_missing"),
				TEXT("Skeleton reuse requires a valid existing_skeleton_package."),
				Result.Manifest.ManifestRepositoryPath,
				TEXT("import.existing_skeleton_package"));
		}
	}

	const TSharedPtr<FJsonObject> Presentation = ReadObject(
		Root,
		TEXT("presentation"),
		TEXT("presentation"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics,
		true);
	ValidateKnownFields(
		Presentation,
		{ TEXT("role"), TEXT("scale"), TEXT("rotation"), TEXT("offset"), TEXT("facing_axis") },
		TEXT("presentation"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);
	if (Presentation.IsValid())
	{
		Presentation->TryGetStringField(TEXT("role"), Result.Manifest.Presentation.Role);
		Presentation->TryGetNumberField(TEXT("scale"), Result.Manifest.Presentation.Scale);
		Presentation->TryGetStringField(TEXT("facing_axis"), Result.Manifest.Presentation.FacingAxis);
		Result.Manifest.Presentation.Rotation = ReadVector(
			Presentation,
			TEXT("rotation"),
			TEXT("presentation.rotation"),
			Result.Manifest.ManifestRepositoryPath,
			FVector::ZeroVector,
			Result.Diagnostics);
		Result.Manifest.Presentation.Offset = ReadVector(
			Presentation,
			TEXT("offset"),
			TEXT("presentation.offset"),
			Result.Manifest.ManifestRepositoryPath,
			FVector::ZeroVector,
			Result.Diagnostics);
	}
	if (!FMath::IsFinite(Result.Manifest.Presentation.Scale)
		|| Result.Manifest.Presentation.Scale <= 0.0
		|| Result.Manifest.Presentation.Scale > 1000.0)
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_scale_invalid"),
			TEXT("Presentation scale must be finite, positive, and no greater than 1000."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("presentation.scale"));
	}
	static const TSet<FString> PresentationRoles = {
		TEXT("player_hero"), TEXT("player_unit"), TEXT("neutral_npc")
	};
	if (!PresentationRoles.Contains(Result.Manifest.Presentation.Role.ToLower()))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_presentation_role_invalid"),
			TEXT("Presentation role must be player_hero, player_unit, or neutral_npc."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("presentation.role"));
	}
	static const TSet<FString> FacingAxes = {
		TEXT("positive_x"), TEXT("negative_x"), TEXT("positive_y"), TEXT("negative_y")
	};
	if (!FacingAxes.Contains(Result.Manifest.Presentation.FacingAxis.ToLower()))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_facing_axis_invalid"),
			TEXT("Facing axis must be positive_x, negative_x, positive_y, or negative_y."),
			Result.Manifest.ManifestRepositoryPath,
			TEXT("presentation.facing_axis"));
	}

	const TSharedPtr<FJsonObject> Previews = ReadObject(
		Root,
		TEXT("previews"),
		TEXT("previews"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics,
		false);
	ValidateKnownFields(
		Previews,
		{ TEXT("front"), TEXT("side"), TEXT("back") },
		TEXT("previews"),
		Result.Manifest.ManifestRepositoryPath,
		Result.Diagnostics);
	if (Previews.IsValid())
	{
		for (const FString& Role : { TEXT("front"), TEXT("side"), TEXT("back") })
		{
			FString Path;
			if (Previews->TryGetStringField(Role, Path) && !Path.IsEmpty())
			{
				Path = NormalizeRepositoryPath(Path);
				if (!IsSafeBundleRelativePath(Path)
					|| !IsSupportedTextureExtension(LowerExtension(Path)))
				{
					AddDiagnostic(
						Result.Diagnostics,
						EWBCharacterDiagnosticSeverity::Error,
						TEXT("manifest_preview_path_invalid"),
						TEXT("Preview paths must be safe bundle-relative image paths."),
						Result.Manifest.ManifestRepositoryPath,
						TEXT("previews.") + Role,
						Path);
				}
				Result.Manifest.PreviewPaths.Add(Role, Path);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
	if (Root->TryGetArrayField(TEXT("tags"), Tags))
	{
		TSet<FString> SeenTags;
		for (const TSharedPtr<FJsonValue>& Tag : *Tags)
		{
			FString TagValue;
			if (!Tag.IsValid()
				|| Tag->Type != EJson::String
				|| !Tag->TryGetString(TagValue)
				|| TagValue.TrimStartAndEnd().IsEmpty()
				|| SeenTags.Contains(TagValue))
			{
				AddDiagnostic(
					Result.Diagnostics,
					EWBCharacterDiagnosticSeverity::Error,
					TEXT("manifest_tag_invalid"),
					TEXT("Tags must be unique, nonempty strings."),
					Result.Manifest.ManifestRepositoryPath,
					TEXT("tags"));
				continue;
			}
			SeenTags.Add(TagValue);
			Result.Manifest.Tags.Add(TagValue);
		}
		Result.Manifest.Tags.Sort();
	}
	Root->TryGetStringField(TEXT("notes"), Result.Manifest.Notes);

	const FString ExpectedManifestPrefix = TEXT("SourceAssets/Characters/");
	const FString NormalizedManifest = Result.Manifest.ManifestRepositoryPath;
	if (!IsSafeBundleRelativePath(NormalizedManifest)
		|| !NormalizedManifest.StartsWith(ExpectedManifestPrefix)
		|| !NormalizedManifest.EndsWith(TEXT("/character_manifest.json")))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_repository_path_invalid"),
			TEXT("Character manifests must be under SourceAssets/Characters/<character_id>/."),
			NormalizedManifest,
			TEXT("$"));
	}
	else
	{
		Result.Manifest.BundleRepositoryPath = FPaths::GetPath(NormalizedManifest);
		const FString PathCharacterId = FPaths::GetCleanFilename(Result.Manifest.BundleRepositoryPath);
		if (!Result.Manifest.CharacterId.IsEmpty()
			&& PathCharacterId != Result.Manifest.CharacterId)
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_character_directory_mismatch"),
				TEXT("The manifest character_id must exactly match its bundle directory."),
				NormalizedManifest,
				TEXT("character_id"));
		}
	}

	const FString AbsoluteProjectRoot = FPaths::ConvertRelativePathToFull(ProjectRoot);
	Result.Manifest.BundleAbsolutePath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(AbsoluteProjectRoot, Result.Manifest.BundleRepositoryPath));
	const FString AbsoluteManifest = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(AbsoluteProjectRoot, NormalizedManifest));
	const FString RootWithSlash = AbsoluteProjectRoot.EndsWith(TEXT("/"))
		|| AbsoluteProjectRoot.EndsWith(TEXT("\\"))
		? AbsoluteProjectRoot
		: AbsoluteProjectRoot + TEXT("/");
	if (!AbsoluteManifest.StartsWith(RootWithSlash, ESearchCase::IgnoreCase)
		|| !Result.Manifest.BundleAbsolutePath.StartsWith(RootWithSlash, ESearchCase::IgnoreCase))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_path_outside_repository"),
			TEXT("The resolved manifest or bundle path escapes the repository."),
			NormalizedManifest,
			TEXT("$"));
	}

	if (bRequireFiles && !IFileManager::Get().FileExists(*AbsoluteManifest))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_file_missing"),
			TEXT("The manifest file does not exist."),
			NormalizedManifest,
			TEXT("$"));
	}
	const FString AbsoluteModel = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(Result.Manifest.BundleAbsolutePath, Result.Manifest.Source.ModelPath));
	if (bRequireFiles && !IFileManager::Get().FileExists(*AbsoluteModel))
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_source_model_missing"),
			TEXT("The declared source model does not exist."),
			NormalizedManifest,
			TEXT("source.model"),
			Result.Manifest.Source.ModelPath);
	}

	if (bRequireFiles && IFileManager::Get().DirectoryExists(*Result.Manifest.BundleAbsolutePath))
	{
		TArray<FString> CandidateModels;
		IFileManager::Get().FindFilesRecursive(
			CandidateModels,
			*FPaths::Combine(Result.Manifest.BundleAbsolutePath, TEXT("model")),
			TEXT("*.*"),
			true,
			false);
		CandidateModels.RemoveAll([](const FString& Candidate)
		{
			const FString Extension = LowerExtension(Candidate);
			return Extension != TEXT(".fbx")
				&& Extension != TEXT(".glb")
				&& Extension != TEXT(".gltf");
		});
		if (CandidateModels.Num() > 1)
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("manifest_multiple_primary_models"),
				TEXT("A character bundle may contain only one primary model file."),
				NormalizedManifest,
				TEXT("source.model"));
		}
	}

	for (const FWBCharacterTextureDefinition& Texture : Result.Manifest.Source.Textures)
	{
		const FString Absolute = FPaths::Combine(Result.Manifest.BundleAbsolutePath, Texture.SourcePath);
		if (bRequireFiles && !IFileManager::Get().FileExists(*Absolute))
		{
			AddDiagnostic(
				Result.Diagnostics,
				Texture.bRequired
					? EWBCharacterDiagnosticSeverity::Error
					: EWBCharacterDiagnosticSeverity::Warning,
				Texture.bRequired
					? TEXT("manifest_required_texture_missing")
					: TEXT("manifest_optional_texture_missing"),
				TEXT("A declared texture file is missing."),
				NormalizedManifest,
				TEXT("source.textures"),
				Texture.SourcePath);
		}
	}
	for (const FWBCharacterAnimationDefinition& Animation : Result.Manifest.Source.Animations)
	{
		const FString Absolute = FPaths::Combine(Result.Manifest.BundleAbsolutePath, Animation.SourcePath);
		if (bRequireFiles && !IFileManager::Get().FileExists(*Absolute))
		{
			AddDiagnostic(
				Result.Diagnostics,
				Animation.bRequired
					? EWBCharacterDiagnosticSeverity::Error
					: EWBCharacterDiagnosticSeverity::Warning,
				Animation.bRequired
					? TEXT("manifest_required_animation_missing")
					: TEXT("manifest_optional_animation_missing"),
				TEXT("A declared animation file is missing."),
				NormalizedManifest,
				TEXT("source.animations"),
				Animation.SourcePath);
		}
	}
	for (const TPair<FString, FString>& Preview : Result.Manifest.PreviewPaths)
	{
		const FString Absolute = FPaths::Combine(Result.Manifest.BundleAbsolutePath, Preview.Value);
		if (bRequireFiles && !IFileManager::Get().FileExists(*Absolute))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Warning,
				TEXT("manifest_optional_preview_missing"),
				TEXT("A declared preview image is missing; import may continue."),
				NormalizedManifest,
				TEXT("previews.") + Preview.Key,
				Preview.Value);
		}
	}

	if (!Result.Manifest.CardDefinitionId.IsEmpty()
		&& CardDefinitionRepository == nullptr)
	{
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Warning,
			TEXT("card_definition_repository_unavailable"),
			TEXT("No production CardDB repository is loaded; the public definition ID is syntax-checked only."),
			NormalizedManifest,
			TEXT("card_definition_id"),
			FString(),
			TEXT("Validate the mapping again when a production CardDB bundle is available."));
	}
	else if (!Result.Manifest.CardDefinitionId.IsEmpty())
	{
		const FWBCardDefinitionRepositoryLookupResult Lookup =
			WBCardDefinitionRepository::FindCardById(
				*CardDefinitionRepository,
				Result.Manifest.CardDefinitionId);
		if (!Lookup.bFound)
		{
			AddDiagnostic(
				Result.Diagnostics,
				bRequireCardDefinition
					? EWBCharacterDiagnosticSeverity::Error
					: EWBCharacterDiagnosticSeverity::Warning,
				TEXT("card_definition_not_found"),
				TEXT("The character manifest public definition ID is not present in the selected CardDB snapshot."),
				NormalizedManifest,
				TEXT("card_definition_id"),
				FString(),
				TEXT("Choose an existing Character or Hero definition ID."));
		}
		else if (Lookup.Definition.Kind != EWBCardDefinitionKind::Character)
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("card_definition_kind_mismatch"),
				TEXT("Character models may bind only to Character or Hero definitions."),
				NormalizedManifest,
				TEXT("card_definition_id"),
				FString(),
				TEXT("Use a Character or Hero definition ID."));
		}
	}
	Result.Manifest.NormalizedJson = Json;

	Result.Diagnostics.Sort([](
		const FWBCharacterManifestDiagnostic& A,
		const FWBCharacterManifestDiagnostic& B)
	{
		if (A.Severity != B.Severity) return A.Severity > B.Severity;
		if (A.Code != B.Code) return A.Code < B.Code;
		if (A.FieldPath != B.FieldPath) return A.FieldPath < B.FieldPath;
		return A.SourceRelativePath < B.SourceRelativePath;
	});
	return Result;
}

FWBCharacterManifestValidationResult
WBCharacterModelPipeline::LoadAndValidateManifest(
	const FString& ProjectRoot,
	const FString& ManifestRepositoryPath,
	const FWBCardDefinitionRepository* CardDefinitionRepository,
	const bool bRequireCardDefinition)
{
	const FString NormalizedPath = NormalizeRepositoryPath(ManifestRepositoryPath);
	const FString AbsolutePath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(ProjectRoot, NormalizedPath));
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath))
	{
		FWBCharacterManifestValidationResult Result;
		AddDiagnostic(
			Result.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("manifest_file_read_failed"),
			TEXT("The character manifest could not be read."),
			NormalizedPath,
			TEXT("$"));
		return Result;
	}
	return ParseAndValidateManifestJson(
		Json,
		NormalizedPath,
		ProjectRoot,
		true,
		CardDefinitionRepository,
		bRequireCardDefinition);
}

FWBCharacterSourceInventory WBCharacterModelPipeline::BuildSourceInventory(
	const FString& ProjectRoot,
	const FWBCharacterModelManifest& Manifest)
{
	FWBCharacterSourceInventory Inventory;
	if (!IFileManager::Get().DirectoryExists(*Manifest.BundleAbsolutePath))
	{
		AddDiagnostic(
			Inventory.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("source_bundle_missing"),
			TEXT("The character source bundle directory does not exist."),
			Manifest.ManifestRepositoryPath);
		return Inventory;
	}

	TArray<FString> AbsoluteFiles;
	FString BundleRootForRelativization = Manifest.BundleAbsolutePath;
	FPaths::NormalizeDirectoryName(BundleRootForRelativization);
	BundleRootForRelativization += TEXT("/");
	IFileManager::Get().FindFilesRecursive(
		AbsoluteFiles,
		*Manifest.BundleAbsolutePath,
		TEXT("*"),
		true,
		false);
	AbsoluteFiles.Sort();
	for (const FString& AbsoluteFile : AbsoluteFiles)
	{
		FString RelativePath = AbsoluteFile;
		FPaths::MakePathRelativeTo(RelativePath, *BundleRootForRelativization);
		RelativePath = NormalizeRepositoryPath(RelativePath);

		FWBCharacterSourceInventoryEntry Entry;
		Entry.RelativePath = RelativePath;
		Entry.FileType = FileTypeForPath(RelativePath);
		Entry.SizeBytes = IFileManager::Get().FileSize(*AbsoluteFile);
		Entry.SHA256 = SHA256File(AbsoluteFile);
		Entry.DeclaredRole = RoleForInventoryPath(Manifest, RelativePath, Entry.bRequired);
		Entry.GitStatus = ClassifyRepositoryFile(
			ProjectRoot,
			NormalizeRepositoryPath(
				FPaths::Combine(Manifest.BundleRepositoryPath, RelativePath)));
		Inventory.Entries.Add(MoveTemp(Entry));
	}
	Inventory.Entries.Sort([](
		const FWBCharacterSourceInventoryEntry& A,
		const FWBCharacterSourceInventoryEntry& B)
	{
		return A.RelativePath < B.RelativePath;
	});
	FString DigestInput;
	for (const FWBCharacterSourceInventoryEntry& Entry : Inventory.Entries)
	{
		DigestInput += Entry.RelativePath + TEXT("|") + Entry.SHA256 + TEXT("\n");
		if (Entry.SHA256.IsEmpty())
		{
			AddDiagnostic(
				Inventory.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("source_hash_failed"),
				TEXT("SHA-256 hashing failed for a source-bundle file."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Entry.RelativePath);
		}
		if (Entry.DeclaredRole == TEXT("undeclared_optional"))
		{
			AddDiagnostic(
				Inventory.Diagnostics,
				EWBCharacterDiagnosticSeverity::Warning,
				TEXT("source_file_undeclared"),
				TEXT("The bundle contains an undeclared optional file."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Entry.RelativePath,
				TEXT("Declare its role or remove it from the bundle."));
		}
	}
	Inventory.InventoryHash = SHA256String(DigestInput);
	Inventory.Diagnostics.Sort([](
		const FWBCharacterManifestDiagnostic& A,
		const FWBCharacterManifestDiagnostic& B)
	{
		if (A.Code != B.Code) return A.Code < B.Code;
		return A.SourceRelativePath < B.SourceRelativePath;
	});
	return Inventory;
}

FWBCharacterDestinationPlan WBCharacterModelPipeline::BuildDestinationPlan(
	const FWBCharacterModelManifest& Manifest)
{
	FWBCharacterDestinationPlan Plan;
	Plan.CharacterId = Manifest.CharacterId;
	if (!IsSafeCharacterId(Manifest.CharacterId))
	{
		AddDiagnostic(
			Plan.Diagnostics,
			EWBCharacterDiagnosticSeverity::Error,
			TEXT("destination_character_id_invalid"),
			TEXT("Cannot build destination packages for an invalid character ID."));
		return Plan;
	}
	Plan.DestinationRoot = TEXT("/Game/Wandbound/Characters/") + Manifest.CharacterId;
	const FString Prefix = Manifest.Source.ModelType == EWBCharacterModelType::Skeletal
		? TEXT("SK_")
		: TEXT("SM_");
	Plan.PrimaryMeshPackage = FString::Printf(
		TEXT("%s/Meshes/%s%s"),
		*Plan.DestinationRoot,
		*Prefix,
		*Manifest.CharacterId);
	Plan.IntendedPackages.Add(Plan.PrimaryMeshPackage);
	if (Manifest.Source.ModelType == EWBCharacterModelType::Skeletal)
	{
		Plan.SkeletonPackage = FString::Printf(
			TEXT("%s/Skeleton/SKEL_%s"),
			*Plan.DestinationRoot,
			*Manifest.CharacterId);
		Plan.IntendedPackages.Add(Plan.SkeletonPackage);
		if (Manifest.Import.bCreatePhysicsAsset)
		{
			Plan.PhysicsPackage = FString::Printf(
				TEXT("%s/Physics/PHYS_%s"),
				*Plan.DestinationRoot,
				*Manifest.CharacterId);
			Plan.IntendedPackages.Add(Plan.PhysicsPackage);
		}
	}
	for (const FWBCharacterTextureDefinition& Texture : Manifest.Source.Textures)
	{
		const FString Package = FString::Printf(
			TEXT("%s/Textures/T_%s_%s"),
			*Plan.DestinationRoot,
			*Manifest.CharacterId,
			*SanitizePackageLeaf(Texture.Role));
		Plan.TexturePackages.Add(Texture.Role, Package);
		Plan.IntendedPackages.Add(Package);
	}
	for (const FWBCharacterAnimationDefinition& Animation : Manifest.Source.Animations)
	{
		const FString Package = FString::Printf(
			TEXT("%s/Animations/A_%s_%s"),
			*Plan.DestinationRoot,
			*Manifest.CharacterId,
			*SanitizePackageLeaf(Animation.Role));
		Plan.AnimationPackages.Add(Animation.Role, Package);
		Plan.IntendedPackages.Add(Package);
	}
	Plan.IntendedPackages.Sort();
	for (const FString& Package : Plan.IntendedPackages)
	{
		if (!FPackageName::IsValidLongPackageName(Package))
		{
			AddDiagnostic(
				Plan.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("destination_package_invalid"),
				TEXT("A deterministic destination package name is invalid."),
				Manifest.ManifestRepositoryPath,
				TEXT("$"),
				Package);
		}
	}
	return Plan;
}

TArray<FWBCharacterImportTaskSpec>
WBCharacterModelPipeline::BuildImportTaskSpecs(
	const FWBCharacterModelManifest& Manifest,
	const FWBCharacterDestinationPlan& Destination,
	const bool bReplaceExisting)
{
	TArray<FWBCharacterImportTaskSpec> Specs;
	FWBCharacterImportTaskSpec Mesh;
	Mesh.SourceAbsolutePath = FPaths::Combine(
		Manifest.BundleAbsolutePath,
		Manifest.Source.ModelPath);
	Mesh.DestinationPath = FPaths::GetPath(Destination.PrimaryMeshPackage);
	Mesh.DestinationName = FPaths::GetCleanFilename(Destination.PrimaryMeshPackage);
	Mesh.SemanticRole = TEXT("primary_model");
	Mesh.ExpectedClass = Manifest.Source.ModelType == EWBCharacterModelType::Skeletal
		? TEXT("SkeletalMesh")
		: TEXT("StaticMesh");
	Mesh.bReplaceExisting = bReplaceExisting;
	Mesh.bImportMaterials = Manifest.Import.bImportMaterials;
	Mesh.bImportTextures = Manifest.Import.bImportTextures;
	Mesh.bImportAnimations = Manifest.Import.bImportAnimations
		&& Manifest.Source.ModelType == EWBCharacterModelType::Skeletal;
	Mesh.bCreatePhysicsAsset = Manifest.Import.bCreatePhysicsAsset;
	Mesh.bGenerateCollision = Manifest.Import.bGenerateCollision;
	Specs.Add(MoveTemp(Mesh));

	for (const FWBCharacterTextureDefinition& Texture : Manifest.Source.Textures)
	{
		FWBCharacterImportTaskSpec Spec;
		Spec.SourceAbsolutePath = FPaths::Combine(
			Manifest.BundleAbsolutePath,
			Texture.SourcePath);
		const FString Package = Destination.TexturePackages[Texture.Role];
		Spec.DestinationPath = FPaths::GetPath(Package);
		Spec.DestinationName = FPaths::GetCleanFilename(Package);
		Spec.SemanticRole = TEXT("texture:") + Texture.Role;
		Spec.ExpectedClass = TEXT("Texture2D");
		Spec.bReplaceExisting = bReplaceExisting;
		Specs.Add(MoveTemp(Spec));
	}
	for (const FWBCharacterAnimationDefinition& Animation : Manifest.Source.Animations)
	{
		FWBCharacterImportTaskSpec Spec;
		Spec.SourceAbsolutePath = FPaths::Combine(
			Manifest.BundleAbsolutePath,
			Animation.SourcePath);
		const FString Package = Destination.AnimationPackages[Animation.Role];
		Spec.DestinationPath = FPaths::GetPath(Package);
		Spec.DestinationName = FPaths::GetCleanFilename(Package);
		Spec.SemanticRole = TEXT("animation:") + Animation.Role;
		Spec.ExpectedClass = TEXT("AnimSequence");
		Spec.bReplaceExisting = bReplaceExisting;
		Spec.bImportAnimations = true;
		Specs.Add(MoveTemp(Spec));
	}
	Specs.Sort([](
		const FWBCharacterImportTaskSpec& A,
		const FWBCharacterImportTaskSpec& B)
	{
		if (A.SemanticRole == TEXT("primary_model")) return true;
		if (B.SemanticRole == TEXT("primary_model")) return false;
		return A.SemanticRole < B.SemanticRole;
	});
	return Specs;
}

FString WBCharacterModelPipeline::BuildImportSettingsDigest(
	const FWBCharacterModelManifest& Manifest)
{
	FString Stable = FString::Printf(
		TEXT("v1|%s|%s|materials=%d|textures=%d|animations=%d|physics=%d|collision=%d|skeleton=%s|existing=%s|normal=%s|scale=%.9g|rotation=%.9g,%.9g,%.9g|offset=%.9g,%.9g,%.9g|facing=%s"),
		*SourceFormatName(Manifest.Source.Format),
		*ModelTypeName(Manifest.Source.ModelType),
		Manifest.Import.bImportMaterials,
		Manifest.Import.bImportTextures,
		Manifest.Import.bImportAnimations,
		Manifest.Import.bCreatePhysicsAsset,
		Manifest.Import.bGenerateCollision,
		*Manifest.Import.SkeletonPolicy,
		*Manifest.Import.ExistingSkeletonPackage,
		*Manifest.Import.NormalPolicy,
		Manifest.Presentation.Scale,
		Manifest.Presentation.Rotation.X,
		Manifest.Presentation.Rotation.Y,
		Manifest.Presentation.Rotation.Z,
		Manifest.Presentation.Offset.X,
		Manifest.Presentation.Offset.Y,
		Manifest.Presentation.Offset.Z,
		*Manifest.Presentation.FacingAxis);
	for (const FWBCharacterTextureDefinition& Texture : Manifest.Source.Textures)
	{
		Stable += TEXT("|texture:") + Texture.Role + TEXT("=") + Texture.SourcePath;
	}
	for (const FWBCharacterAnimationDefinition& Animation : Manifest.Source.Animations)
	{
		Stable += TEXT("|animation:") + Animation.Role + TEXT("=") + Animation.SourcePath;
	}
	return SHA256String(Stable);
}

EWBCharacterReimportState WBCharacterModelPipeline::DetermineReimportState(
	const FWBCharacterModelManifest& Manifest,
	const FWBCharacterSourceInventory& Inventory,
	const FWBCharacterDestinationPlan& Destination,
	const FWBCharacterImportReceipt* PreviousReceipt,
	const bool bDestinationsExist)
{
	if (PreviousReceipt == nullptr)
	{
		return EWBCharacterReimportState::NeverImported;
	}
	if (PreviousReceipt->LastResult != TEXT("success"))
	{
		return EWBCharacterReimportState::ImportFailed;
	}
	if (!bDestinationsExist)
	{
		return EWBCharacterReimportState::DestinationMissing;
	}
	if (PreviousReceipt->CharacterId != Manifest.CharacterId
		|| PreviousReceipt->ManifestSchemaVersion != Manifest.SchemaVersion)
	{
		return EWBCharacterReimportState::ManifestChanged;
	}
	if (PreviousReceipt->ImportSettingsDigest != BuildImportSettingsDigest(Manifest))
	{
		return EWBCharacterReimportState::SettingsChanged;
	}
	if (PreviousReceipt->SourceInventoryHash != Inventory.InventoryHash)
	{
		TMap<FString, FString> PreviousHashes;
		for (const FWBCharacterSourceInventoryEntry& Entry : PreviousReceipt->SourceEntries)
		{
			PreviousHashes.Add(Entry.RelativePath, Entry.SHA256);
		}
		for (const FWBCharacterSourceInventoryEntry& Entry : Inventory.Entries)
		{
			if (Entry.RelativePath == TEXT("character_manifest.json"))
			{
				continue;
			}
			const FString* PreviousHash = PreviousHashes.Find(Entry.RelativePath);
			if (PreviousHash == nullptr || *PreviousHash != Entry.SHA256)
			{
				return EWBCharacterReimportState::SourceChanged;
			}
		}
		return EWBCharacterReimportState::ManifestChanged;
	}
	for (const FString& Package : Destination.IntendedPackages)
	{
		if (!PreviousReceipt->DestinationPackages.Contains(Package))
		{
			return EWBCharacterReimportState::DependencyMissing;
		}
	}
	return EWBCharacterReimportState::UpToDate;
}

FWBCharacterPresentationCandidate WBCharacterModelPipeline::BuildPresentationCandidate(
	const FWBCharacterModelManifest& Manifest,
	const FWBCharacterDestinationPlan& Destination)
{
	FWBCharacterPresentationCandidate Candidate;
	Candidate.CharacterId = Manifest.CharacterId;
	Candidate.CardDefinitionId = Manifest.CardDefinitionId;
	Candidate.ModelType = ModelTypeName(Manifest.Source.ModelType);
	Candidate.PrimaryMeshPackage = Destination.PrimaryMeshPackage;
	Candidate.SkeletonPackage = Destination.SkeletonPackage;
	Candidate.Scale = Manifest.Presentation.Scale;
	Candidate.Rotation = Manifest.Presentation.Rotation;
	Candidate.Offset = Manifest.Presentation.Offset;
	Candidate.AnimationPackages = Destination.AnimationPackages;
	Candidate.ValidationStatus = Destination.IsValid() ? TEXT("candidate_ready") : TEXT("candidate_invalid");
	for (const FString& Role : {
		TEXT("idle"), TEXT("move"), TEXT("attack"), TEXT("hit"),
		TEXT("summon"), TEXT("death"), TEXT("activation") })
	{
		if (!Candidate.AnimationPackages.Contains(Role))
		{
			Candidate.FallbackRequirements.Add(Role);
		}
	}
	return Candidate;
}

FWBCharacterCookVerificationResult
WBCharacterModelPipeline::ValidateCookPackageList(
	const FWBCharacterCookVerificationRequest& Request)
{
	FWBCharacterCookVerificationResult Result;
	TArray<FString> Sorted = Request.ExactPackages;
	Sorted.Sort();
	for (const FString& Package : Sorted)
	{
		const FString Lower = Package.ToLower();
		if (!FPackageName::IsValidLongPackageName(Package)
			|| !Package.StartsWith(TEXT("/Game/")))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				TEXT("cook_package_invalid"),
				TEXT("Cook verification accepts only valid /Game package names."),
				FString(),
				TEXT("$"),
				Package);
		}
		else if (Lower.Contains(TEXT("sourceassets"))
			|| Lower.Contains(TEXT("/meshy"))
			|| Lower.Contains(TEXT("godot"))
			|| Lower.StartsWith(TEXT("/script/"))
			|| Lower.Contains(TEXT("/editor")))
		{
			AddDiagnostic(
				Result.Diagnostics,
				EWBCharacterDiagnosticSeverity::Error,
				Lower.Contains(TEXT("meshy"))
					? TEXT("cook_unexpected_meshy_package")
					: Lower.Contains(TEXT("godot"))
						? TEXT("cook_godot_path_rejected")
						: TEXT("cook_editor_or_source_package_rejected"),
				TEXT("The package is not allowed in an imported-character cook list."),
				FString(),
				TEXT("$"),
				Package);
		}
		else
		{
			Result.AcceptedPackages.Add(Package);
		}
	}
	Result.bValid = !HasError(Result.Diagnostics);
	return Result;
}

EWBCharacterGitStatus WBCharacterModelPipeline::ClassifyRepositoryFile(
	const FString& ProjectRoot,
	const FString& RepositoryPath)
{
	const FString Normalized = NormalizeRepositoryPath(RepositoryPath);
	if (Normalized.StartsWith(TEXT("/Engine/")))
	{
		return EWBCharacterGitStatus::EngineAsset;
	}
	if (!IsSafeBundleRelativePath(Normalized))
	{
		return EWBCharacterGitStatus::OutsideRepository;
	}
	const FString Absolute = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(ProjectRoot, Normalized));
	if (!IFileManager::Get().FileExists(*Absolute))
	{
		return EWBCharacterGitStatus::Missing;
	}

	FString Output;
	int32 Code = 1;
	RunGit(
		ProjectRoot,
		FString::Printf(TEXT("check-ignore -q -- \"%s\""), *Normalized),
		Output,
		Code);
	if (Code == 0)
	{
		return EWBCharacterGitStatus::Ignored;
	}
	RunGit(
		ProjectRoot,
		FString::Printf(TEXT("ls-files --error-unmatch -- \"%s\""), *Normalized),
		Output,
		Code);
	if (Code != 0)
	{
		return EWBCharacterGitStatus::Untracked;
	}
	RunGit(
		ProjectRoot,
		FString::Printf(TEXT("check-attr filter -- \"%s\""), *Normalized),
		Output,
		Code);
	return Code == 0 && Output.Contains(TEXT("filter: lfs"))
		? EWBCharacterGitStatus::TrackedLFS
		: EWBCharacterGitStatus::TrackedGit;
}

FString WBCharacterModelPipeline::GitStatusName(const EWBCharacterGitStatus Status)
{
	switch (Status)
	{
	case EWBCharacterGitStatus::TrackedGit: return TEXT("TrackedGit");
	case EWBCharacterGitStatus::TrackedLFS: return TEXT("TrackedLFS");
	case EWBCharacterGitStatus::Untracked: return TEXT("Untracked");
	case EWBCharacterGitStatus::Ignored: return TEXT("Ignored");
	case EWBCharacterGitStatus::Missing: return TEXT("Missing");
	case EWBCharacterGitStatus::OutsideRepository: return TEXT("OutsideRepository");
	case EWBCharacterGitStatus::EngineAsset: return TEXT("EngineAsset");
	default: return TEXT("Unknown");
	}
}

FString WBCharacterModelPipeline::ReimportStateName(const EWBCharacterReimportState State)
{
	switch (State)
	{
	case EWBCharacterReimportState::NeverImported: return TEXT("NeverImported");
	case EWBCharacterReimportState::UpToDate: return TEXT("UpToDate");
	case EWBCharacterReimportState::SourceChanged: return TEXT("SourceChanged");
	case EWBCharacterReimportState::ManifestChanged: return TEXT("ManifestChanged");
	case EWBCharacterReimportState::SettingsChanged: return TEXT("SettingsChanged");
	case EWBCharacterReimportState::DestinationMissing: return TEXT("DestinationMissing");
	case EWBCharacterReimportState::DependencyMissing: return TEXT("DependencyMissing");
	case EWBCharacterReimportState::ReimportRequired: return TEXT("ReimportRequired");
	case EWBCharacterReimportState::ImportFailed: return TEXT("ImportFailed");
	default: return TEXT("Unknown");
	}
}

FString WBCharacterModelPipeline::InventoryToJson(
	const FWBCharacterSourceInventory& Inventory)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schema_version"), 1);
	Writer->WriteValue(TEXT("inventory_hash"), Inventory.InventoryHash);
	Writer->WriteArrayStart(TEXT("entries"));
	for (const FWBCharacterSourceInventoryEntry& Entry : Inventory.Entries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("relative_path"), Entry.RelativePath);
		Writer->WriteValue(TEXT("file_type"), Entry.FileType);
		Writer->WriteValue(TEXT("size_bytes"), Entry.SizeBytes);
		Writer->WriteValue(TEXT("sha256"), Entry.SHA256);
		Writer->WriteValue(TEXT("declared_role"), Entry.DeclaredRole);
		Writer->WriteValue(TEXT("required"), Entry.bRequired);
		Writer->WriteValue(TEXT("git_status"), GitStatusName(Entry.GitStatus));
		const bool bLFSBinary =
			Entry.FileType == TEXT("fbx")
			|| Entry.FileType == TEXT("glb")
			|| Entry.FileType == TEXT("gltf")
			|| Entry.FileType == TEXT("image");
		Writer->WriteValue(
			TEXT("lfs_status"),
			Entry.GitStatus == EWBCharacterGitStatus::TrackedLFS
				? TEXT("covered")
				: bLFSBinary
					? TEXT("not_covered")
					: TEXT("not_applicable"));
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteArrayStart(TEXT("diagnostics"));
	for (const FWBCharacterManifestDiagnostic& Diagnostic : Inventory.Diagnostics)
	{
		WriteDiagnostic(Writer, Diagnostic);
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

FString WBCharacterModelPipeline::ReceiptToJson(
	const FWBCharacterImportReceipt& Receipt)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("receipt_schema_version"), Receipt.ReceiptSchemaVersion);
	Writer->WriteValue(TEXT("manifest_schema_version"), Receipt.ManifestSchemaVersion);
	Writer->WriteValue(TEXT("character_id"), Receipt.CharacterId);
	Writer->WriteValue(TEXT("manifest_hash"), Receipt.ManifestHash);
	Writer->WriteValue(TEXT("source_inventory_hash"), Receipt.SourceInventoryHash);
	Writer->WriteValue(TEXT("import_settings_digest"), Receipt.ImportSettingsDigest);
	Writer->WriteValue(TEXT("importer_version"), Receipt.ImporterVersion);
	Writer->WriteValue(TEXT("engine_version"), Receipt.EngineVersion);
	WriteStringArray(Writer, TEXT("destination_packages"), Receipt.DestinationPackages);
	Writer->WriteValue(TEXT("last_result"), Receipt.LastResult);
	Writer->WriteValue(TEXT("audit_timestamp_utc"), Receipt.AuditTimestampUtc);
	Writer->WriteArrayStart(TEXT("source_entries"));
	for (const FWBCharacterSourceInventoryEntry& Entry : Receipt.SourceEntries)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("relative_path"), Entry.RelativePath);
		Writer->WriteValue(TEXT("sha256"), Entry.SHA256);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}

bool WBCharacterModelPipeline::LoadReceipt(
	const FString& AbsolutePath,
	FWBCharacterImportReceipt& OutReceipt)
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *AbsolutePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
		|| !Root.IsValid())
	{
		return false;
	}
	double Number = 0;
	if (Root->TryGetNumberField(TEXT("receipt_schema_version"), Number))
		OutReceipt.ReceiptSchemaVersion = static_cast<int32>(Number);
	if (Root->TryGetNumberField(TEXT("manifest_schema_version"), Number))
		OutReceipt.ManifestSchemaVersion = static_cast<int32>(Number);
	Root->TryGetStringField(TEXT("character_id"), OutReceipt.CharacterId);
	Root->TryGetStringField(TEXT("manifest_hash"), OutReceipt.ManifestHash);
	Root->TryGetStringField(TEXT("source_inventory_hash"), OutReceipt.SourceInventoryHash);
	Root->TryGetStringField(TEXT("import_settings_digest"), OutReceipt.ImportSettingsDigest);
	Root->TryGetStringField(TEXT("importer_version"), OutReceipt.ImporterVersion);
	Root->TryGetStringField(TEXT("engine_version"), OutReceipt.EngineVersion);
	Root->TryGetStringField(TEXT("last_result"), OutReceipt.LastResult);
	Root->TryGetStringField(TEXT("audit_timestamp_utc"), OutReceipt.AuditTimestampUtc);
	const TArray<TSharedPtr<FJsonValue>>* Packages = nullptr;
	if (Root->TryGetArrayField(TEXT("destination_packages"), Packages))
	{
		for (const TSharedPtr<FJsonValue>& Package : *Packages)
			OutReceipt.DestinationPackages.Add(Package->AsString());
	}
	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (Root->TryGetArrayField(TEXT("source_entries"), Entries))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Entries)
		{
			const TSharedPtr<FJsonObject> EntryObject = Value->AsObject();
			FWBCharacterSourceInventoryEntry Entry;
			if (EntryObject.IsValid())
			{
				EntryObject->TryGetStringField(TEXT("relative_path"), Entry.RelativePath);
				EntryObject->TryGetStringField(TEXT("sha256"), Entry.SHA256);
				OutReceipt.SourceEntries.Add(MoveTemp(Entry));
			}
		}
	}
	return !OutReceipt.CharacterId.IsEmpty();
}

FString WBCharacterModelPipeline::PresentationCandidateToJson(
	const FWBCharacterPresentationCandidate& Candidate)
{
	FString Output;
	const TSharedRef<FCondensedWriter> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("schema_version"), 1);
	Writer->WriteValue(TEXT("character_id"), Candidate.CharacterId);
	Writer->WriteValue(TEXT("card_definition_id"), Candidate.CardDefinitionId);
	Writer->WriteValue(TEXT("model_type"), Candidate.ModelType);
	Writer->WriteValue(TEXT("primary_mesh_package"), Candidate.PrimaryMeshPackage);
	Writer->WriteValue(TEXT("skeleton_package"), Candidate.SkeletonPackage);
	Writer->WriteValue(TEXT("scale"), Candidate.Scale);
	Writer->WriteArrayStart(TEXT("rotation"));
	Writer->WriteValue(Candidate.Rotation.X);
	Writer->WriteValue(Candidate.Rotation.Y);
	Writer->WriteValue(Candidate.Rotation.Z);
	Writer->WriteArrayEnd();
	Writer->WriteArrayStart(TEXT("offset"));
	Writer->WriteValue(Candidate.Offset.X);
	Writer->WriteValue(Candidate.Offset.Y);
	Writer->WriteValue(Candidate.Offset.Z);
	Writer->WriteArrayEnd();
	WriteStringMap(Writer, TEXT("animation_packages"), Candidate.AnimationPackages);
	WriteStringArray(Writer, TEXT("material_packages"), Candidate.MaterialPackages);
	Writer->WriteValue(TEXT("validation_status"), Candidate.ValidationStatus);
	WriteStringArray(Writer, TEXT("fallback_requirements"), Candidate.FallbackRequirements);
	Writer->WriteValue(TEXT("applies_to_starter_asset_automatically"), false);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Output;
}
