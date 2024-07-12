# Projects Overview
- **OffWinLib**: Static library containing a C++ module that provides offensive capabilities for exploitation and post-exploitation on Windows.
- **Symlink**: Sample tool for creating symbolic file system links ([based on this](https://github.com/googleprojectzero/symboliclink-testing-tools/tree/main/CreateSymlink)).
- **Reglink**: Sample tool for creating and manipulating registry links ([based on this](https://github.com/googleprojectzero/symboliclink-testing-tools/tree/main/CreateRegSymlink)).
- **Oplock**: Sample tool for using opportunistic locks ([based on this](https://github.com/googleprojectzero/symboliclink-testing-tools/tree/main/SetOpLock)).
- **DLLInjector**: Sample tool for injecting DLLs into processes.
- **FolderContentsDeleteToArbitraryDelete**: Sample tool for turning an arbitrary folder contents delete primitive into an arbitrary file or folder delete primitive ([based on this](https://www.zerodayinitiative.com/blog/2022/3/16/abusing-arbitrary-file-deletes-to-escalate-privilege-and-other-great-tricks)).
- **FolderDeleteToCodeExec**: Sample tool for turning an arbitrary folder delete primitive into privileged code execution ([also based on this](https://www.zerodayinitiative.com/blog/2022/3/16/abusing-arbitrary-file-deletes-to-escalate-privilege-and-other-great-tricks)).
- **ErrorMSI**: MSI installer package that throws an error and initiates a rollback immediately.

# Dependencies
- [Windows Implementation Libraries (WIL)](https://github.com/microsoft/wil) via the Microsoft.Windows.ImplementationLibrary NuGet package. If it's not installed automatically on build, run `nuget restore`.
- The ErrorMSI project requires the [HeatWave for VS2022](https://marketplace.visualstudio.com/items?itemName=FireGiant.FireGiantHeatWaveDev17) Visual Studio extension.

# How to Use OffWinLib in your Visual Studio Project
1. Clone the repository.
2. Build the OffWinLib project.
3. Add the OffWinLib project to the same solution as your project.
4. Add a reference to the OffWinLib project from your project.
5. Use `import offwinlib;` in your project.
