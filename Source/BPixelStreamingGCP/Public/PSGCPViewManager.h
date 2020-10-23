/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "TickableEditorObject.h"
#include "PSGCPViewManager.generated.h"

enum class EMapChangeType : uint8;

UCLASS()
class BPIXELSTREAMINGGCP_API UPSGCPViewManager : public UObject, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	UPSGCPViewManager();
	~UPSGCPViewManager();

	void Start();

private:
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual void Tick(float DeltaTime) override;

	FName UITabRegistrationName;
	TWeakObjectPtr<class UEditorUtilityWidgetBlueprint> UITabAssetBP;
	TWeakPtr<class SDockTab> UITabWeakPtr;

	static TSharedPtr<class FSlateStyleSet> StyleSet;

	bool bConstructSuccessful = false;

	void OnMapLoaded(class UWorld* World, EMapChangeType MapChangeType);

	void OnTabBeingClosed(TSharedRef<class SDockTab> TabBeingClosed);

	void RegisterUITab();
};