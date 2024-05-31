#ifndef _GR4_PACKET_MODEM_FILE_SINK
#define _GR4_PACKET_MODEM_FILE_SINK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace gr::packet_modem {

template <typename T>
class FileSink : public gr::Block<FileSink<T>>
{
public:
    using Description = Doc<R""(
@brief File Sink. Writes input items to a file.

This block writes all its input items into a binary file. The file can be
overwritten or appended, depending on the `append` parameter of the block constructor.

)"">;

private:
    FILE* _file = nullptr;

public:
    gr::PortIn<T> in;
    std::string filename;
    bool append = false;

    void start()
    {
        _file = std::fopen(filename.c_str(), append ? "ab" : "wb");
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

    gr::work::Status processBulk(gr::ConsumableSpan auto& inSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {})", this->name, inSpan.size());
#endif
        if (fwrite(inSpan.data(), sizeof(T), inSpan.size(), _file) != inSpan.size()) {
            this->emitErrorMessage(
                fmt::format("{}::processBulk", this->name),
                fmt::format("error writing to file: {}", std::strerror(errno)));
            this->requestStop();
            return gr::work::Status::ERROR;
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::FileSink, in, filename, append);

#endif // _GR4_PACKET_MODEM_FILE_SINK
