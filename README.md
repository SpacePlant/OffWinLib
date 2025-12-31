# Description
Static library containing a C++ module that provides offensive capabilities for exploitation and post-exploitation on Windows. See [OffWinSamples](https://github.com/SpacePlant/OffWinSamples) for some sample projects utilizing the module.

# Dependencies
- [Windows Implementation Libraries (WIL)](https://github.com/microsoft/wil) via the Microsoft.Windows.ImplementationLibrary NuGet package. If it is not installed automatically on build, run `nuget restore`.

# How to Use OffWinLib in Your Visual Studio Project
1. Clone the repository or add the repository as a Git submodule.
2. Add the OffWinLib project to the same solution as your project.
3. Add a reference to the OffWinLib project from your project.
4. Use `import offwinlib;` in your project.
