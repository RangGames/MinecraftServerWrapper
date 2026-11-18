#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
int wmain(int argc, wchar_t **argv) {
    std::ofstream args("args.txt");
    for (int i = 1; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, s.data(), n, nullptr, nullptr);
        args << s.c_str() << '\n';
    }
    args.close();
    std::cout << "READY\n[12:30:00 INFO]: Done (0.123s)! For help, type help\n" << std::flush;
    std::cerr << "STDERR\n";
    std::cout << "\xed\x95" << std::flush;
    Sleep(160);
    std::cout << "\x9c\xea\xb8\x80\n" << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::ofstream("last-command.txt") << line;
        std::cout << "ACK:" << line << '\n' << std::flush;
        if (line == "stop" || line == "shutdown" || line == "end") {
            std::ofstream("stopped.txt") << "saved";
            return 0;
        }
        if (line == "colors")
            std::cout << "\x1b[38;2;12;200;80mRGB-COLOR\x1b[0m RESET-COLOR\n" << std::flush;
        if (line == "scrolltest") {
            for (int i = 0; i < 80; ++i)
                std::cout << "SCROLL-LINE-" << i << '\n';
            std::cout << std::flush;
        }
        if (line == "logburst") {
            for (int i = 0; i < 6000; ++i)
                std::cout << "BURST-" << i << ' ' << std::string(80, 'x') << '\n';
            std::cout << "BURST-DONE\n" << std::flush;
            std::ofstream("burst-done.txt") << "done";
        }
        if (line == "tabs")
            std::cout << "TAB-A\tTAB-B\tTAB-C\n" << std::flush;
        if (line == "crash")
            return 7;
        if (line == "login") {
            std::cout << "[12:30:01] [Server thread/INFO]: ZenE_[/127.0.0.1:53724] logged in " << std::flush;
            Sleep(180);
            std::cout << "with entity id 83 at ([minecraft:overworld]43.5, 65.0, 111.5)\n" << std::flush;
        }
        if (line == "joined")
            std::cout << "[12:30:02 INFO]: ZenE_ joined the game\n" << std::flush;
        if (line == "ipv6")
            std::cout << "[12:30:03 INFO]: Notch[/[0:0:0:0:0:0:0:1]:53725] logged in with entity id 84 at "
                         "([world]0, 64, 0)\n"
                      << std::flush;
        if (line == "quit")
            std::cout << "[12:30:04 INFO]: ZenE_ lost connection: Disconnected\n" << std::flush;
        if (line == "left")
            std::cout << "[12:30:05 INFO]: ZenE_ left the game\n" << std::flush;
        if (line == "notchquit")
            std::cout << "Notch left the game\n" << std::flush;
        if (line == "spoof")
            std::cout << "[12:30:06 INFO]: <Steve> Evil joined the game\n" << std::flush;
        if (line == "longline")
            std::cout << std::string(70000, 'x') << " Evil joined the game\n" << std::flush;
    }
    return 2;
}
