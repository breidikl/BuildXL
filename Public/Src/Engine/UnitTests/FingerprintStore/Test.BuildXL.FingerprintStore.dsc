// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

import * as Managed from "Sdk.Managed";
import * as Deployment from "Sdk.Deployment";

namespace Test.BuildXL.FingerprintStore {
    @@public
    export const dll = BuildXLSdk.test({
        // These tests require Detours to run itself, so we can't detour xunit itself
        // TODO: QTest
        testFramework: importFrom("Sdk.Managed.Testing.XUnit.UnsafeUnDetoured").framework,

        assemblyName: "Test.BuildXL.FingerprintStore",
        sources: globR(d`.`, "*.cs"),
        references: [
            importFrom("BuildXL.Engine").Cache.dll,
            importFrom("BuildXL.App").Main.exe,
            importFrom("BuildXL.Engine").Engine.dll,
            importFrom("BuildXL.Engine").Processes.dll,
            importFrom("BuildXL.Engine").Scheduler.dll,
            Scheduler.IntegrationTest.dll,
            Scheduler.dll,
            importFrom("BuildXL.Pips").dll,
            importFrom("BuildXL.Tools").Execution.Analyzer.exe,
            importFrom("BuildXL.Utilities").dll,
            importFrom("BuildXL.Utilities").Configuration.dll,
            importFrom("BuildXL.Utilities").KeyValueStore.dll,
            importFrom("BuildXL.Utilities").Native.dll,
            importFrom("BuildXL.Utilities").Ipc.dll,      
            importFrom("BuildXL.Utilities").Storage.dll,
            importFrom("BuildXL.Utilities").ToolSupport.dll,
            importFrom("BuildXL.Utilities.UnitTests").TestProcess.exe,
            importFrom("BuildXL.Utilities.UnitTests").StorageTestUtilities.dll,
        ],
        runtimeContent: [
            importFrom("BuildXL.Utilities.UnitTests").TestProcess.deploymentDefinition
        ],
        runTestArgs: { weight: 10 },
    });
}
