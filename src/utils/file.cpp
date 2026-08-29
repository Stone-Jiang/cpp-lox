#include "file.h"

namespace utils
{
FileMode FileMode::parse(const std::string& m)
{
    if(m.empty())
        throw std::invalid_argument("mode string is empty");
    if(m.size() > 3)
        throw std::invalid_argument(std::format("mode string too long (max 3): '{}'", m));

    int r = 0, w = 0, a = 0, p = 0, b= 0;
    for(char c: m)
    {
        switch (c)
        {
        case 'r': r++; break;
        case 'w': w++; break;
        case 'a': a++; break;
        case '+': p++; break;
        case 'b': b++; break;
        default:
            throw std::invalid_argument(std::format("unkown mode char: '{}'", c));
        }
    }

    if (a+w+r == 0)
        throw std::invalid_argument("file mode must contain 'r', 'w', or 'a'");
    if (a+w+r > 1)
        throw std::invalid_argument("file mode has more than one of 'r', 'w', or 'a'");

    FileMode fm;
    fm.str = m;

    const bool plus = (p==1);
    const bool bin = (b==1);

    if (r)
        fm.flags = plus ? std::ios::in | std::ios::out : std::ios::in;
    else if (w)  
        fm.flags = plus ? std::ios::in | std::ios::out | std::ios::trunc : std::ios::out | std::ios::trunc;
    else
        fm.flags = plus ? std::ios::in | std::ios::out | std::ios::app : std::ios::out | std::ios::app;

    if (bin)           
        fm.flags |= std::ios::binary;

    fm.can_read = (fm.flags & std::ios::in) != std::ios::openmode{};
    fm.can_write = (fm.flags & std::ios::out) != std::ios::openmode{};

    return fm;
}

// File

File::File(const std::string& path, const std::string& mode_str): m_path(path), m_fm(FileMode::parse(mode_str))
{
    do_open();
}

File File::open(const std::string& path, const std::string& mode_str)
{
    return File(path, mode_str);
}

File::~File()
{
    // Destruction cannot report an I/O failure to the script, and must never
    // let an exception escape while the VM is collecting the file object.
    try { close(); }
    catch(...) {}
}

File::File(File&& other) noexcept: m_path(std::move(other.m_path)), m_fm(std::move(other.m_fm)), m_stream(std::move(other.m_stream)), m_closed(other.m_closed)
{
    other.m_closed = true;
}

File& File::operator=(File&& other) noexcept
{
    if(this != &other)
    {
        try { close(); }
        catch(...) {}
        m_path = std::move(other.m_path);
        m_fm = std::move(other.m_fm);
        m_stream = std::move(other.m_stream);
        m_closed = other.m_closed;
        other.m_closed = true;
    }
    return *this;
}

std::string File::read(std::streamsize n)
{
    check_readable();
    if (n<-1)
        throw std::invalid_argument(std::format("read(): size must >= -1 got {}", std::to_string(n)));

    m_stream.clear();
    std::string result;

    if (n<0)
    {
        result.assign(std::istreambuf_iterator<char>(m_stream), std::istreambuf_iterator<char>());
        m_stream.peek();
    }
    else
    {
        result.resize(static_cast<size_t>(n));
        m_stream.read(result.data(), n);
        result.resize(static_cast<size_t>(m_stream.gcount()));
    }

    check_stream("read");
    return result;
}

std::optional<std::string> File::readline()
{
    check_readable();
    m_stream.clear();

    std::string line;
    if(!std::getline(m_stream, line))
    {
        if (m_stream.eof())
            return std::nullopt;
        check_stream("readline");
    }

    if(!m_stream.eof())
        line += '\n';
    return line;
}

std::vector<std::string> File::readlines() 
{
    check_readable();
    m_stream.clear();

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(m_stream, line))
        lines.push_back(line + '\n');
    if (!lines.empty() && !line.empty())
        lines.back().pop_back();

    check_stream("readlines");
    return lines;
}

// Write

