/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "PSGCPWidgetBlueprintLibrary.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Misc/FileHelper.h"
#include "JsonUtilities.h"
#include "Misc/Paths.h"
#include "BLambdaRunnable.h"
#include "BZipFile.h"
#include "Runtime/Online/HTTP/Public/Http.h"

#define SAVE_FILE_PATH FPaths::ProjectPluginsDir() + "BPixelStreamingGCP/Saved/LastPSGCProjectInfo.json"

#define B_UNREAL_PS_PLUGIN_PROCESSOR_URL "https://storage.googleapis.com/{{BUCKET_NAME}}/releases/ps_unreal_plugin_processor.zip"
#define B_UNREAL_PS_PLUGIN_PROCESSOR_LOCAL_RELATIVE_PATH FPaths::ProjectPluginsDir() + "BPixelStreamingGCP/Saved/ps_unreal_plugin_processor.zip"
#define B_UNREAL_PS_PLUGIN_PROCESSOR_EXTRACT_FOLDER_LOCAL_RELATIVE_PATH FPaths::ProjectPluginsDir() + "BPixelStreamingGCP/Saved/ps_unreal_plugin_processor"
#define B_UNREAL_PS_PLUGIN_PROCESSOR_EXE_LOCAL_RELATIVE_PATH_UPON_EXTRACT FPaths::ProjectPluginsDir() + "BPixelStreamingGCP/Saved/ps_unreal_plugin_processor/PixelStreamingUnrealEditorPluginProcessor.exe"

#define B_UNREAL_PACKAGED_PS_APPLICATION_ZIP_LOCAL_RELATIVE_PATH FPaths::ProjectPluginsDir() + "BPixelStreamingGCP/Saved/ps_unreal_packaged_application.zip"

void UPSGCPWidgetBlueprintLibrary::SelectPackageDirectory(const FPSGCPSelectPackageDirectoryResult& Callback)
{
	FString OutSelectedDirectoryRelativePath;
	
	if (FDesktopPlatformModule::Get()->OpenDirectoryDialog(
		FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle(),
		TEXT("Select a folder containing CAD files"),
		FPaths::GetProjectFilePath(),
		OutSelectedDirectoryRelativePath))
	{
		FString OutSelectedDirectoryAbsolutePath = FPaths::ConvertRelativePathToFull(OutSelectedDirectoryRelativePath);
		OutSelectedDirectoryAbsolutePath.RemoveFromEnd("/");
		OutSelectedDirectoryAbsolutePath.RemoveFromEnd("\\");
		Callback.ExecuteIfBound(true, OutSelectedDirectoryAbsolutePath);
	}
	else
	{
		Callback.ExecuteIfBound(false, "");
	}
}

void UPSGCPWidgetBlueprintLibrary::SaveGCProjectInfoToSavedPath(const FString& ProjectID, const FString& BucketName, const FString& PlainCredentials, const FString& UniqueAppName, const FString& VMZone, const FString& GPUName)
{
	static const FString SavePath = SAVE_FILE_PATH;
	
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	TSharedPtr<FJsonObject> GCProjectInfoJsonObject = MakeShareable(new FJsonObject);

	GCProjectInfoJsonObject->SetStringField("projectId", ProjectID);
	GCProjectInfoJsonObject->SetStringField("bucketName", BucketName);
	GCProjectInfoJsonObject->SetStringField("plainCredentials", PlainCredentials);
	GCProjectInfoJsonObject->SetStringField("uniqueAppName", UniqueAppName);
	GCProjectInfoJsonObject->SetStringField("vmZone", VMZone);
	GCProjectInfoJsonObject->SetStringField("gpuName", GPUName);
	JsonObject->SetObjectField("gc_project_info", GCProjectInfoJsonObject);

	FString OutputString;
	auto Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	FFileHelper::SaveStringToFile(OutputString, *SavePath);
}

