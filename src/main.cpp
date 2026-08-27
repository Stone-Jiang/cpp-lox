#include <fstream>
#include <sstream>
#include <iostream>
#include "lang/vm.h"

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
            if(line.substr(0,2)=="-r")
                return process(vm, line.substr(3, line.size()));
            vm.interpret(line);
        }
        return 0;
    }
};

int main(int argc, char* argv[])
{
    VM vm;

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
