using System;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;

namespace System.Threading.Tasks
{
    /// <summary>
    /// Lightweight awaitable that represents the eventual completion of an asynchronous operation.
    /// This implementation intentionally mirrors the modern .NET ValueTask shape while relying on
    /// Task when the runtime lacks specialized support.
    /// </summary>
    [AsyncMethodBuilder(typeof(System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder))]
    public struct ValueTask : IEquatable<ValueTask>
    {
        private static readonly Task s_completedTask = Task.FromResult(true);
        private readonly Task _task;

        public ValueTask(Task task)
        {
            _task = task ?? s_completedTask;
        }

        public ValueTaskAwaiter GetAwaiter()
        {
            return new ValueTaskAwaiter(_task ?? s_completedTask);
        }

        public Task AsTask()
        {
            return _task ?? s_completedTask;
        }

        public static ValueTask CompletedTask
        {
            get { return new ValueTask(s_completedTask); }
        }

        public bool Equals(ValueTask other)
        {
            return AsTask().Equals(other.AsTask());
        }

        public override bool Equals(object obj)
        {
            return obj is ValueTask other && Equals(other);
        }

        public override int GetHashCode()
        {
            return AsTask().GetHashCode();
        }

        public struct ValueTaskAwaiter : ICriticalNotifyCompletion
        {
            private readonly Task _task;

            internal ValueTaskAwaiter(Task task)
            {
                _task = task;
            }

            public bool IsCompleted
            {
                get { return _task.IsCompleted; }
            }

            public void OnCompleted(Action continuation)
            {
                if (continuation == null)
                {
                    throw new ArgumentNullException("continuation");
                }

                _task.GetAwaiter().OnCompleted(continuation);
            }

            public void UnsafeOnCompleted(Action continuation)
            {
                if (continuation == null)
                {
                    throw new ArgumentNullException("continuation");
                }

                _task.GetAwaiter().UnsafeOnCompleted(continuation);
            }

            public void GetResult()
            {
                _task.GetAwaiter().GetResult();
            }
        }
    }

    [AsyncMethodBuilder(typeof(System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder<TResult>))]
    public struct ValueTask<TResult> : IEquatable<ValueTask<TResult>>
    {
        private readonly Task<TResult> _task;
        private readonly TResult _result;
        private readonly bool _hasResult;

        public ValueTask(Task<TResult> task)
        {
            if (task == null)
            {
                throw new ArgumentNullException("task");
            }

            _task = task;
            _result = default(TResult);
            _hasResult = false;
        }

        public ValueTask(TResult result)
        {
            _result = result;
            _task = null;
            _hasResult = true;
        }

        public bool IsCompleted
        {
            get { return _hasResult || (_task != null && _task.IsCompleted); }
        }

        public ValueTaskAwaiter GetAwaiter()
        {
            return new ValueTaskAwaiter(this);
        }

        public Task<TResult> AsTask()
        {
            return _task ?? Task.FromResult(_result);
        }

        public bool Equals(ValueTask<TResult> other)
        {
            if (_hasResult && other._hasResult)
            {
                return Equals(_result, other._result);
            }

            return AsTask().Equals(other.AsTask());
        }

        public override bool Equals(object obj)
        {
            return obj is ValueTask<TResult> other && Equals(other);
        }

        public override int GetHashCode()
        {
            if (_hasResult)
            {
                return _result == null ? 0 : _result.GetHashCode();
            }

            return AsTask().GetHashCode();
        }

        public struct ValueTaskAwaiter : ICriticalNotifyCompletion
        {
            private readonly ValueTask<TResult> _owner;

            internal ValueTaskAwaiter(ValueTask<TResult> owner)
            {
                _owner = owner;
            }

            public bool IsCompleted
            {
                get { return _owner.IsCompleted; }
            }

            public TResult GetResult()
            {
                if (_owner._hasResult)
                {
                    return _owner._result;
                }

                return _owner._task.GetAwaiter().GetResult();
            }

            public void OnCompleted(Action continuation)
            {
                if (_owner._hasResult)
                {
                    continuation();
                    return;
                }

                _owner._task.GetAwaiter().OnCompleted(continuation);
            }

            public void UnsafeOnCompleted(Action continuation)
            {
                if (_owner._hasResult)
                {
                    continuation();
                    return;
                }

                _owner._task.GetAwaiter().UnsafeOnCompleted(continuation);
            }
        }
    }

    public static class ValueTaskExtensions
    {
        public static ValueTask<TResult> AsValueTask<TResult>(this Task<TResult> task)
        {
            return new ValueTask<TResult>(task);
        }

        public static ValueTask AsValueTask(this Task task)
        {
            return new ValueTask(task);
        }
    }
}

namespace System.Runtime.CompilerServices
{
    /// <summary>
    /// Async method builder used by the C# compiler when targeting ValueTask.
    /// This builder delegates all work to TaskCompletionSource while exposing
    /// the shape required by the compiler.
    /// </summary>
    public struct AsyncValueTaskMethodBuilder
    {
        private AsyncTaskMethodBuilder _builder;

        public static AsyncValueTaskMethodBuilder Create()
        {
            return new AsyncValueTaskMethodBuilder
            {
                _builder = AsyncTaskMethodBuilder.Create()
            };
        }

        public ValueTask Task
        {
            get { return new ValueTask(_builder.Task); }
        }

        public void SetException(Exception exception)
        {
            _builder.SetException(exception);
        }

        public void SetResult()
        {
            _builder.SetResult();
        }

        public void SetStateMachine(IAsyncStateMachine stateMachine)
        {
            _builder.SetStateMachine(stateMachine);
        }

        public void Start<TStateMachine>(ref TStateMachine stateMachine)
            where TStateMachine : IAsyncStateMachine
        {
            _builder.Start(ref stateMachine);
        }

        public void AwaitOnCompleted<TAwaiter, TStateMachine>(ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : INotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            _builder.AwaitOnCompleted(ref awaiter, ref stateMachine);
        }

        public void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            _builder.AwaitUnsafeOnCompleted(ref awaiter, ref stateMachine);
        }
    }

    public struct AsyncValueTaskMethodBuilder<TResult>
    {
        private AsyncTaskMethodBuilder<TResult> _builder;

        public static AsyncValueTaskMethodBuilder<TResult> Create()
        {
            return new AsyncValueTaskMethodBuilder<TResult>
            {
                _builder = AsyncTaskMethodBuilder<TResult>.Create()
            };
        }

        public ValueTask<TResult> Task
        {
            get { return new ValueTask<TResult>(_builder.Task); }
        }

        public void SetException(Exception exception)
        {
            _builder.SetException(exception);
        }

        public void SetResult(TResult result)
        {
            _builder.SetResult(result);
        }

        public void SetStateMachine(IAsyncStateMachine stateMachine)
        {
            _builder.SetStateMachine(stateMachine);
        }

        public void Start<TStateMachine>(ref TStateMachine stateMachine)
            where TStateMachine : IAsyncStateMachine
        {
            _builder.Start(ref stateMachine);
        }

        public void AwaitOnCompleted<TAwaiter, TStateMachine>(ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : INotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            _builder.AwaitOnCompleted(ref awaiter, ref stateMachine);
        }

        public void AwaitUnsafeOnCompleted<TAwaiter, TStateMachine>(ref TAwaiter awaiter, ref TStateMachine stateMachine)
            where TAwaiter : ICriticalNotifyCompletion
            where TStateMachine : IAsyncStateMachine
        {
            _builder.AwaitUnsafeOnCompleted(ref awaiter, ref stateMachine);
        }
    }
}