bool UPSGCPWidgetBlueprintLibrary::TryLoadingGCProjectInfoFromSavedPath(FString& ProjectID, FString& BucketName, FString& PlainCredentials, FString& UniqueAppName, FString& VMZone, FString& GPUName)
{
	static const FString SavePath = SAVE_FILE_PATH;

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*SavePath))
	{
		return false;
	}

	FString JsonString;
	FFileHelper::LoadFileToString(JsonString, *SavePath);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);

	const TSharedPtr<FJsonObject>* GCProjectInfoJsonObject;

	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid()
		|| !JsonObject->TryGetObjectField("gc_project_info", GCProjectInfoJsonObject)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("projectId", ProjectID)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("bucketName", BucketName)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("plainCredentials", PlainCredentials)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("uniqueAppName", UniqueAppName)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("vmZone", VMZone)
		|| !(*GCProjectInfoJsonObject)->TryGetStringField("gpuName", GPUName))
	{
		FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*SavePath);
		return false;
	}
	return true;
}

FString UPSGCPWidgetBlueprintLibrary::HexEncode(const FString& Input)
{
	return BytesToHex((uint8*)TCHAR_TO_UTF8(*Input), Input.Len());
}

bool UPSGCPWidgetBlueprintLibrary::CreateHiddenProcess(
	FProcessHandleWrapper& ProcessHandle, 
	FString ProgramAbsolutePath, 
	TArray<FString> CommandlineArgs, 
	FString& ReadMessage, 
	int32& ExitCode,
	PS_GCP_PROCESS_EXEC& Exec,
	FLatentActionInfo LatentInfo)
{
	bool* DoneIf = new bool(false);
	bool* TriggerUndoneIf = new bool(false);

	FString* ReadMessagePtr = &ReadMessage;
	int32* ExitCodePtr = &ExitCode;
	PS_GCP_PROCESS_EXEC* ExecPtr = &Exec;

	PrepareLatentBPExecAction(DoneIf, TriggerUndoneIf, LatentInfo);

	FString Args = "";
	for (int32 i = 0; i < CommandlineArgs.Num(); i++)
	{
		Args += "\"" + CommandlineArgs[i] + "\" ";
	}
	Args = Args.TrimEnd();

	uint32 UProcessID = 0;

	verify(FPlatformProcess::CreatePipe(ProcessHandle.ReadPipe, ProcessHandle.WritePipe));

	ProcessHandle.ProcessHandle = FPlatformProcess::CreateProc(
		*ProgramAbsolutePath,
		*Args,
		false,
		true,
		true,
		&UProcessID,
		0,
		nullptr,
		ProcessHandle.WritePipe,
		ProcessHandle.ReadPipe
	);

	ProcessHandle.ProcessID = UProcessID;
	ProcessHandle.OnProcessReadMessage = &ReadMessage;

	if (FPlatformProcess::IsProcRunning((FProcHandle&)ProcessHandle.ProcessHandle))
	{
		FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([ProcessHandle, DoneIf, TriggerUndoneIf, ReadMessagePtr, ExitCodePtr, ExecPtr]()
			{
				while (FPlatformProcess::IsProcRunning((FProcHandle&)ProcessHandle.ProcessHandle))
				{
					TArray<uint8> BinaryData;
					FPlatformProcess::ReadPipeToArray(ProcessHandle.ReadPipe, BinaryData);
					if (BinaryData.Num() > 0)
					{
						FString Stringified = FString(UTF8_TO_TCHAR(BinaryData.GetData()));
						FBLambdaRunnable::RunLambdaOnGameThread([ProcessHandle, Stringified, TriggerUndoneIf, ReadMessagePtr, ExecPtr]()
							{
								*ExecPtr = PS_GCP_PROCESS_EXEC::DataAvailable;
								*ReadMessagePtr = Stringified;
								*TriggerUndoneIf = true;
							});
					}
				}
				FBLambdaRunnable::RunLambdaOnGameThread([ProcessHandle, DoneIf, ExitCodePtr, ExecPtr]()
					{
						if (UWorld* EdWorld = GEditor->GetEditorWorldContext().World())
						{
							if (EdWorld->IsValidLowLevel() && !EdWorld->IsPendingKillOrUnreachable())
							{
								FTimerHandle TimerHandle;
								FTimerDelegate TimerDelegate;

								TimerDelegate.BindLambda([ProcessHandle, DoneIf, ExitCodePtr, ExecPtr]()
									{
										*ExecPtr = PS_GCP_PROCESS_EXEC::ProcessExited;
										if (!FPlatformProcess::GetProcReturnCode((FProcHandle&)ProcessHandle.ProcessHandle, ExitCodePtr))
										{
											*ExitCodePtr = -1;
										}
										*DoneIf = true;
									});
								EdWorld->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.5f, false);
							}
						}
					});
			});

		return true;
	}
	return false;
}

