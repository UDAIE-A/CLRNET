using System;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Storage;

namespace System.IO
{
    public static class WinRtStorageExtensions
    {
        public static async Task<Stream> OpenReadStreamAsync(this StorageFile file)
        {
            if (file == null)
            {
                throw new ArgumentNullException("file");
            }

            return await file.OpenStreamForReadAsync().ConfigureAwait(false);
        }

        public static async Task<Stream> OpenWriteStreamAsync(this StorageFile file, CreationCollisionOption option = CreationCollisionOption.OpenIfExists)
        {
            if (file == null)
            {
                throw new ArgumentNullException("file");
            }

            StorageFile writable = file;
            if (option != CreationCollisionOption.OpenIfExists)
            {
                StorageFolder parent = await file.GetParentAsync().AsTask().ConfigureAwait(false);
                writable = await parent.CreateFileAsync(file.Name, option).AsTask().ConfigureAwait(false);
            }

            return await writable.OpenStreamForWriteAsync().ConfigureAwait(false);
        }

        public static async Task<StorageFile> EnsureFileAsync(this StorageFolder folder, string relativePath)
        {
            if (folder == null)
            {
                throw new ArgumentNullException("folder");
            }

            if (string.IsNullOrEmpty(relativePath))
            {
                throw new ArgumentException("relativePath");
            }

            string[] segments = relativePath.Split(new[] { '/', '\\' }, StringSplitOptions.RemoveEmptyEntries);
            StorageFolder current = folder;
            for (int i = 0; i < segments.Length - 1; i++)
            {
                current = await current.CreateFolderAsync(segments[i], CreationCollisionOption.OpenIfExists).AsTask().ConfigureAwait(false);
            }

            return await current.CreateFileAsync(segments[segments.Length - 1], CreationCollisionOption.OpenIfExists).AsTask().ConfigureAwait(false);
        }
    }
}
