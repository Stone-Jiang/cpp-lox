#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include "include.h"

class Runtime
{
public:
    static int process(VM& vm, const string& path)
    {
        std::ifstream file(path);
        if(!file.is_open())
        {
            std::cerr<<"[file error] Could not open '"<<path<<"'.\n";
            return 74;
        }
        std::stringstream buf;
        buf<<file.rdbuf();

        auto result = vm.interpret(buf.str());
        if(result==Result::COMPILE_ERROR)
            return 65;
        if(result==Result::RUNTIME_ERROR)
            return 70;
        return 0;
    }

    static int repl(VM& vm)
    {
        while (true)
        {
            string line;
            std::cout<<">> ";
            if(!getline(std::cin, line))
                break;
            if(line.substr(0,2)=="-q")
                break;
            vm.interpret(line);
        }
        return 0;
    }

    static int load(VM& vm, const char* path)
    {
        std::error_code pathError;
        const auto executable = std::filesystem::absolute(path, pathError);
        if(pathError)
        {
            std::cerr << "[lib error] Could not resolve the interpreter path.\n";
            return 74;  
        }

        const std::string libPaths[] = {"functions.lox", "arrays.lox", "maps.lox"};
        int libResult = 0;
        for (const auto& path: libPaths)
        {
            const auto lib = executable.parent_path() / "src" / "lib" / path;
            libResult += Runtime::process(vm, lib.string());
        }
        return libResult;
    }
};

int main(int argc, char* argv[])
{
    VM vm;

    int libResult = Runtime::load(vm, argv[0]);
    if(libResult != 0)
    {
        std::cerr << "[lib error] Could not initialize the standard library.\n";
        return 78;
    }

    if(argc==1)
        return Runtime::repl(vm);
    else if(argc==2)
        return Runtime::process(vm, argv[1]);
    else 
    {
        std::cout<<"Wrong usage.";
        return -1;
    }
}
