using System;
using System.Linq.Expressions;

namespace System.Linq.Expressions
{
    /// <summary>
    /// Extension points that allow IL post-processing to retarget <see cref="Expression.Compile()"/> to CLRNET's
    /// Track B virtual machine without forcing application authors to change their source code.
    /// </summary>
    public static class CLRNetExpressionCompiler
    {
        public static Delegate Compile(LambdaExpression expression)
        {
            return CLRNet.Runtime.Dynamic.VmExpressionCompiler.Compile(expression);
        }

        public static TDelegate Compile<TDelegate>(Expression<TDelegate> expression)
        {
            return CLRNet.Runtime.Dynamic.VmExpressionCompiler.Compile(expression);
        }
    }
}
