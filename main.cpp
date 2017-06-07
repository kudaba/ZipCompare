#include "Bootstrap.h"
#include "ChartJSPrinter.h"
#include "File.h"

class FileParameter : public Parameter, public NamedObject
{
public:
    FileParameter(const char* filename)
        : NamedObject(GetFileName(filename))
        , FileData(File::ReadFile(filename))
    {

    }

    FileParameter(const char* filename, size_t size)
        : NamedObject(GetFileName(filename))
        , FileData(File::ReadFile(filename, size))
    {

    }

    string ToString() const override { return GetName(); }
    __int64 Max() const override { return FileData.size(); }

    vector<char> const& Data() const { return FileData; }

private:
    static const char* GetFileName(const char* path)
    {
        const char* lastSlash = path;
        for (const char* i = path; *i; ++i)
        {
            if (*i == '\\' || *i == '/')
                lastSlash = i + 1;
        }
        return lastSlash;
    }

    vector<char> FileData;
};

class CompressionTest : public CodeTest
{
protected:
    CompressionTest(char const* name)
        : CodeTest(name)
    {
        SetPass("compression", [this](Parameter const* param) { return Compress(((FileParameter const*)param)->Data()); });
        SetPassSetup("compression", [this](Parameter const* param) { CompressSetup(((FileParameter const*)param)->Data()); });
        SetPassTeardown("compression", [this](Parameter const* param) { CompressTeardown(((FileParameter const*)param)->Data()); });

        SetPass("decompression", [this](Parameter const* param) { return Decompress(((FileParameter const*)param)->Data()); });
        SetPassSetup("decompression", [this](Parameter const* param) { DecompressSetup(((FileParameter const*)param)->Data()); });
        SetPassTeardown("decompression", [this](Parameter const* param) { DecompressTeardown(((FileParameter const*)param)->Data()); });
    }

    virtual size_t CompressionSize(size_t sourceSize) const = 0;
    virtual void DoCompress(vector<char> const& sourceData, vector<char>& destData) const = 0;
    virtual void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const = 0;

    virtual void Setup(bool /*compress*/) {}
    virtual void Teardown(bool /*compress*/) {}
private:
    size_t Compress(vector<char> const& sourceData)
    {
        DoCompress(sourceData, CompressedData);
        return CompressedData.size();
    }

    void CompressSetup(vector<char> const& sourceData)
    {
        Setup(true);
        CompressedData.resize(CompressionSize(sourceData.size()));
    }

    void CompressTeardown(vector<char> const& sourceData)
    {
        CompressedData = vector<char>();
        Teardown(true);
    }

    size_t Decompress(vector<char> const& sourceData)
    {
        DoDecompress(CompressedData, UnCompressedData);
        return UnCompressedData.size();
    }

    void DecompressSetup(vector<char> const& sourceData)
    {
        Setup(false);
        CompressSetup(sourceData);
        DoCompress(sourceData, CompressedData);
        UnCompressedData.resize(sourceData.size());
    }

    void DecompressTeardown(vector<char> const& sourceData)
    {
        assert(sourceData.size() == UnCompressedData.size());
        assert(memcmp(sourceData.data(), UnCompressedData.data(), UnCompressedData.size()) == 0);

        UnCompressedData = vector<char>();
        CompressTeardown(sourceData);
        Teardown(false);
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
    size_t CompressionSize(size_t sourceSize) const override
    {
        return LZ4_compressBound(sourceSize);
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        int result = LZ4_compress_default(sourceData.data(), destData.data(), sourceData.size(), destData.size());
        assert(result >= 0);
        destData.resize(result);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        int result = LZ4_decompress_safe(sourceData.data(), destData.data(), sourceData.size(), destData.size());
        assert(result >= 0 && result == destData.size());
    }
};

class LZ4FastTest : public CompressionTest
{
public:
    LZ4FastTest() : CompressionTest("lz4Fast") {}

protected:
    size_t CompressionSize(size_t sourceSize) const override
    {
        return LZ4_compressBound(sourceSize);
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        int result = LZ4_compress_fast(sourceData.data(), destData.data(), sourceData.size(), destData.size(), 10);
        assert(result >= 0);
        destData.resize(result);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        int result = LZ4_decompress_fast(sourceData.data(), destData.data(), destData.size());
        assert(result >= 0 && result == sourceData.size());
    }
};

#include "snappy/snappy-c.h"

class SnappyTest : public CompressionTest
{
public:
    SnappyTest() : CompressionTest("Snappy") {}

protected:
    size_t CompressionSize(size_t sourceSize) const override
    {
        return snappy_max_compressed_length(sourceSize);
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        size_t result = destData.size();
        snappy_status status = snappy_compress(sourceData.data(), sourceData.size(), destData.data(), &result);
        assert(status == SNAPPY_OK && result >= 0);
        destData.resize(result);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        size_t result = destData.size();
        snappy_status status = snappy_uncompress(sourceData.data(), sourceData.size(), destData.data(), &result);
        assert(status == SNAPPY_OK && result == destData.size());
    }
};

#include "zlib/zlib.h"

class ZLibTest : public CompressionTest
{
public:
    ZLibTest() : CompressionTest("zlib") {}

protected:
    static voidpf alloc(voidpf opaque, uInt items, uInt size)
    {
        return new char[items*size];
    }
    static void free(voidpf opaque, voidpf address)
    {
        delete [] (char*)address;
    }

