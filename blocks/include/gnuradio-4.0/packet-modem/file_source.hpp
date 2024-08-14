#ifndef _GR4_PACKET_MODEM_FILE_SOURCE
#define _GR4_PACKET_MODEM_FILE_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace gr::packet_modem {

template <typename T>
class FileSource : public gr::Block<FileSource<T>>
{
public:
    using Description = Doc<R""(
@brief File Source. Reads output items from a file.

This block reads data from a binary file and outputs the file contents as items.

)"">;

public:
    FILE* _file = nullptr;

public:
    gr::PortOut<T> out;
    std::string filename;

    void start()
    {
        _file = std::fopen(filename.c_str(), "rb");
        if (_file == nullptr) {
            throw gr::exception(
                fmt::format("error opening file: {}", std::strerror(errno)));
        }
    }

    void stop()
    {
        if (_file != nullptr) {
            std::fclose(_file);
            _file = nullptr;
        }
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, inSpan.size());
#endif
        const size_t n = outSpan.size();
        const size_t ret = fread(outSpan.data(), sizeof(T), n, _file);
        if (ret != n) {
            int errno_save = errno;
            fmt::println("{} fread failed: n = {}, ret = {}, errno = {}",
                         this->name,
                         n,
                         ret,
                         errno_save);
            this->emitErrorMessage(
                fmt::format("{}::processBulk", this->name),
                fmt::format("error reading from file: {}", std::strerror(errno_save)));
            this->requestStop();
            return gr::work::Status::ERROR;
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::FileSource, out, filename);

#endif // _GR4_PACKET_MODEM_FILE_SOURCE
