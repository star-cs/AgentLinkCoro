#include "base/net/streams/zlib_stream.h"
#include "base/util.h"

void test_gzip()
{
    std::cout << "===================gzip===================" << std::endl;
    std::string data = base::random_string(102400);

    auto gzip_compress = base::ZlibStream::CreateGzip(true, 1);
    std::cout << "compress: " << gzip_compress->write(data.c_str(), data.size())
              << " length: " << gzip_compress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_compress->flush() << std::endl;

    auto comperss_str = gzip_compress->getResult();
    auto gzip_uncompress = base::ZlibStream::CreateGzip(false, 1);

    std::cout << "uncompress: " << gzip_uncompress->write(comperss_str.c_str(), comperss_str.size())
              << " length: " << gzip_uncompress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_uncompress->flush() << std::endl;
    auto uncompress_str = gzip_uncompress->getResult();

    std::cout << "test_gzip: " << (data == uncompress_str) << " origin_size: " << data.size()
              << " uncompress.size: " << uncompress_str.size() << std::endl;
}

void test_deflate()
{
    std::cout << "===================deflate===================" << std::endl;
    std::string data = base::random_string(102400);

    auto gzip_compress = base::ZlibStream::CreateDeflate(true, 1);
    std::cout << "compress: " << gzip_compress->write(data.c_str(), data.size())
              << " length: " << gzip_compress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_compress->flush() << std::endl;

    auto comperss_str = gzip_compress->getResult();
    auto gzip_uncompress = base::ZlibStream::CreateDeflate(false, 1);

    std::cout << "uncompress: " << gzip_uncompress->write(comperss_str.c_str(), comperss_str.size())
              << " length: " << gzip_uncompress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_uncompress->flush() << std::endl;
    auto uncompress_str = gzip_uncompress->getResult();

    std::cout << "test_gzip: " << (data == uncompress_str) << " origin_size: " << data.size()
              << " uncompress.size: " << uncompress_str.size() << std::endl;
}

void test_zlib()
{
    std::cout << "===================zlib===================" << std::endl;
    std::string data = base::random_string(102400);

    auto gzip_compress = base::ZlibStream::CreateZlib(true, 1);
    std::cout << "compress: " << gzip_compress->write(data.c_str(), data.size())
              << " length: " << gzip_compress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_compress->flush() << std::endl;

    auto comperss_str = gzip_compress->getResult();
    auto gzip_uncompress = base::ZlibStream::CreateZlib(false, 1);

    std::cout << "uncompress: " << gzip_uncompress->write(comperss_str.c_str(), comperss_str.size())
              << " length: " << gzip_uncompress->getBuffers().size() << std::endl;
    std::cout << "flush: " << gzip_uncompress->flush() << std::endl;
    auto uncompress_str = gzip_uncompress->getResult();

    std::cout << "test_gzip: " << (data == uncompress_str) << " origin_size: " << data.size()
              << " uncompress.size: " << uncompress_str.size() << std::endl;
}

int main(int argc, char **argv)
{
    srand(time(0));
    test_gzip();
    test_deflate();
    test_zlib();
    return 0;
}
