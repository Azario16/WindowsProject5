#include "framework.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <mutex>
#include <fstream>
#include <vector>
#include <filesystem>


#pragma comment(lib, "Ws2_32.lib")

#define PORT 20000 // ����, �� ������� �� ����� ��������� ������

// ���������� ���������� ��� ����������� ������� �� ���������� �������
std::mutex g_mutex;
std::string g_receivedData;
int frameNumber = 0; // ���������� ���������� ��� ������������ ������ �����

// ������� ��� ���������� ����������� � ����
void SaveImageToFile(const std::string& imageData, int frameNumber, const std::string& directory) {
    // std::filesystem::create_directories(directory); // ������� ����������, ���� ��� �� ����������

    std::string filename = directory + "\\frame_" + std::to_string(frameNumber) + ".jpg";
    std::ofstream outfile(filename, std::ofstream::binary);
    if (outfile.is_open()) {
        outfile.write(imageData.c_str(), imageData.size());
        outfile.close();
        std::cout << "����������� ��������� � ����: " << filename << std::endl;
    }
    else {
        std::cerr << "������ �������� ����� ��� ����������: " << filename << std::endl;
    }
}

void ReceiveThreadFunc(HWND hwnd, SOCKET udpSocket) {
    std::vector<char> imageBuffer; // ����� ��� �������� ������ �����������

    char buffer[1024];
    sockaddr_in senderAddress;
    int senderAddressSize = sizeof(senderAddress);
    while (true) {
        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (sockaddr*)&senderAddress, &senderAddressSize);
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "������ ������ ������" << std::endl;
            OutputDebugStringA("������ ������ ������\n"); // ����� ������ � Debug Output
            break;
        }

        std::string receivedData(buffer, bytesReceived);

        // ��������� ���������� ������
        if (receivedData == "start") {
            // �������� ��������� "start", �������� �������� ����� �����������
            imageBuffer.clear(); // ������� �����
        }
        else if (receivedData == "stop") {
            // �������� ��������� "stop", ��������� ���� ������ � ������� �����������
            if (!imageBuffer.empty()) {
                // �������� ����������� JPEG �� ������
                SaveImageToFile(std::string(imageBuffer.begin(), imageBuffer.end()), frameNumber++, "C:\\image");
                imageBuffer.clear(); // ������� ������
            }
        }
        else {
            // �������� ����� �����������, ��������� �� � �����
            imageBuffer.insert(imageBuffer.end(), buffer, buffer + bytesReceived);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // �������� ������ ��� ������ ������ �� UDP
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            std::cerr << "������ �������� ������" << std::endl;
            WSACleanup();
            return 1;
        }

        // �������� ������ � ���������� ������ � �����
        sockaddr_in localAddress;
        localAddress.sin_family = AF_INET;
        localAddress.sin_port = htons(PORT);
        localAddress.sin_addr.s_addr = INADDR_ANY;
        if (bind(udpSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR) {
            std::cerr << "������ �������� ������ � �����" << std::endl;
            closesocket(udpSocket);
            WSACleanup();
            return 1;
        }

        // ������ ������ ��� ������ ������ �� UDP
        std::thread receiveThread(ReceiveThreadFunc, hwnd, udpSocket);
        receiveThread.detach(); // ����������� ����� �� ��������� ������ GUI
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // OutputDebugStringA("������������� Winsock\n");

        // ����� ���������� ������ � ������ ����
        std::lock_guard<std::mutex> lock(g_mutex);
        DrawTextA(hdc, g_receivedData.c_str(), -1, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
    }
    break;
    case WM_TIMER:
    {
        // ����������� ����
        InvalidateRect(hwnd, NULL, TRUE);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_USER + 1: // ��������� � ��������� ������
    {
        std::string receivedData(reinterpret_cast<const char*>(lParam));
        std::lock_guard<std::mutex> lock(g_mutex);
        g_receivedData = receivedData;
        InvalidateRect(hwnd, NULL, TRUE); // ����������� ���� ��� ����������� ���������� ������
    }
    break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
// ������� �������� � ����������� �������� ����
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    OutputDebugStringA("������������� Winsock\n");
    std::cerr << "������������� Winsock" << std::endl;

    // ������������� Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "������ ������������� Winsock" << std::endl;
        return 1;
    }

    // ����������� ������ ����
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // �������� ����
    HWND hwnd = CreateWindowEx(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Sample Window",               // Window text
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // ���� ��������� ���������
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    WSACleanup();
    return 0;
}
