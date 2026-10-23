#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

// Условное событие: класс, передаётся по указателю (несериализуемые данные)
class Event {
public:
    int id;
    Event(int i) : id(i) {}
};

// Монитор: мьютекс (ключ) + две условные переменные (колокольчики)
class Monitor {
public:
    // Вызывается потоком-поставщиком
    void provide(Event* e) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_producer.wait(lock, [this] { return event == nullptr; }); // спит, пока событие не получено
        event = e;
        std::cout << "Поставщик: событие " << e->id << " отправлено" << std::endl;
        cv_consumer.notify_one(); // будит потребителя
    }

    // Вызывается потоком-потребителем
    Event* consume() {
        std::unique_lock<std::mutex> lock(mtx);
        cv_consumer.wait(lock, [this] { return event != nullptr; }); // спит, пока события нет
        Event* e = event;
        event = nullptr;
        std::cout << "Потребитель: событие " << e->id << " получено" << std::endl;
        cv_producer.notify_one(); // будит поставщика
        return e;
    }

private:
    std::mutex mtx;
    std::condition_variable cv_producer;
    std::condition_variable cv_consumer;
    Event* event = nullptr;
};

int main() {
    Monitor monitor;
    const int N = 5;

    std::thread producer([&] {
        for (int i = 1; i <= N; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // задержка в одну секунду
            monitor.provide(new Event(i));
        }
    });

    std::thread consumer([&] {
        for (int i = 1; i <= N; i++) {
            Event* e = monitor.consume();
            delete e;
        }
    });

    producer.join();
    consumer.join();
    return 0;
}
