#pragma once

#include "Modules/ModuleManager.h"

/**
 * Hell Run Networking Module
 * 
 * Provides complete multiplayer networking infrastructure:
 * - Session management (create, find, join, destroy)
 * - Social features (friends, invites)
 * - Proximity-based chat system
 * 
 * All features are accessible via blueprint-friendly interfaces
 */
class FHellRunNetworkingModule : public IModuleInterface
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
