using System;
using System.IO;
using System.Linq.Expressions;
using System.Text.Json;
using System.Threading.Tasks;

record ModernRecord(int Id, string Name);

class Program
{
    static async Task Main()
    {
        ModernRecord record = new ModernRecord(42, "TrackC");
        string json = JsonSerializer.Serialize(record, new JsonSerializerOptions { WriteIndented = true });
        Console.WriteLine(json);

        StorageHelper.EnsureOverlayFile("appsettings.json");

        Expression<Func<int, int>> lambda = x => x * 2 + 10;
        Func<int, int> compiled = lambda.Compile();
        Console.WriteLine($"VM-backed expression output: {compiled(5)}");

        ValueTask<int> task = PerformWorkAsync();
        Console.WriteLine($"ValueTask result: {await task}");
    }

    static async ValueTask<int> PerformWorkAsync()
    {
        await Task.Delay(10).ConfigureAwait(false);
        return 123;
    }
}

static class StorageHelper
{
    public static void EnsureOverlayFile(string relativePath)
    {
        string fullPath = Path.Combine(AppContext.BaseDirectory, relativePath);
        if (!File.Exists(fullPath))
        {
            File.WriteAllText(fullPath, "{\"message\":\"overlay generated\"}");
        }
    }
}
