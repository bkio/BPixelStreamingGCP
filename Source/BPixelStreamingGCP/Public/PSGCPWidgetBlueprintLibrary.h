/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Runtime/Engine/Public/LatentActions.h"
#include "PSGCPWidgetBlueprintLibrary.generated.h"

USTRUCT(BlueprintType)
struct BPIXELSTREAMINGGCP_API FProcessHandleWrapper
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Google Cloud Pixel Streaming")
	int32 ProcessID;

	FString* OnProcessReadMessage;

	struct FProcHandle ProcessHandle;

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
};

class FPSGCPLatentAction_Internal : public FPendingLatentAction
{
public:
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	bool* DoneIf;
	bool* TriggerUndoneIf;

	void Initialize(bool* InDoneIf, bool* InTriggerUndoneIf, const FLatentActionInfo& InLatentInfo)
	{
		ExecutionFunction = InLatentInfo.ExecutionFunction;
		OutputLink = InLatentInfo.Linkage;
		CallbackTarget = InLatentInfo.CallbackTarget;
		DoneIf = InDoneIf;
		TriggerUndoneIf = InTriggerUndoneIf;
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		if (DoneIf && (*DoneIf) == true)
		{
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
			delete DoneIf;

			if (TriggerUndoneIf)
				delete TriggerUndoneIf;
		}
		else if (TriggerUndoneIf && (*TriggerUndoneIf) == true)
		{
			*TriggerUndoneIf = false;
			Response.TriggerLink(ExecutionFunction, OutputLink, CallbackTarget);
		}
	}
};

UENUM(BlueprintType)
enum class PS_GCP_SUCCESS_FAIL_OUT_EXEC : uint8
{
	Succeed = 0,
	Failed = 1
};

UENUM(BlueprintType)
enum class PS_GCP_PROCESS_EXEC : uint8
{
	DataAvailable = 0,
	ProcessExited = 1
};

UCLASS()
class BPIXELSTREAMINGGCP_API UPSGCPWidgetBlueprintLibrary : public UWidgetBlueprintLibrary
{
	GENERATED_BODY()
	
public:
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FPSGCPSelectPackageDirectoryResult, bool, bResult, FString, Path);
	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming")
	static void SelectPackageDirectory(const FPSGCPSelectPackageDirectoryResult& Callback);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming")
	static void SaveGCProjectInfoToSavedPath(const FString& ProjectID, const FString& BucketName, const FString& PlainCredentials, const FString& UniqueAppName, const FString& VMZone, const FString& GPUName);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming")
	static bool TryLoadingGCProjectInfoFromSavedPath(FString& ProjectID, FString& BucketName, FString& PlainCredentials, FString& UniqueAppName, FString& VMZone, FString& GPUName);

	UFUNCTION(BlueprintPure, Category = "Google Cloud Pixel Streaming")
	static FString HexEncode(const FString& Input);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming", meta = (ExpandEnumAsExecs = "Exec", Latent, LatentInfo = "LatentInfo"))
	static bool CreateHiddenProcess(FProcessHandleWrapper& ProcessHandle, FString ProgramAbsolutePath, TArray<FString> CommandlineArgs, FString& ReadMessage, int32& ExitCode, PS_GCP_PROCESS_EXEC& Exec, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming")
	static bool KillCloseHiddenProcess(const FProcessHandleWrapper& ProcessHandle);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming", meta = (ExpandEnumAsExecs = "Exec", Latent, LatentInfo = "LatentInfo"))
	static bool DownloadBUnrealPSPluginProcessor(const FString& GC_BucketName, FString& ProgramAbsolutePath, FString& ErrorMessage, PS_GCP_SUCCESS_FAIL_OUT_EXEC& Exec, FLatentActionInfo LatentInfo);

	UFUNCTION(BlueprintCallable, Category = "Google Cloud Pixel Streaming", meta = (ExpandEnumAsExecs = "Exec", Latent, LatentInfo = "LatentInfo"))
	static void ZipPackagedApplicationFolder(const FString& PackagedApplicationFolderAbsolutePath, FString& CompressedZipAbsolutePath, FString& ErrorMessage, PS_GCP_SUCCESS_FAIL_OUT_EXEC& Exec, FLatentActionInfo LatentInfo);

private:
	static void PrepareLatentBPExecAction(bool* InDoneIf, bool* InTriggerUndoneIf, FLatentActionInfo& LatentInfo);
};