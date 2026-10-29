#include <iostream>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

// Флаг: был ли получен сигнал SIGHUP
volatile sig_atomic_t wasSigHup = 0;

// Обработчик сигнала: только поднимает флаг
void sigHupHandler(int)
{
    wasSigHup = 1;
}

int main()
{
    const int PORT = 8080;

    // 1. Создание сокета
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == -1) {
        perror("socket");
        return 1;
    }

    int yes = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }
    if (listen(listenFd, 5) == -1) {
        perror("listen");
        return 1;
    }

    // 2. Регистрация обработчика сигнала
    struct sigaction sa;
    sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // 3. Блокирование сигнала
    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    std::cout << "Сервер запущен, порт " << PORT << ", PID " << getpid() << std::endl;

    int clientFd = -1; // единственное принятое соединение

    // 4. Основной цикл
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listenFd, &fds);
        int maxFd = listenFd;
        if (clientFd != -1) {
            FD_SET(clientFd, &fds);
            if (clientFd > maxFd)
                maxFd = clientFd;
        }

        // pselect временно разблокирует SIGHUP на время ожидания
        if (pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask) == -1) {
            if (errno == EINTR) {
                if (wasSigHup) {
                    wasSigHup = 0;
                    std::cout << "Получен сигнал SIGHUP" << std::endl;
                }
                continue;
            }
            perror("pselect");
            break;
        }

        // Данные в установленном соединении
        if (clientFd != -1 && FD_ISSET(clientFd, &fds)) {
            char buf[1024];
            ssize_t n = read(clientFd, buf, sizeof(buf));
            if (n > 0) {
                std::cout << "Получено данных: " << n << " байт" << std::endl;
            } else {
                if (n == 0)
                    std::cout << "Клиент закрыл соединение" << std::endl;
                else
                    perror("read");
                close(clientFd);
                clientFd = -1;
            }
        }

        // Новое подключение
        if (FD_ISSET(listenFd, &fds)) {
            int newFd = accept(listenFd, NULL, NULL);
            if (newFd == -1) {
                perror("accept");
            } else {
                std::cout << "Новое подключение" << std::endl;
                if (clientFd == -1) {
                    clientFd = newFd;
                    std::cout << "Соединение оставлено" << std::endl;
                } else {
                    close(newFd);
                    std::cout << "Лишнее соединение закрыто" << std::endl;
                }
            }
        }
    }

    // 5. Завершение работы
    if (clientFd != -1)
        close(clientFd);
    close(listenFd);
    return 0;
}
