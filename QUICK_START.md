# CLRNet Quick Start Guide

## 🚀 5-Minute Integration

Get your Windows Phone 8.1 app running with CLRNet in just 5 minutes!

### Step 1: Add CLRNet to Your Project (1 minute)

1. **Copy CLRNet Binaries**
   ```
   YourApp/
   ├── CLRNet/
   │   ├── CLRNetCore.dll
   │   ├── CLRNetInterop.dll
   │   ├── CLRNetSystem.dll
   │   └── CLRNetHost.exe
   └── SamplePlugin.dll
   ```

2. **Update Your .csproj File**
   ```xml
   <ItemGroup>
     <Content Include="CLRNet\*.dll">
       <CopyToOutputDirectory>Always</CopyToOutputDirectory>
     </Content>
   </ItemGroup>
   ```

### Step 2: Initialize CLRNet (2 minutes)

Add this to your `MainPage.xaml.cs`:

```csharp
using System.Runtime.InteropServices;

public sealed partial class MainPage : Page
{
    [DllImport("CLRNetCore.dll")]
    private static extern int CLRNet_Initialize(IntPtr config);
    
    [DllImport("CLRNetCore.dll")]
    private static extern int CLRNet_LoadAssembly(string path);
    
    [DllImport("CLRNetCore.dll")]
    private static extern int CLRNet_ExecuteMethod(string type, string method);

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        // Initialize CLRNet
        CLRNet_Initialize(IntPtr.Zero);
    }
}
```

### Step 3: Load and Execute (1 minute)

```csharp
private void RunPlugin_Click(object sender, RoutedEventArgs e)
{
    // Load your plugin
    CLRNet_LoadAssembly("SamplePlugin.dll");
    
    // Execute plugin method
    CLRNet_ExecuteMethod("PluginMain", "HelloWorld");
}
```

### Step 4: Update Package Manifest (1 minute)

Add to `Package.appxmanifest`:

```xml
<Extension Category="windows.activatableClass.inProcessServer">
  <InProcessServer>
    <Path>CLRNet\CLRNetCore.dll</Path>
    <ActivatableClass ActivatableClassId="CLRNet.Runtime" ThreadingModel="both" />
  </InProcessServer>
</Extension>
```

## ✅ That's It!

Your app now has dynamic .NET assembly loading capabilities!

## 🔥 Advanced Usage (Optional)

### Plugin Interface
```csharp
public interface IMyPlugin
{
    void Initialize();
    string Execute(string input);
}

// In your plugin assembly
public class MyPlugin : IMyPlugin
{
    public void Initialize() { /* setup */ }
    public string Execute(string input) { return "Processed: " + input; }
}
```

### Dynamic Code Compilation
```csharp
// Compile C# code at runtime
string code = @"
    public class RuntimeCode
    {
        public static string Process() { return ""Hello from runtime!""; }
    }";

// CLRNet can compile and execute this dynamically
```

### Game Modding System
```csharp
public class GameModLoader
{
    public async Task LoadMod(string modFile)
    {
        await CLRNet_LoadAssembly(modFile);
        CLRNet_ExecuteMethod("GameMod", "OnModLoaded");
    }
}
```

## 📱 Real-World Examples

### 1. **Business Rules Engine**
Load business logic from external assemblies without app updates.

### 2. **Plugin Marketplace**
Allow users to download and install app extensions.

### 3. **Dynamic UI Generation**
Create UI components from user-defined templates.

### 4. **Scripting System**
Enable power users to write custom scripts.

### 5. **A/B Testing Framework**
Load different feature implementations dynamically.

## 🛠️ Troubleshooting

**Runtime not initializing?**
- Check if CLRNet binaries are in your app package
- Verify Package.appxmanifest has the extension registration

**Assembly won't load?**
- Ensure assembly targets .NET Framework 4.0
- Check file path is correct

**Method execution fails?**
- Verify class and method names are exact
- Check method is public and static

## 🎯 Next Steps

1. **Read the full [Implementation Guide](IMPLEMENTATION_GUIDE.md)**
2. **Check out the [complete sample app](examples/WP81Integration/)**
3. **Explore advanced patterns and best practices**

## 💡 Pro Tips

- Initialize CLRNet once at app startup
- Cache loaded assemblies for better performance
- Use background threads for heavy operations
- Always handle exceptions gracefully
- Test on actual Windows Phone devices

**Happy Coding with CLRNet! 🎉**