bool UPSGCPWidgetBlueprintLibrary::KillCloseHiddenProcess(const FProcessHandleWrapper& ProcessHandle)
{
	if (ProcessHandle.ProcessHandle.IsValid())
	{
		FPlatformProcess::ClosePipe(ProcessHandle.ReadPipe, ProcessHandle.WritePipe);
		FPlatformProcess::TerminateProc((FProcHandle&)ProcessHandle.ProcessHandle, true);
		FPlatformProcess::CloseProc((FProcHandle&)ProcessHandle.ProcessHandle);
		return true;
	}
	return false;
}

bool UPSGCPWidgetBlueprintLibrary::DownloadBUnrealPSPluginProcessor(const FString& GC_BucketName, FString& ProgramAbsolutePath, FString& ErrorMessage, PS_GCP_SUCCESS_FAIL_OUT_EXEC& Exec, FLatentActionInfo LatentInfo)
{
	bool* DoneIf = new bool(false);

	FString* ProgramAbsolutePathPtr = &ProgramAbsolutePath;
	FString* ErrorMessagePtr = &ErrorMessage;
	PS_GCP_SUCCESS_FAIL_OUT_EXEC* ExecPtr = &Exec;

	PrepareLatentBPExecAction(DoneIf, nullptr, LatentInfo);

	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb("GET");
	HttpRequest->SetURL(FString(B_UNREAL_PS_PLUGIN_PROCESSOR_URL).Replace(TEXT("{{BUCKET_NAME}}"), *FString::Printf(TEXT("%s"), *GC_BucketName), ESearchCase::CaseSensitive));
	HttpRequest->OnProcessRequestComplete().BindLambda([DoneIf, ProgramAbsolutePathPtr, ErrorMessagePtr, ExecPtr]
		(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!bConnectedSuccessfully)
			{
				UE_LOG(LogTemp, Error, TEXT("UPSGCPWidgetBlueprintLibrary::DownloadBUnrealPSPluginProcessor::OnProcessRequestComplete: Callback is invalid."));
				return;
			}

			if (!Response.IsValid())
			{
				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = "Response is invalid.";
				*DoneIf = true;
				return;
			}

			if (Response->GetResponseCode() >= 400)
			{
				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = FString::Printf(TEXT("Request returned %d"), Response->GetResponseCode());
				*DoneIf = true;
				return;
			}

			FString LocalZipRelativePath = FString(B_UNREAL_PS_PLUGIN_PROCESSOR_LOCAL_RELATIVE_PATH);
			FString LocalZipAbsolutePath = FPaths::ConvertRelativePathToFull(LocalZipRelativePath);

			if (IFileManager::Get().FileExists(*LocalZipRelativePath))
				IFileManager::Get().Delete(*LocalZipRelativePath);

			if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *LocalZipRelativePath)
				|| !FPaths::FileExists(LocalZipRelativePath))
			{
				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = "Failed to save the file.";
				*DoneIf = true;
				return;
			}

			FString ExtractFolderRelativePath = FString(B_UNREAL_PS_PLUGIN_PROCESSOR_EXTRACT_FOLDER_LOCAL_RELATIVE_PATH);
			FString ExtractFolderAbsolutePath = FPaths::ConvertRelativePathToFull(ExtractFolderRelativePath);

			FString ErrorMessage;
			TSharedPtr<BZipArchive> Archive;

			if (IFileManager::Get().DirectoryExists(*ExtractFolderRelativePath))
				IFileManager::Get().DeleteDirectory(*ExtractFolderRelativePath);

			if (!BZipFile::ExtractAll(LocalZipAbsolutePath, ExtractFolderAbsolutePath, ErrorMessage))
			{
				if (IFileManager::Get().DirectoryExists(*ExtractFolderRelativePath))
					IFileManager::Get().DeleteDirectory(*ExtractFolderRelativePath);
				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = "Zip extraction has failed.";
				*DoneIf = true;
				return;
			}

			FString ExeRelativePath = FString(B_UNREAL_PS_PLUGIN_PROCESSOR_EXE_LOCAL_RELATIVE_PATH_UPON_EXTRACT);
			FString ExeAbsolutePath = FPaths::ConvertRelativePathToFull(ExeRelativePath);

			if (!IFileManager::Get().FileExists(*ExeRelativePath))
			{
				if (IFileManager::Get().DirectoryExists(*ExtractFolderRelativePath))
					IFileManager::Get().DeleteDirectory(*ExtractFolderRelativePath);

				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = "Zip file has been downloaded, extracted; but the exe file could not be found.";
				*DoneIf = true;
				return;
			}

			*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Succeed;
			*ProgramAbsolutePathPtr = ExeAbsolutePath;
			*DoneIf = true;
		});
	return HttpRequest->ProcessRequest();
}

