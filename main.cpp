#include <windows.h>
#include <iostream>
#include <fstream>
#include <chrono>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main() {
    srand(time(NULL));

    // 830704 - 3+0+7+0+4=14 - число страниц буферной памяти
    const int numOfPages = 14;

    // Получаем размер физической страницы ОП
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int sizeOfPage = sysInfo.dwPageSize;

    // Размер буфера = размер страницы * количество страниц
    int sizeOfBuff = sizeOfPage * numOfPages;

    // Открываем лог файл писателей
    std::fstream logFile;
    logFile.open("writersLog.txt", std::fstream::app);

    char* data = new char[sizeOfPage];
    memset(data, 97 + rand() % 26, sizeOfPage);

    HANDLE file = CreateFile("buffer.txt", GENERIC_ALL, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    // Открываем проецируемый файл
    HANDLE buffer = OpenFileMapping(GENERIC_WRITE, NULL, "buffer");
    if (buffer == NULL) {
        // Или получаем дескриптор проекции файла
        buffer = CreateFileMapping(file, NULL, PAGE_READWRITE, 0, sizeOfBuff, "buffer");
    }

    // Делаем проекцию в память
    LPVOID buffAddress = MapViewOfFile(buffer, FILE_MAP_WRITE, 0, 0, sizeOfBuff);

    // Блокируем страницы в оперативной памяти (их нельзя вытеснить в файл подкачки)
    VirtualLock(buffAddress, sizeOfBuff);

    // Массив HANDLE'ов семафоров писателей
    HANDLE* writingSemaphoreArray = new HANDLE[numOfPages];
    char writingSemaphoreName[] = {"SwL" };
    for (int i = 0; i < numOfPages; ++i) {
        writingSemaphoreName[2] = i + '0';
        writingSemaphoreArray[i] = OpenSemaphore(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, false, writingSemaphoreName);
        if (writingSemaphoreArray[i] == NULL) {
            writingSemaphoreArray[i] = CreateSemaphore(NULL, 1, 1, writingSemaphoreName);
            if (writingSemaphoreArray[i] == NULL)
                std::cout << "Ошибка при создании Sw. Код ошибки: " << GetLastError() << std::endl;
        }
    }

    // Массив HANDLE'ов семафоров читателей
    HANDLE* readingSemaphoreArray = new HANDLE[numOfPages];
    char readingSemaphoreName[] = {"SrL" };
    for (int i = 0; i < numOfPages; ++i) {
        readingSemaphoreName[2] = i + '0';
        readingSemaphoreArray[i] = OpenSemaphore(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, false, readingSemaphoreName);
        if (readingSemaphoreArray[i] == NULL) {
            readingSemaphoreArray[i] = CreateSemaphore(NULL, 0, 1, readingSemaphoreName);
            if (readingSemaphoreArray[i] == NULL)
                std::cout << "Ошибка при создании Sr. Код ошибки: " << GetLastError() << std::endl;
        }
    }

    /*
     * 0 Ожидание
     * 1 Начало записи
     * 2 Конец записи
     * */

    while (true) {
        // Пишем в лог, что находимся в состоянии ожидания
        logFile << GetCurrentProcessId() << ','
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        << ",,0" << std::endl;

        // Ждем, когда один из семафоров писателей перейдёт в сигнальное состояние
        int currentPage = WaitForMultipleObjects(numOfPages, writingSemaphoreArray, false, INFINITE);

        // Пишем в лог, что перешли в состояние записи
        logFile << GetCurrentProcessId() << ','
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        << ',' << currentPage
        << ",1" << std::endl;

        std::cout << (int)buffAddress + currentPage * sizeOfPage << " " << currentPage << std::endl;

        // Производим "запись"
        memcpy((void*)((int)buffAddress + currentPage * sizeOfPage), data, sizeOfPage);

        // "Задаём" длительность записи случайным образом от 0,5 до 1,5 секунд
        Sleep(500 + rand() % 1000);

        // Пишем в лог, что закончили писать
        logFile << GetCurrentProcessId() << ','
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
        << ',' << currentPage
        << ",2" << std::endl;

        // Увеличивает счетчик семафора читателя на 1, тем самым переводя его в сигнальное состояние
        ReleaseSemaphore(readingSemaphoreArray[currentPage], 1, NULL);
    }
    return 0;
}
#pragma clang diagnostic pop