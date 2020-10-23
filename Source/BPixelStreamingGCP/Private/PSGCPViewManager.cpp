/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "PSGCPViewManager.h"
#include "UObject/ConstructorHelpers.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilitySubsystem.h"
#include "IBlutilityModule.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "EditorUtilitySubsystem"

UPSGCPViewManager::UPSGCPViewManager()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;

	static ConstructorHelpers::FObjectFinder<UWidgetBlueprint> UIWidgetAssetFinder(TEXT("/BPixelStreamingGCP/UI_PS_GCP_Package.UI_PS_GCP_Package"));
	
	if (UIWidgetAssetFinder.Succeeded())
	{
		UITabRegistrationName = FName(*(UIWidgetAssetFinder.Object->GetPathName() + LOCTEXT("ActiveTabSuffix", "_ActiveTab").ToString()));
		UITabAssetBP = (UEditorUtilityWidgetBlueprint*)UIWidgetAssetFinder.Object;
	}

	FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").OnMapChanged().AddUObject(this, &UPSGCPViewManager::OnMapLoaded);
	
	StyleSet = MakeShareable(new FSlateStyleSet("PSGCPTabIconStyle"));

	StyleSet->SetContentRoot(FPaths::ProjectPluginsDir() / TEXT("BPixelStreamingGCP/Content"));
	StyleSet->SetCoreContentRoot(FPaths::ProjectPluginsDir() / TEXT("BPixelStreamingGCP/Content"));

	StyleSet->Set("PSGCPTabIcon", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("b_40"), TEXT(".png")), FVector2D(40.0f, 40.0f)));
	StyleSet->Set("PSGCPTabIcon.Small", new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("b_20"), TEXT(".png")), FVector2D(20.0f, 20.0f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

UPSGCPViewManager::~UPSGCPViewManager()
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) return;

	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}

	FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor").OnMapChanged().RemoveAll(this);
}

TSharedPtr<FSlateStyleSet> UPSGCPViewManager::StyleSet = nullptr;

void UPSGCPViewManager::Start()
{
}

void UPSGCPViewManager::OnMapLoaded(UWorld* World, EMapChangeType MapChangeType)
{
	if (MapChangeType == EMapChangeType::NewMap || MapChangeType == EMapChangeType::LoadMap)
	{
		if (bConstructSuccessful) return;
		bConstructSuccessful = true;

		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;

		TWeakObjectPtr<UPSGCPViewManager> ThisPtr(this);

		//To have the level fully opened.
		TimerDelegate.BindLambda([ThisPtr]()
			{
				if (!ThisPtr.IsValid()) return;

				ThisPtr->RegisterUITab();
			});
		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.01f, false);
	}
}

void UPSGCPViewManager::OnTabBeingClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	RegisterUITab();
}

void UPSGCPViewManager::RegisterUITab()
{
	if (!UITabAssetBP.IsValid()) return;

	if (UWorld* EdWorld = GEditor->GetEditorWorldContext().World())
	{
		if (EdWorld->IsValidLowLevel() && !EdWorld->IsPendingKillOrUnreachable())
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegate;

			TWeakObjectPtr<UPSGCPViewManager> ThisPtr(this);

			//To have the level fully opened.
			TimerDelegate.BindLambda([ThisPtr]()
				{
					if (!ThisPtr.IsValid()) return;

					UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();

					EditorUtilitySubsystem->SpawnAndRegisterTab(ThisPtr->UITabAssetBP.Get());

					FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
					TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditor.GetLevelEditorTabManager();

					TSharedPtr<SDockTab> CreatedTab = LevelEditorTabManager->FindExistingLiveTab(ThisPtr->UITabRegistrationName);
					ensure(CreatedTab.IsValid());

					ThisPtr->UITabWeakPtr = CreatedTab;
					CreatedTab->SetTabIcon(FSlateIcon(StyleSet->GetStyleSetName(), "PSGCPTabIcon", "PSGCPTabIcon.Small").GetSmallIcon());
					CreatedTab->SetLabel(FText::FromString(TEXT("Pixel Streaming GCP Setup")));
					CreatedTab->SetContentScale(FVector2D(1.08f, 1.92f));
					CreatedTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateUObject(ThisPtr.Get(), &UPSGCPViewManager::OnTabBeingClosed));
					CreatedTab->FlashTab();

					if (IBlutilityModule* BlutilityModule = FModuleManager::GetModulePtr<IBlutilityModule>("Blutility"))
					{
						BlutilityModule->RemoveLoadedScriptUI(ThisPtr->UITabAssetBP.Get()); //We do not need it.
					}
				});
			EdWorld->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.01f, false);
		}
	}
}

bool UPSGCPViewManager::IsTickable() const
{
	return bConstructSuccessful;
}

TStatId UPSGCPViewManager::GetStatId() const
{
	return TStatId();
}

void UPSGCPViewManager::Tick(float DeltaTime)
{
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	FVector2D WindowSize(DisplayMetrics.PrimaryDisplayHeight / 3, DisplayMetrics.PrimaryDisplayWidth / 3);

	if (UITabWeakPtr.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = UITabWeakPtr.Pin()->GetParentWindow();
		if (ParentWindow.IsValid())
		{
			//Here; tab is not being dragged and not docked somewhere.

			ParentWindow->Resize(WindowSize);

			if (!ParentWindow->IsModalWindow())
			{
				ParentWindow->SetAsModalWindow();
				ParentWindow->SetSizingRule(ESizingRule::FixedSize);
				ParentWindow->SetNativeWindowButtonsVisibility(false);
				ParentWindow->SetViewportSizeDrivenByWindow(false);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE