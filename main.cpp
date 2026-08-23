#include <fstream>
#include <sstream>
#include <iostream>
#include "lang/vm.h"

class Runtime
{
public:
    static int process(const string& path)
    {
        std::ifstream file(path);
        if(!file.is_open())
        {
            std::cerr<<"[file error] Could not open '"<<path<<"'.\n";
            return 74;
        }
        std::stringstream buf;
        buf<<file.rdbuf();

        auto result = VM::vm.interpret(buf.str());
        if(result==Result::COMPILE_ERROR)
            return 65;
        if(result==Result::RUNTIME_ERROR)
            return 70;
        return 0;
    }

    static int repl()
    {
        while (true)
        {
            string line;
            std::cout<<">> ";
            if(!getline(std::cin, line))
                break;
            if(line.substr(0,2)=="-r")
                return process(line.substr(3, line.size()));
            VM::vm.interpret(line);
        }
        return 0;
    }
};

int main(int argc, char* argv[])
{
    if(argc==1)
        return Runtime::repl();
    else if(argc==2)
        return Runtime::process(argv[1]);
    else 
    {
        std::cout<<"Wrong usage.";
        return -1;
    }
}
