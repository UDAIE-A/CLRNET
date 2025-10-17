using System;

namespace System.Buffers
{
    public sealed class ArrayBufferWriter<T> : IBufferWriter<T>
    {
        private T[] _buffer;
        private int _index;

        public ArrayBufferWriter()
            : this(256)
        {
        }

        public ArrayBufferWriter(int initialCapacity)
        {
            if (initialCapacity <= 0)
            {
                throw new ArgumentOutOfRangeException("initialCapacity");
            }

            _buffer = new T[initialCapacity];
        }

        public ReadOnlyMemory<T> WrittenMemory
        {
            get { return new ReadOnlyMemory<T>(_buffer, 0, _index); }
        }

        public int WrittenCount
        {
            get { return _index; }
        }

        public int Capacity
        {
            get { return _buffer.Length; }
        }

        public int FreeCapacity
        {
            get { return _buffer.Length - _index; }
        }

        public void Clear()
        {
            Array.Clear(_buffer, 0, _index);
            _index = 0;
        }

        public void Advance(int count)
        {
            if (count < 0 || _index + count > _buffer.Length)
            {
                throw new ArgumentOutOfRangeException("count");
            }

            _index += count;
        }

        public Memory<T> GetMemory(int sizeHint = 0)
        {
            EnsureCapacity(sizeHint);
            return new Memory<T>(_buffer, _index, _buffer.Length - _index);
        }

        public Span<T> GetSpan(int sizeHint = 0)
        {
            EnsureCapacity(sizeHint);
            return new Span<T>(_buffer, _index, _buffer.Length - _index);
        }

        private void EnsureCapacity(int sizeHint)
        {
            if (sizeHint < 0)
            {
                throw new ArgumentOutOfRangeException("sizeHint");
            }

            if (sizeHint == 0)
            {
                sizeHint = 1;
            }

            int available = _buffer.Length - _index;
            if (available >= sizeHint)
            {
                return;
            }

            int newSize = _buffer.Length;
            while (newSize - _index < sizeHint)
            {
                newSize = newSize * 2;
            }

            T[] newBuffer = ArrayPool<T>.Shared.Rent(newSize);
            Array.Copy(_buffer, newBuffer, _index);
            ArrayPool<T>.Shared.Return(_buffer, true);
            _buffer = newBuffer;
        }
    }

    public interface IBufferWriter<T>
    {
        void Advance(int count);

        Memory<T> GetMemory(int sizeHint = 0);

        Span<T> GetSpan(int sizeHint = 0);
    }
}