std::streamsize File::write(const std::string& data) 
{
    check_writable();
    m_stream << data;
    m_stream.flush();
    check_stream("write");
    return static_cast<std::streamsize>(data.size());
}

void File::writelines(const std::vector<std::string>& lines) 
{
    check_writable();
    for (const auto& l: lines) 
        m_stream << l;
    m_stream.flush();
    check_stream("writelines");
}

// Seek & Tell

void File::seek(std::streamoff offset, int whence) 
{
    check_open();
    if (whence < 0 || whence > 2)
        throw std::invalid_argument(std::format("seek(): whence must by 0, 1, 2 (got {})", std::to_string(whence)));
    m_stream.clear();

    auto dir = whence == 1 ? std::ios::cur
            : whence == 2 ? std::ios::end
            : std::ios::beg;
    if (m_fm.can_read)  
        m_stream.seekg(offset, dir);
    if (m_fm.can_write) 
        m_stream.seekp(offset, dir);

    check_stream("seek");
}

std::streampos File::tell()
{
    check_open();
    return m_fm.can_read? m_stream.tellg(): m_stream.tellp();
}

// State

void File::close()
{
    if (!m_closed && m_stream.is_open())
    {
        m_stream.flush();
        m_stream.close();
    }
    m_closed = true;
}

void File::flush()
{
    check_open();
    m_stream.flush();
    check_stream("flush");
}

bool File::readable() const 
{
    return !m_closed && m_fm.can_read;
}

bool File::writable() const 
{
    return !m_closed && m_fm.can_write;
}

bool File::seekable() const 
{
    return !m_closed; 
}

bool File::closed() const 
{
    return m_closed;
}

bool File::eof() const 
{
    return m_stream.eof();
}

bool File::good() const 
{
    return m_stream.good();
}

bool File::fail() const 
{
    return m_stream.fail();
}

const std::string& File::name() const 
{
    return m_path;
}

const std::string& File::mode() const 
{
    return m_fm.str;
}

std::ios::openmode File::openmode() const 
{
    return m_fm.flags;
}

std::uintmax_t File::size() const
{
    return std::filesystem::file_size(m_path);
}

// Iteration

File::LineIterator::LineIterator(File* f, bool end): file(f)
{ 
    if (!end) 
        next(); 
}

void File::LineIterator::next() 
{ 
    cur = file->readline();
}

const std::string& File::LineIterator::operator*()  const 
{ 
    return *cur; 
}

File::LineIterator& File::LineIterator::operator++() 
{ 
    next(); 
    return *this; 
}

bool File::LineIterator::operator!=(const LineIterator& o) const 
{ 
    return cur.has_value() != o.cur.has_value();
}

File::LineIterator File::begin() 
{ 
    check_readable(); 
    m_stream.clear();
    return {this, false}; 
}

File::LineIterator File::end()   
{ 
    return {this, true}; 
}

// Access
void File::do_open()
{
    m_stream.open(m_path, m_fm.flags);
    if(!m_stream.is_open())
        throw std::ios_base::failure(std::format("Cannot open file {} (mode: {}).", m_path, m_fm.str));
    m_stream.exceptions(std::ios::badbit);
    m_closed = false;
}


void File::check_open() const
{
    if(m_closed)
        throw std::ios_base::failure(std::format("IO on closed file: {}", m_path));
}

void File::check_readable() const
{
    check_open();
    if(!m_fm.can_read)
        throw std::ios_base::failure(std::format("File {} not open for reading (mode: {}).", m_path, m_fm.str));
}

void File::check_writable() const
{
    check_open();
    if(!m_fm.can_write)
        throw std::ios_base::failure(std::format("File {} not open for writing (mode: {}).", m_path, m_fm.str));
}

void File::check_stream(const char* op) const
{
    if(m_stream.bad())
        throw std::ios_base::failure(std::format("{}(): stream of {} is in bad state", std::string(op), m_path));
    if (m_stream.fail() && !m_stream.eof())
        throw std::ios_base::failure(std::format("{}(): stream failbit set on {}", std::string(op), m_path));
}

} // namespace utils
