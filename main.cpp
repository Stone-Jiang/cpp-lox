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
            VM::vm.interpret(line);
        }
        return 0;
    }
};

int main(int argc, char* argv[])
{
    if(argc==2)
        return Runtime::process(argv[1]);

    std::cout<<"Enter file path or press enter to start REPL: ";
    string str;
    std::getline(std::cin, str);

    return str.empty()? Runtime::repl(): Runtime::process(str);
}
