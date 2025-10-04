using System;

namespace TestApp 
{
    class Program 
    {
        static int Main(string[] args) 
        {
            Console.WriteLine("Hello from CLRNET Phase 1!");
            
            // Test basic operations
            TestBasicTypes();
            TestStringOperations();
            TestSimpleCollections();
            
            Console.WriteLine("All tests completed successfully.");
            return 0;
        }
        
        static void TestBasicTypes() 
        {
            int i = 42;
            double d = 3.14159;
            bool b = true;
            
            Console.WriteLine($"Integer: {i}");
            Console.WriteLine($"Double: {d}");
            Console.WriteLine($"Boolean: {b}");
        }
        
        static void TestStringOperations() 
        {
            string str1 = "Hello";
            string str2 = "World";
            string combined = str1 + " " + str2;
            
            Console.WriteLine($"Combined string: {combined}");
            Console.WriteLine($"Length: {combined.Length}");
        }
        
        static void TestSimpleCollections() 
        {
            int[] numbers = { 1, 2, 3, 4, 5 };
            int sum = 0;
            
            foreach (int num in numbers) 
            {
                sum += num;
            }
            
            Console.WriteLine($"Array sum: {sum}");
        }
    }
}