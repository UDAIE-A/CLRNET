using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace System.Collections.Generic
{
    public interface IAsyncEnumerable<out T>
    {
        IAsyncEnumerator<T> GetAsyncEnumerator(CancellationToken cancellationToken = default(CancellationToken));
    }

    public interface IAsyncEnumerator<out T> : IAsyncDisposable
    {
        T Current { get; }

        ValueTask<bool> MoveNextAsync();
    }
}

namespace System
{
    public interface IAsyncDisposable
    {
        ValueTask DisposeAsync();
    }
}

namespace System.Runtime.CompilerServices
{
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Struct)]
    public sealed class AsyncIteratorStateMachineAttribute : StateMachineAttribute
    {
        public AsyncIteratorStateMachineAttribute(Type stateMachineType)
            : base(stateMachineType)
        {
        }
    }
}

namespace System.Threading.Tasks
{
    public abstract class AsyncIteratorBase<T> : IAsyncEnumerator<T>, IAsyncEnumerable<T>
    {
        private readonly CancellationToken _cancellationToken;
        private bool _initialized;
        private bool _disposed;

        protected AsyncIteratorBase(CancellationToken cancellationToken)
        {
            _cancellationToken = cancellationToken;
        }

        public abstract T Current { get; }

        public abstract ValueTask<bool> MoveNextAsyncCore();

        public ValueTask<bool> MoveNextAsync()
        {
            if (_disposed)
            {
                throw new ObjectDisposedException(GetType().FullName);
            }

            _cancellationToken.ThrowIfCancellationRequested();
            _initialized = true;
            return MoveNextAsyncCore();
        }

        public virtual ValueTask DisposeAsync()
        {
            _disposed = true;
            return ValueTask.CompletedTask;
        }

        public IAsyncEnumerator<T> GetAsyncEnumerator(CancellationToken cancellationToken = default(CancellationToken))
        {
            if (_initialized)
            {
                throw new InvalidOperationException("Async iterator cannot be re-used.");
            }

            return this;
        }
    }

    public static class AsyncEnumerable
    {
        public static async ValueTask<List<T>> ToListAsync<T>(this IAsyncEnumerable<T> source, CancellationToken cancellationToken = default(CancellationToken))
        {
            if (source == null)
            {
                throw new ArgumentNullException("source");
            }

            List<T> result = new List<T>();
            IAsyncEnumerator<T> enumerator = source.GetAsyncEnumerator(cancellationToken);
            try
            {
                while (await enumerator.MoveNextAsync().ConfigureAwait(false))
                {
                    result.Add(enumerator.Current);
                }
            }
            finally
            {
                await enumerator.DisposeAsync().ConfigureAwait(false);
            }

            return result;
        }
    }
}
