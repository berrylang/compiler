#include "bery_cli.h"
#include "commands/cmd_compile.h"
#include "commands/cmd_run.h"
#include "commands/cmd_version.h"
#include <iostream>
#include <string>


void printUsage() {
    std::cerr <<"Usage:\n";
    std::cerr <<"  bery compile <file.bry>   Compile to native binary\n";
    std::cerr <<"  bery run <file.bry>       Compile and run\n";
    std::cerr <<"  bery --version            Print version\n";
}


int beryMain(int argc, char* argv[]){
    if(argc<2){
        printUsage();
        return 1;
    }
    std::string command = argv[1];
    std::string ExeDir= getExeDir(argv[0]);
    if(command=="--version"|| command=="-v"|| command == "--VERSION"){
        return cmd_version();
    }
    if(command=="compile"){
        if(argc<3){
            std::cerr<<"Bery : Error : Missing source path\n";
            return 1;
        }
        std::string out_path;
        return cmdCompile(argv[2],out_path,ExeDir);
    }
    if(command == "run"){
        if(argc<3){
            std::cerr<<"Bery : Error : Missing source path\n";
            return 1;
        }
        return cmdRun(argv[2],ExeDir);
    }
    std::cerr <<"Bery : Error : Unknown command '" << command <<"'\n";
    printUsage();
    return 1;
}