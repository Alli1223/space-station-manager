#include "client/game_client.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <signal.h>
    #include <sys/wait.h>
#endif

// Try to connect to the server briefly to see if one is already running
static bool isServerRunning(const std::string& host, uint16_t port) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    bool running = (::connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0);

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    return running;
}

#ifdef _WIN32
static PROCESS_INFORMATION serverProcess = {};
static bool serverLaunched = false;

static bool launchServer() {
    // Find server executable next to this client executable
    char clientPath[MAX_PATH];
    GetModuleFileNameA(nullptr, clientPath, MAX_PATH);

    std::string clientDir(clientPath);
    auto lastSlash = clientDir.find_last_of("\\/");
    std::string serverExe = clientDir.substr(0, lastSlash + 1) + "station_server.exe";

    // Check if the server exe exists next to the client
    DWORD attr = GetFileAttributesA(serverExe.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        // Try sibling directory (build/server/Debug/ vs build/client/Debug/)
        std::string dir = clientDir.substr(0, lastSlash); // e.g. build/client/Debug
        auto slash2 = dir.find_last_of("\\/");
        std::string config = dir.substr(slash2 + 1); // e.g. Debug
        dir = dir.substr(0, slash2); // e.g. build/client
        auto slash3 = dir.find_last_of("\\/");
        std::string buildDir = dir.substr(0, slash3); // e.g. build
        serverExe = buildDir + "\\server\\" + config + "\\station_server.exe";

        attr = GetFileAttributesA(serverExe.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            std::cerr << "Could not find station_server.exe" << std::endl;
            return false;
        }
    }

    std::cout << "Launching server: " << serverExe << std::endl;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);

    // CREATE_NEW_CONSOLE gives the server its own console window
    if (!CreateProcessA(
        serverExe.c_str(),
        nullptr,
        nullptr, nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,  // inherit working directory
        &si,
        &serverProcess))
    {
        std::cerr << "Failed to launch server (error " << GetLastError() << ")" << std::endl;
        return false;
    }

    serverLaunched = true;
    return true;
}

static void shutdownServer() {
    if (serverLaunched) {
        // Gracefully terminate the server process
        TerminateProcess(serverProcess.hProcess, 0);
        WaitForSingleObject(serverProcess.hProcess, 3000);
        CloseHandle(serverProcess.hProcess);
        CloseHandle(serverProcess.hThread);
        serverLaunched = false;
    }
}
#else
static pid_t serverPid = 0;

static bool launchServer() {
    // Find server exe relative to client
    char clientPath[4096];
    ssize_t len = readlink("/proc/self/exe", clientPath, sizeof(clientPath) - 1);
    if (len < 0) return false;
    clientPath[len] = '\0';

    std::string clientDir(clientPath);
    auto lastSlash = clientDir.find_last_of('/');
    std::string serverExe = clientDir.substr(0, lastSlash + 1) + "station_server";

    // Check sibling directory too
    if (access(serverExe.c_str(), X_OK) != 0) {
        std::string dir = clientDir.substr(0, lastSlash);
        auto slash2 = dir.find_last_of('/');
        std::string buildDir = dir.substr(0, slash2);
        serverExe = buildDir + "/server/station_server";
    }

    std::cout << "Launching server: " << serverExe << std::endl;

    serverPid = fork();
    if (serverPid == 0) {
        execl(serverExe.c_str(), serverExe.c_str(), nullptr);
        _exit(1);
    }
    return serverPid > 0;
}

static void shutdownServer() {
    if (serverPid > 0) {
        kill(serverPid, SIGTERM);
        waitpid(serverPid, nullptr, 0);
        serverPid = 0;
    }
}
#endif

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = ssm::DEFAULT_PORT;
    std::string name = "Player";
    bool autoServer = true;

    // Generate a default name with random suffix
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    name += std::to_string(std::rand() % 1000);

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--name" && i + 1 < argc) {
            name = argv[++i];
        } else if (arg == "--no-server") {
            autoServer = false;
        }
    }

    std::cout << "=== Space Station Manager Client ===" << std::endl;

    // Auto-launch server if none is running
    if (autoServer && host == "127.0.0.1") {
        if (!isServerRunning(host, port)) {
            std::cout << "No server detected, launching one automatically..." << std::endl;
            if (launchServer()) {
                // Give the server time to start up
                std::cout << "Waiting for server to start..." << std::endl;
                for (int i = 0; i < 20; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    if (isServerRunning(host, port)) break;
                }
            }
        } else {
            std::cout << "Server already running on port " << port << std::endl;
        }
    }

    std::cout << "Connecting as '" << name << "' to " << host << ":" << port << std::endl;

    ssm::GameClient client;
    if (!client.init(host, port, name)) {
        std::cerr << "Failed to initialize client" << std::endl;
        shutdownServer();
        return 1;
    }

    client.run();
    client.shutdown();

    // If we launched the server, shut it down when the client exits
    shutdownServer();

    std::cout << "Client shut down cleanly." << std::endl;
    return 0;
}
