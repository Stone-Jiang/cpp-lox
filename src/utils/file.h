#pragma once

#include <string>
#include <fstream>
#include <stdexcept>
#include <format>
#include <utility>
#include <vector>
#include <filesystem>
#include <optional>

//  mode │ in  out  app  trunc
//  ─────┼──────────────────────
//  "r"  │  ✓
//  "w"  │       ✓        ✓
//  "a"  │       ✓   ✓
//  "r+" │  ✓   ✓
//  "w+" │  ✓   ✓        ✓
//  "a+" │  ✓   ✓   ✓

namespace utils
{
struct FileMode
{
    std::ios::openmode flags = {};
    bool can_read = false;
    bool can_write = false;
    std::string str;

    static FileMode parse(const std::string& m);
};

class File
{
private:
    std::string m_path;
    FileMode m_fm;
    std::fstream m_stream;
    bool m_closed = true;
    
public:
    // Construction and RAII
    File(const std::string& path, const std::string& mode_str);
    static File open(const std::string& path, const std::string& mode_str);
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    
    // Read
    std::string read(std::streamsize n = -1);
    std::optional<std::string> readline();
    std::vector<std::string> readlines();

    // Write
    std::streamsize write(const std::string& data);
    void writelines(const std::vector<std::string>& lines);

    // Seek & Tell
    void seek(std::streamoff offset, int whence = 0);
    std::streampos tell();

    // State
    void close();
    void flush();

    bool readable() const;
    bool writable() const;
    bool seekable() const;
    bool closed() const;
    bool eof() const;
    bool good() const;
    bool fail() const;

    const std::string& name() const;
    const std::string& mode() const;
    std::ios::openmode openmode() const;

    std::uintmax_t size() const;

    // Iteration

    struct LineIterator 
    {
        File* file; 
        std::optional<std::string> cur;

        LineIterator(File* f, bool end);
        void next();
        const std::string& operator*() const;
        LineIterator& operator++();
        bool operator!=(const LineIterator& o) const;
    };
    
    LineIterator begin();
    LineIterator end();

private:
    // Access
    void do_open();
    void check_open() const;
    void check_readable() const;
    void check_writable() const;
    void check_stream(const char* op) const;
};
}