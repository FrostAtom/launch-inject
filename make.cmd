windres resources.rc -O coff -o resources.res
clang++ main.cpp -o injector.exe resources.res -mwindows -std=c++20 -Ofast -MJ compile_commands.json -Wl,--strip-all -fvisibility=hidden -fvisibility-inlines-hidden -lboost_filesystem-mt -lboost_program_options-mt -static