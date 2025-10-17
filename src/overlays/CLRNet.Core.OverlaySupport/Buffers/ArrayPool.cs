using System;
using System.Collections.Generic;
using System.Threading;

namespace System.Buffers
{
    public abstract class ArrayPool<T>
    {
        private static ArrayPool<T> s_shared;

        public static ArrayPool<T> Shared
        {
            get
            {
                if (s_shared == null)
                {
                    Interlocked.CompareExchange(ref s_shared, new DefaultArrayPool<T>(), null);
                }

                return s_shared;
            }
        }

        public abstract T[] Rent(int minimumLength);

        public abstract void Return(T[] array, bool clearArray = false);
    }

    internal sealed class DefaultArrayPool<T> : ArrayPool<T>
    {
        private const int MaxBuckets = 20;
        private readonly object _sync = new object();
        private readonly Dictionary<int, Stack<T[]>> _buckets = new Dictionary<int, Stack<T[]>>();

        public override T[] Rent(int minimumLength)
        {
            if (minimumLength < 0)
            {
                throw new ArgumentOutOfRangeException("minimumLength");
            }

            int bucketSize = GetBucketSize(minimumLength);
            lock (_sync)
            {
                Stack<T[]> stack;
                if (_buckets.TryGetValue(bucketSize, out stack) && stack.Count > 0)
                {
                    return stack.Pop();
                }
            }

            return new T[bucketSize];
        }

        public override void Return(T[] array, bool clearArray = false)
        {
            if (array == null)
            {
                throw new ArgumentNullException("array");
            }

            if (clearArray)
            {
                Array.Clear(array, 0, array.Length);
            }

            int bucketSize = GetBucketSize(array.Length);
            lock (_sync)
            {
                Stack<T[]> stack;
                if (!_buckets.TryGetValue(bucketSize, out stack))
                {
                    stack = new Stack<T[]>();
                    _buckets[bucketSize] = stack;
                }

                if (stack.Count < 32)
                {
                    stack.Push(array);
                }
            }
        }

        private static int GetBucketSize(int length)
        {
            if (length <= 0)
            {
                return 16;
            }

            int power = 16;
            int max = 1 << MaxBuckets;
            while (power < length && power < max)
            {
                power <<= 1;
            }

            return power;
        }
    }
}
