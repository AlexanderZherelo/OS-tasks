#include "pch.h"
#include "CppUnitTest.h"
#include "../Lab2_OS/Lab2_OS.cpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ArrayProcessingTests
{
    TEST_CLASS(MainCppTests)
    {
    public:

        TEST_METHOD(TestFindMinMax_AllDistinct)
        {
            arr = { 5, 3, 9, 1, 4 };
            arrSize = static_cast<int>(arr.size());

            MinMaxThread(nullptr);

            Assert::AreEqual(3, minIndex);
            Assert::AreEqual(2, maxIndex);
        }

        TEST_METHOD(TestAverage_PositiveNumbers)
        {
            arr = { 10, 20, 30, 40 };
            arrSize = static_cast<int>(arr.size());

            AverageThread(nullptr);

            Assert::AreEqual(25.0, avg);
        }

        TEST_METHOD(TestAverage_NegativeAndPositive)
        {
            arr = { -5, 0, 5 };
            arrSize = static_cast<int>(arr.size());

            AverageThread(nullptr);

            Assert::AreEqual(0.0, avg);
        }

        TEST_METHOD(TestReplacement_WithAverage)
        {
            arr = { 2, 8, 6, 4 };
            arrSize = static_cast<int>(arr.size());

            MinMaxThread(nullptr);
            AverageThread(nullptr);

            int avgInt = static_cast<int>(avg);
            arr[minIndex] = avgInt;
            arr[maxIndex] = avgInt;

            std::vector<int> expected = { 5, 5, 6, 4 };
            Assert::IsTrue(arr == expected);
        }
    };
}
