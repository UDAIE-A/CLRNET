using System;
using System.Collections;
using System.Collections.Generic;

namespace System
{
    public struct ValueTuple : IEquatable<ValueTuple>, IStructuralEquatable, IStructuralComparable, IComparable
    {
        public static ValueTuple Create()
        {
            return new ValueTuple();
        }

        public override int GetHashCode()
        {
            return 0;
        }

        public override bool Equals(object obj)
        {
            return obj is ValueTuple;
        }

        public bool Equals(ValueTuple other)
        {
            return true;
        }

        public int CompareTo(object obj)
        {
            return obj is ValueTuple ? 0 : 1;
        }

        bool IStructuralEquatable.Equals(object other, IEqualityComparer comparer)
        {
            return other is ValueTuple;
        }

        int IStructuralEquatable.GetHashCode(IEqualityComparer comparer)
        {
            return 0;
        }

        int IStructuralComparable.CompareTo(object other, IComparer comparer)
        {
            return other is ValueTuple ? 0 : 1;
        }

        public override string ToString()
        {
            return "()";
        }
    }

    public struct ValueTuple<T1> : IEquatable<ValueTuple<T1>>, IStructuralEquatable, IStructuralComparable, IComparable
    {
        public T1 Item1;

        public ValueTuple(T1 item1)
        {
            Item1 = item1;
        }

        public static ValueTuple<T1> Create(T1 item1)
        {
            return new ValueTuple<T1>(item1);
        }

        public override bool Equals(object obj)
        {
            return obj is ValueTuple<T1> other && Equals(other);
        }

        public bool Equals(ValueTuple<T1> other)
        {
            return EqualityComparer<T1>.Default.Equals(Item1, other.Item1);
        }

        public override int GetHashCode()
        {
            return ValueTupleHelpers.CombineHashCodes(EqualityComparer<T1>.Default.GetHashCode(Item1));
        }

        public override string ToString()
        {
            return "(" + Item1 + ")";
        }

        public int CompareTo(object obj)
        {
            if (obj == null)
            {
                return 1;
            }

            if (!(obj is ValueTuple<T1> other))
            {
                throw new ArgumentException("Incompatible tuple");
            }

            return Comparer<T1>.Default.Compare(Item1, other.Item1);
        }

        bool IStructuralEquatable.Equals(object other, IEqualityComparer comparer)
        {
            return other is ValueTuple<T1> tuple && comparer.Equals(Item1, tuple.Item1);
        }

        int IStructuralEquatable.GetHashCode(IEqualityComparer comparer)
        {
            return ValueTupleHelpers.CombineHashCodes(comparer.GetHashCode(Item1));
        }

        int IStructuralComparable.CompareTo(object other, IComparer comparer)
        {
            if (!(other is ValueTuple<T1> tuple))
            {
                throw new ArgumentException("Incompatible tuple");
            }

            return comparer.Compare(Item1, tuple.Item1);
        }
    }

    public struct ValueTuple<T1, T2> : IEquatable<ValueTuple<T1, T2>>, IStructuralEquatable, IStructuralComparable, IComparable
    {
        public T1 Item1;
        public T2 Item2;

        public ValueTuple(T1 item1, T2 item2)
        {
            Item1 = item1;
            Item2 = item2;
        }

        public static ValueTuple<T1, T2> Create(T1 item1, T2 item2)
        {
            return new ValueTuple<T1, T2>(item1, item2);
        }

        public override bool Equals(object obj)
        {
            return obj is ValueTuple<T1, T2> other && Equals(other);
        }

        public bool Equals(ValueTuple<T1, T2> other)
        {
            return EqualityComparer<T1>.Default.Equals(Item1, other.Item1) &&
                   EqualityComparer<T2>.Default.Equals(Item2, other.Item2);
        }

        public override int GetHashCode()
        {
            return ValueTupleHelpers.CombineHashCodes(
                EqualityComparer<T1>.Default.GetHashCode(Item1),
                EqualityComparer<T2>.Default.GetHashCode(Item2));
        }

        public override string ToString()
        {
            return "(" + Item1 + ", " + Item2 + ")";
        }

        public int CompareTo(object obj)
        {
            if (obj == null)
            {
                return 1;
            }

            if (!(obj is ValueTuple<T1, T2> other))
            {
                throw new ArgumentException("Incompatible tuple");
            }

            int result = Comparer<T1>.Default.Compare(Item1, other.Item1);
            if (result != 0)
            {
                return result;
            }

            return Comparer<T2>.Default.Compare(Item2, other.Item2);
        }

        bool IStructuralEquatable.Equals(object other, IEqualityComparer comparer)
        {
            return other is ValueTuple<T1, T2> tuple &&
                   comparer.Equals(Item1, tuple.Item1) &&
                   comparer.Equals(Item2, tuple.Item2);
        }

        int IStructuralEquatable.GetHashCode(IEqualityComparer comparer)
        {
            return ValueTupleHelpers.CombineHashCodes(comparer.GetHashCode(Item1), comparer.GetHashCode(Item2));
        }

        int IStructuralComparable.CompareTo(object other, IComparer comparer)
        {
            if (!(other is ValueTuple<T1, T2> tuple))
            {
                throw new ArgumentException("Incompatible tuple");
            }

            int result = comparer.Compare(Item1, tuple.Item1);
            if (result != 0)
            {
                return result;
            }

            return comparer.Compare(Item2, tuple.Item2);
        }
    }

    internal static class ValueTupleHelpers
    {
        internal static int CombineHashCodes(int h1)
        {
            return h1;
        }

        internal static int CombineHashCodes(int h1, int h2)
        {
            return ((h1 << 5) + h1) ^ h2;
        }
    }
}
