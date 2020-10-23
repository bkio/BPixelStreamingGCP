/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BPixelStreamingGCP.h"
#include "PSGCPViewManager.h"
#include "PSGCPPackageManager.h"

#define LOCTEXT_NAMESPACE "FBPixelStreamingGCPModule"

void FBPixelStreamingGCPModule::StartupModule()
{
	PSGCP_ViewManager = NewObject<UPSGCPViewManager>(GetTransientPackage(), NAME_None, RF_Public | RF_Standalone | RF_MarkAsRootSet);
	PSGCP_ViewManager->ClearFlags(RF_Transactional);
	PSGCP_ViewManager->Start();

	PSGCP_PackageManager = MakeShareable<PSGCPPackageManager>(new PSGCPPackageManager());
	PSGCP_PackageManager->Start();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBPixelStreamingGCPModule, BPixelStreamingGCP)