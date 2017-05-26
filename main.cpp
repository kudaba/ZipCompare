#include "Bootstrap.h"
#include "ChartJSPrinter.h"
#include "File.h"

class FileParameter : public Parameter, public NamedObject
{
public:
    FileParameter(const char* filename)
        : NamedObject(filename)
        , FileData(File::ReadFile(filename))
    {

    }

    FileParameter(const char* filename, size_t size)
        : NamedObject(filename)
        , FileData(File::ReadFile(filename, size))
    {

    }

    string ToString() const override { return GetName(); }
    vector<char> const& Data() const { return FileData; }

private:
    vector<char> FileData;
};

class CompressionTest : public CodeTest
{
protected:
    CompressionTest(char const* name)
        : CodeTest(name)
    {
        SetPass("compression", [this](Parameter const* param) { return Compress(((FileParameter const*)param)->Data()); });
        SetPass("decompression", [this](Parameter const* param) { return Decompress(((FileParameter const*)param)->Data()); });

        SetPassSetup("decompression", [this](Parameter const* param) { DecompressSetup(((FileParameter const*)param)->Data()); });
        SetPassTeardown("decompression", [this](Parameter const* param) { DecompressTeardown(((FileParameter const*)param)->Data()); });
    }

    virtual vector<char> DoCompress(vector<char> const& sourceData) const = 0;
    virtual vector<char> DoDecompress(vector<char> const& sourceData, size_t originalSize) const = 0;
private:
    size_t Compress(vector<char> const& sourceData)
    {
        vector<char> cdata = DoCompress(sourceData);
        return cdata.size();
    }
    size_t Decompress(vector<char> const& sourceData)
    {
        UnCompressedData = DoDecompress(CompressedData, sourceData.size());
        return UnCompressedData.size();
    }

    void DecompressSetup(vector<char> const& sourceData)
    {
        CompressedData = DoCompress(sourceData);
    }

    void DecompressTeardown(vector<char> const& sourceData)
    {
        assert(sourceData.size() == UnCompressedData.size());
        assert(memcmp(sourceData.data(), UnCompressedData.data(), UnCompressedData.size()) == 0);

        CompressedData = vector<char>();
        UnCompressedData = vector<char>();
    }

    vector<char> CompressedData;
    vector<char> UnCompressedData;
};

#include "lz4/lib/lz4.h"

class LZ4Test : public CompressionTest
{
public:
    LZ4Test() : CompressionTest("lz4") {}

protected:
    vector<char> DoCompress(vector<char> const& sourceData) const override
    {
        vector<char> data;
        data.resize(LZ4_compressBound(sourceData.size()));

        int result = LZ4_compress_default(sourceData.data(), data.data(), sourceData.size(), data.size());
        assert(result >= 0);
        data.resize(result);
        return data;
    }

    vector<char> DoDecompress(vector<char> const& sourceData, size_t originalSize) const override
    {
        vector<char> data;
        data.resize(originalSize);

        int result = LZ4_decompress_safe(sourceData.data(), data.data(), sourceData.size(), data.size());
        assert(result >= 0);
        data.resize(result);
        return data;
    }
};

class LZ4FastTest : public CompressionTest
{
public:
    LZ4FastTest() : CompressionTest("lz4Fast") {}

protected:
    vector<char> DoCompress(vector<char> const& sourceData) const override
    {
        vector<char> data;
        data.resize(LZ4_compressBound(sourceData.size()));

        int result = LZ4_compress_fast(sourceData.data(), data.data(), sourceData.size(), data.size(), 10);
        assert(result >= 0);
        data.resize(result);
        return data;
    }

    vector<char> DoDecompress(vector<char> const& sourceData, size_t originalSize) const override
    {
        vector<char> data;
        data.resize(originalSize);

        int result = LZ4_decompress_fast(sourceData.data(), data.data(), data.size());
        assert(result >= 0);
        return data;
    }
};

class ExampleHarness : public TestHarness
{
    unique_ptr<TestSuite const> TestHarness::CreateTest() const override
    {
        TestSuite* suite = new TestSuite("Test Suite");

        File::FindFile("../TestData", "*.*", [suite](char const* filename, size_t size)
        {
            Parameter* param = new FileParameter(filename, size);
            suite->AddTestParameter(unique_ptr<Parameter const>(param));
        });

        suite->AddTest(unique_ptr<CodeTest>(new LZ4Test()));
        suite->AddTest(unique_ptr<CodeTest>(new LZ4FastTest()));

        return unique_ptr<TestSuite const>(suite);
    }

    void PrintTest(TestResults const& results) const override
    {
        ChartJSPrinter printer;
        printer.PrintResults(results);
        printer.Open();
    }
};

void main()
{
    Bootstrap::RunTests(ExampleHarness());
}