void UPSGCPWidgetBlueprintLibrary::ZipPackagedApplicationFolder(const FString& PackagedApplicationFolderAbsolutePath, FString& CompressedZipAbsolutePath, FString& ErrorMessage, PS_GCP_SUCCESS_FAIL_OUT_EXEC& Exec, FLatentActionInfo LatentInfo)
{
	bool* DoneIf = new bool(false);

	FString* ErrorMessagePtr = &ErrorMessage;
	FString* CompressedZipAbsolutePathPtr = &CompressedZipAbsolutePath;
	PS_GCP_SUCCESS_FAIL_OUT_EXEC* ExecPtr = &Exec;

	PrepareLatentBPExecAction(DoneIf, nullptr, LatentInfo);

	FBLambdaRunnable::RunLambdaOnDedicatedBackgroundThread([PackagedApplicationFolderAbsolutePath, DoneIf, CompressedZipAbsolutePathPtr, ErrorMessagePtr, ExecPtr]()
		{
			if (!IFileManager::Get().DirectoryExists(*PackagedApplicationFolderAbsolutePath))
			{
				FBLambdaRunnable::RunLambdaOnGameThread([PackagedApplicationFolderAbsolutePath, DoneIf, ErrorMessagePtr, ExecPtr]()
					{
						*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
						*ErrorMessagePtr = FString::Printf(TEXT("Directory does not exist at %s"), *PackagedApplicationFolderAbsolutePath);
						*DoneIf = true;
					});
				return;
			}

			FString LocalZipRelativePath = FString(B_UNREAL_PACKAGED_PS_APPLICATION_ZIP_LOCAL_RELATIVE_PATH);
			FString LocalZipAbsolutePath = FPaths::ConvertRelativePathToFull(LocalZipRelativePath);

			if (IFileManager::Get().FileExists(*LocalZipRelativePath))
				IFileManager::Get().Delete(*LocalZipRelativePath);

			FString TmpErrorMessage;
			
			if (!BZipFile::CompressAll(PackagedApplicationFolderAbsolutePath, LocalZipAbsolutePath, TmpErrorMessage))
			{
				if (IFileManager::Get().FileExists(*LocalZipRelativePath))
					IFileManager::Get().Delete(*LocalZipRelativePath);
				*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Failed;
				*ErrorMessagePtr = TmpErrorMessage;
				*DoneIf = true;
				return;
			}

			*CompressedZipAbsolutePathPtr = LocalZipAbsolutePath;
			*ExecPtr = PS_GCP_SUCCESS_FAIL_OUT_EXEC::Succeed;
			*DoneIf = true;
		});
}

void UPSGCPWidgetBlueprintLibrary::PrepareLatentBPExecAction(bool* InDoneIf, bool* InTriggerUndoneIf, FLatentActionInfo& LatentInfo)
{
	if (UWorld* World = GEditor->GetEditorWorldContext().World())
	{
		if (World->IsValidLowLevel() && !World->IsPendingKillOrUnreachable())
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

			if (FPSGCPLatentAction_Internal* ExistingAction = LatentActionManager.FindExistingAction<FPSGCPLatentAction_Internal>(LatentInfo.CallbackTarget, LatentInfo.UUID))
			{
				ExistingAction->Initialize(InDoneIf, InTriggerUndoneIf, LatentInfo);
			}
			else
			{
				auto NewAction = new FPSGCPLatentAction_Internal();
				NewAction->Initialize(InDoneIf, InTriggerUndoneIf, LatentInfo);
				LatentActionManager.AddNewAction(
					LatentInfo.CallbackTarget,
					LatentInfo.UUID,
					NewAction);
			}
		}
	}
}