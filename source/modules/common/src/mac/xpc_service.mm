//=============================================================================
// Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Apple 3D
/// @file
/// @brief RDP XPC handler implementation.
//=============================================================================

#import <Foundation/Foundation.h>

class XPCWrapper {
public:
    XPCWrapper();
    ~XPCWrapper();
};

XPCWrapper::XPCWrapper()
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        NSString* helperPath = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"RadeonDeveloperServiceXPC"];
        if (!helperPath) {
            helperPath = [[[NSBundle mainBundle].bundlePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"RadeonDeveloperServiceXPC"];
        }
        if (helperPath) {
            NSURL* launchAgentFolderURL = [[[NSFileManager defaultManager] homeDirectoryForCurrentUser] URLByAppendingPathComponent:@"Library/LaunchAgents"];
            NSURL* launchAgentURL = [launchAgentFolderURL URLByAppendingPathComponent:@"com.amd.devservice.plist"];

            NSDictionary* dictionaryFromDisk = [NSDictionary dictionaryWithContentsOfURL:launchAgentURL];
            if (dictionaryFromDisk) {
                system("launchctl stop ~/Library/LaunchAgents/com.amd.devservice.plist");
                system("launchctl unload ~/Library/LaunchAgents/com.amd.devservice.plist");
            }

            if (!dictionaryFromDisk || [[dictionaryFromDisk objectForKey:@"Program"] compare:helperPath options:(NSStringCompareOptions)NSCaseInsensitiveSearch] != NSOrderedSame) {
                NSMutableDictionary* dict = [NSMutableDictionary new];
                [dict setObject:@YES forKey:@"KeepAlive"];
                [dict setObject:@"com.amd.devservice" forKey:@"Label"];
                [dict setObject:helperPath forKey:@"Program"];
                [dict setObject:[NSDictionary dictionaryWithObjectsAndKeys:@YES, @"com.amd.devservice", nil] forKey:@"MachServices"];

                [[NSFileManager defaultManager] createDirectoryAtURL:launchAgentFolderURL withIntermediateDirectories:YES attributes:nil error:nil];
                [[NSFileManager defaultManager] removeItemAtURL:launchAgentURL error:nil];
                [dict writeToURL:launchAgentURL atomically:YES];
            }

            system("launchctl load ~/Library/LaunchAgents/com.amd.devservice.plist");
            system("launchctl unload ~/Library/LaunchAgents/com.amd.devservice.plist");
            system("launchctl load ~/Library/LaunchAgents/com.amd.devservice.plist");
        }
    });
}

XPCWrapper::~XPCWrapper()
{
    system("launchctl stop ~/Library/LaunchAgents/com.amd.devservice.plist");
    system("launchctl unload ~/Library/LaunchAgents/com.amd.devservice.plist");

    NSURL* launchAgentURL = [[[NSFileManager defaultManager] homeDirectoryForCurrentUser] URLByAppendingPathComponent:@"Library/LaunchAgents/com.amd.devservice.plist"];
    [[NSFileManager defaultManager] removeItemAtURL:launchAgentURL error:nil];

    // On shutdown disable the global developer mode environment, so we don't continue to instrument applications.
    system("launchctl unsetenv AMD_MTL_DEVELOPER_MODE");
}

static XPCWrapper xpc;
