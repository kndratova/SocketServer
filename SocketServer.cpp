#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

using namespace std;

// Функция для очистки ресурсов и закрытия сокетов
void cleanup(SOCKET& ListenSocket, SOCKET& ClientSocket, ADDRINFO* addrResult) {
    // Закрываем клиентский сокет, если он действительный
    if (ClientSocket != INVALID_SOCKET) {
        closesocket(ClientSocket);
        ClientSocket = INVALID_SOCKET;
    }

    // Закрываем слушающий сокет, если он действительный
    if (ListenSocket != INVALID_SOCKET) {
        closesocket(ListenSocket);
        ListenSocket = INVALID_SOCKET;
    }

    // Освобождаем память, выделенную для структуры addrinfo
    if (addrResult != NULL) {
        freeaddrinfo(addrResult);
        addrResult = NULL;
    }

    // Завершаем работу с библиотекой Winsock
    WSACleanup();
}

// Функция для получения доступных IP-адресов на устройстве
vector<string> GetAvailableIPAddresses() {
    vector<string> ipAddresses;

    // Инициализация библиотеки Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Winsock" << endl;
        return ipAddresses;
    }

    // Получение списка доступных IP-адресов
    char hostname[NI_MAXHOST];
    if (gethostname(hostname, NI_MAXHOST) != 0) {
        cerr << "Failed to get hostname" << endl;
        WSACleanup();
        return ipAddresses;
    }

    ADDRINFO hints;
    ADDRINFO* addrResult = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;

    if (getaddrinfo(hostname, NULL, &hints, &addrResult) != 0) {
        cerr << "Failed to get address info" << endl;
        WSACleanup();
        return ipAddresses;
    }

    // Обход списка адресов и добавление доступных IP-адресов в вектор
    for (ADDRINFO* addr = addrResult; addr != NULL; addr = addr->ai_next) {
        sockaddr_in* addrIPv4 = (sockaddr_in*)addr->ai_addr;
        char ipAddress[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &(addrIPv4->sin_addr), ipAddress, INET_ADDRSTRLEN) != NULL) {
            ipAddresses.push_back(ipAddress);
        }
    }

    freeaddrinfo(addrResult);
    WSACleanup();

    return ipAddresses;
}

int main() {
    WSADATA wsaData;
    ADDRINFO hints;
    ADDRINFO* addrResult = NULL;
    SOCKET ClientSocket = INVALID_SOCKET;
    SOCKET ListenSocket = INVALID_SOCKET;

    const char* sendBuffer = "Server test message";
    char recvBuffer[512];
    int result;

    // Инициализация сокета
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "WSAStartup failed, result = " << result << endl;
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Получение доступных IP-адресов на устройстве
    vector<string> ipAddresses = GetAvailableIPAddresses();
    if (ipAddresses.empty()) {
        cerr << "No available IP addresses found" << endl;
        WSACleanup();
        return 1;
    }

    // Выбор первого доступного IP-адреса
    const string ipAddress = ipAddresses[0];

    cout << "Server IP address: " << ipAddress << endl;

    // Получение информации об адресе для прослушивания
    result = getaddrinfo(ipAddress.c_str(), "8888", &hints, &addrResult);
    if (result != 0) {
        cout << "getaddrinfo failed, result = " << result << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    // Создание слушающего сокета
    ListenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "Socket creation failed" << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    // Привязка слушающего сокета к адресу
    result = bind(ListenSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
    if (result == SOCKET_ERROR) {
        cout << "Socket binding failed" << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    // Ожидание входящего подключения
    result = listen(ListenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        cout << "Listening failed, result = " << result << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    // Принятие входящего подключения
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        cout << "Accepting failed" << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    closesocket(ListenSocket);

    do {
        ZeroMemory(recvBuffer, sizeof(recvBuffer)); // Очистка буфера приема перед каждым приемом данных
        // Прием данных от клиента
        result = recv(ClientSocket, recvBuffer, (int)sizeof(recvBuffer), 0);
        if (result > 0) {
            cout << "Received " << result << " bytes" << endl;
            cout << "Received data: " << recvBuffer << endl;

            // Отправка данных клиенту
            result = send(ClientSocket, sendBuffer, (int)strlen(sendBuffer), 0);
            if (result == SOCKET_ERROR) {
                cout << "Failed to send data back" << endl;
                cleanup(ListenSocket, ClientSocket, addrResult);
                return 1;
            }
        }
        else if (result == 0) {
            cout << "Connection closing..." << endl;
        }
        else {
            cout << "recv failed with error" << endl;
            cleanup(ListenSocket, ClientSocket, addrResult);
            return 1;
        }
    } while (result > 0);

    // Завершение соединения с клиентом
    result = shutdown(ClientSocket, SD_SEND);
    if (result == SOCKET_ERROR) {
        cout << "shutdown client socket failed" << endl;
        cleanup(ListenSocket, ClientSocket, addrResult);
        return 1;
    }

    // Очистка ресурсов и закрытие сокетов
    cleanup(ListenSocket, ClientSocket, addrResult);

    // Ожидание ввода пользователя перед закрытием консоли
    system("pause");

    return 0;
}