    size_t CompressionSize(size_t sourceSize) const override
    {
        return compressBound(sourceSize);
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        // don't init/end in setup/teardown to simulate use of 'compress' method
        z_stream strm;
        strm.zalloc = &alloc;
        strm.zfree = &free;
        strm.opaque = Z_NULL;

        deflateInit(&strm, Z_DEFAULT_COMPRESSION);
        strm.avail_in = sourceData.size();
        strm.next_in = (Bytef*)sourceData.data();

        strm.avail_out = destData.size();
        strm.next_out = (Bytef*)destData.data();

        int status = deflate(&strm, Z_FINISH);
        assert(status == Z_STREAM_END && strm.total_out >= 0);
        destData.resize(strm.total_out);
        deflateEnd(&strm);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        // don't init/end in setup/teardown to simulate use of 'decompress' method
        z_stream strm;
        strm.zalloc = &alloc;
        strm.zfree = &free;
        strm.opaque = Z_NULL;

        inflateInit(&strm);
        strm.avail_in = sourceData.size();
        strm.next_in = (Bytef*)sourceData.data();

        strm.avail_out = destData.size();
        strm.next_out = (Bytef*)destData.data();

        int status = inflate(&strm, Z_FINISH);
        assert(status == Z_STREAM_END && strm.total_out == destData.size());
        inflateEnd(&strm);
    }
};

#include "minilzo/minilzo.h"

class MiniLZOTest : public CompressionTest
{
public:
    MiniLZOTest() : CompressionTest("miniLZO") {}

protected:
    size_t CompressionSize(size_t sourceSize) const override
    {
        // taken from testmini.c
        return sourceSize + sourceSize / 16 + 64 + 3;
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        lzo_align_t workMemory[(LZO1X_1_MEM_COMPRESS + sizeof(lzo_align_t) - 1) / sizeof(lzo_align_t)];

        lzo_uint result = destData.size();
        int status = minilzo1x_1_compress((unsigned char const*)sourceData.data(), sourceData.size(), (unsigned char*)destData.data(), &result, &workMemory);
        assert(status == LZO_E_OK && result >= 0);
        destData.resize(result);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        lzo_uint result = destData.size();
        int status = minilzo1x_decompress((unsigned char const*)sourceData.data(), sourceData.size(), (unsigned char*)destData.data(), &result, NULL);
        assert(status == LZO_E_OK && result == destData.size());
    }
};

#include "lzo/lzo1c.h"

class LZO1CTest : public CompressionTest
{
public:
    LZO1CTest() : CompressionTest("LZO1C-3")
    {
        static bool init = false;
        if (!init)
        {
            lzo_init();
            init = true;
        }
    }

protected:
    size_t CompressionSize(size_t sourceSize) const override
    {
        // taken from testmini.c
        return sourceSize + sourceSize / 16 + 64 + 3;
    }
    void DoCompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        lzo_align_t workMemory[(LZO1C_MEM_COMPRESS + sizeof(lzo_align_t) - 1) / sizeof(lzo_align_t)];

        lzo_uint result = destData.size();
        int status = lzo1c_3_compress((unsigned char const*)sourceData.data(), sourceData.size(), (unsigned char*)destData.data(), &result, &workMemory);
        assert(status == LZO_E_OK && result >= 0);
        destData.resize(result);
    }

    void DoDecompress(vector<char> const& sourceData, vector<char>& destData) const override
    {
        lzo_uint result = destData.size();
        int status = lzo1c_decompress((unsigned char const*)sourceData.data(), sourceData.size(), (unsigned char*)destData.data(), &result, NULL);
        assert(status == LZO_E_OK && result == destData.size());
    }
};


class ZipHarness : public TestHarness
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
        suite->AddTest(unique_ptr<CodeTest>(new SnappyTest()));
        suite->AddTest(unique_ptr<CodeTest>(new ZLibTest()));
        suite->AddTest(unique_ptr<CodeTest>(new MiniLZOTest()));
        suite->AddTest(unique_ptr<CodeTest>(new LZO1CTest()));

        TestConfig config;
        config.CustomResult.Sort = PassConfig::Percentage;
        config.Performance.Logarithmic = true;

        suite->SetPassConfig("compression", config);

        config.CustomResult.Enabled = false;
        suite->SetPassConfig("decompression", config);
        suite->SetSummaryConfig(config);

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
    Bootstrap::RunTests(ZipHarness());
}