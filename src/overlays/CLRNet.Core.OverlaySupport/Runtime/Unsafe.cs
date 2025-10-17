using System;
using System.Runtime.InteropServices;

namespace System.Runtime.CompilerServices
{
    public static unsafe class Unsafe
    {
        public static ref TTo As<TFrom, TTo>(ref TFrom source)
        {
            return ref *(TTo*)AsPointer(ref source);
        }

        public static void* AsPointer<T>(ref T value)
        {
            TypedReference reference = __makeref(value);
            return *(void**)&reference;
        }

        public static ref T AsRef<T>(void* source)
        {
            if (source == null)
            {
                throw new NullReferenceException();
            }

            return ref *(T*)source;
        }

        public static ref T Add<T>(ref T source, int elementOffset)
        {
            byte* pointer = (byte*)AsPointer(ref source) + elementOffset * SizeOf<T>();
            return ref *(T*)pointer;
        }

        public static int SizeOf<T>()
        {
            return Marshal.SizeOf(typeof(T));
        }
    }
}
