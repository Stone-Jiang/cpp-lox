#include <fstream>
#include <sstream>
#include "commons.h"
#include "vm.h"

class Runtime
{
public:
    static int process(const string& path)
    {
        ifstream file(path);
        if(!file.is_open())
        {
            cerr<<"[file error] Could not open '"<<path<<"'.\n";
            return 74;
        }
        stringstream buf;
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
            cout<<">> ";
            if(!getline(cin, line))
                break;
            VM::vm.interpret(line);
        }
        return 0;
    }

    static int run()
    {
        cout<<"Enter file path or press enter to start REPL: ";
        string str;
        getline(cin, str);

        if(str.empty())
            return Runtime::repl();
        else
            return Runtime::process(str);
    }
};

int main()
{
    cout<<"----- Lox Running on C++ "<<__cplusplus<<" -----\n";

    return Runtime::run();
}
