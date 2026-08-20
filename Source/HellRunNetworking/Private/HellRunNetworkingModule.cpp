#include "HellRunNetworkingModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHellRunNetworkingModule"

void FHellRunNetworkingModule::StartupModule()
{
    // This code will execute after your module is loaded into memory
    UE_LOG(LogTemp, Log, TEXT("HellRunNetworking: Module started"));
}

void FHellRunNetworkingModule::ShutdownModule()
{
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module to cleanly release all dependencies.
    UE_LOG(LogTemp, Log, TEXT("HellRunNetworking: Module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHellRunNetworkingModule, HellRunNetworking)
