/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBPixelStreamingGCPModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	//Note: On ShutdownModule; all UObjects are already destroyed.

private:
	TWeakObjectPtr<class UPSGCPViewManager> PSGCP_ViewManager;
	TSharedPtr<class PSGCPPackageManager> PSGCP_PackageManager;
};