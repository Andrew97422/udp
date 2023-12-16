﻿/*
 * Шаблон параллельного эхо-сервера TCP, работающего по модели
 * "один клиент - один поток".
 *
 * Компиляция:
 *      gcc -Wall -O2 -lpthread -o server3 server3.c

    -Wall - сообщения о предупреждениях и ошибках
    -O2 - уровень оптимизации (безопасная оптимизация всего)
    -lpthread - связывание с многопоточной библиотекой pthread -> link pthread
    -o имя исполняемого файла (без флага создаст a.out)

 */

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>

 /*
  * Конфигурация сервера.
  */
#define PORT 1027
#define BACKLOG 5
#define MAXLINE 256

#define SA struct sockaddr

  /*
   * Обработчик фатальных ошибок.
   */
void error(const char* s)
{
    perror(s);
    exit(-1);
}

/*
 * Функции-обёртки.
 */
/*
* Socket() создаёт конечную точку соединения и возвращает файловый дескриптор, указывающий на эту точку.
* Параметр domain задает домен соединения:
* выбирает семейство протоколов, которое будет использоваться для создания соединения.
*/
//Параметр domain задает домен соединения: выбирает семейство протоколов,
//которое будет использоваться для создания соединения.
//Сокет имеет тип type, задающий семантику соединения.
//В protocol задаётся определённый протокол, используемый с сокетом.
//Обычно, только единственный протокол существует для поддержи определённого типа сокета с заданным семейством
//протоколов, в этом случае в protocol можно указать 0.
int Socket(int domain, int type, int protocol)
{
    int rc;

    rc = socket(domain, type, protocol);
    if (rc == -1) error("socket()");

    return rc;
}

//После создания с помощью socket(2), сокет появляется в адресном пространстве(семействе адресов),
//но без назначенного адреса.bind() назначает адрес, заданный в addr, сокету, указываемому дескриптором файла sockfd.
//В аргументе addrlen задаётся размер структуры адреса(в байтах), на которую указывает addr.
//В силу традиции, эта операция называется «присваивание сокету имени».
//Обычно, сокету типа SOCK_STREAM нужно назначить локальный адрес с помощью bind() до того,
//как он сможет принимать соединения.
//При успешном выполнении возвращается 0. В случае ошибки возвращается -1,
//а errno устанавливается в соответствующее значение.
int Bind(int socket, struct sockaddr* addr, socklen_t addrlen)
{
    int rc;

    rc = bind(socket, addr, addrlen);
    if (rc == -1) error("bind()");

    return rc;
}

//Вызов listen() помечает сокет, указанный в sockfd как пассивный, то есть как сокет,
//который будет использоваться для приёма запросов входящих соединений с помощью accept(2).
//Аргумент sockfd является файловым дескриптором, который ссылается на сокет типа SOCK_STREAM или SOCK_SEQPACKET.
//Аргумент backlog задает максимальный размер, до которого может расти очередь ожидающих соединений у sockfd.
//Если приходит запрос на соединение, а очередь полна, то клиент может получить ошибку с указание ECONNREFUSED или,
//если низлежащий протокол поддерживает повторную передачу, запрос может быть игнорирован,
//чтобы попытаться соединиться позднее.
int Listen(int socket, int backlog)
{
    int rc;

    rc = listen(socket, backlog);
    if (rc == -1) error("listen()");

    return rc;
}

//Системный вызов accept() используется с сокетами, ориентированными на установление соединения
//(SOCK_STREAM, SOCK_SEQPACKET). Она извлекает первый запрос на соединение из очереди ожидающих
//соединений прослушивающего сокета, sockfd, создаёт новый подключенный сокет и и возвращает новый
//файловый дескриптор, указывающий на сокет. Новый сокет более не находится в слушающем состоянии.
//Исходный сокет sockfd не изменяется при этом вызове.

//Аргумент addr --- это указатель на структуру sockaddr. В эту структуру помещается адрес ответной стороны
//в том виде, в каком он известен на коммуникационном уровне. Точный формат адреса, возвращаемого в параметре addr,
//определяется семейством адресов сокета (см. socket(2) и справочную страницу по соответствующему протоколу).
//Если addr равен NULL, то ничего не помещается; в этом случае addrlen не используется и также должен быть NULL.

//Через аргумент addrlen осуществляется возврат результата : вызывающая сторона должна указать в нём
//размер(в байтах) структуры, на которую указывает addr; при возврате он будет содержать реальный
//размер адреса ответной стороны.
int Accept(int socket, struct sockaddr* addr, socklen_t* addrlen)
{
    int rc;

    for (;;) {
        rc = accept(socket, addr, addrlen);
        if (rc != -1) break;
        //EINTR - Системный вызов прервал сигналом, который поступил до момента прихода допустимого соединения
        //ECONNABORTED - Соединение было прервано
        if (errno == EINTR || errno == ECONNABORTED) continue;
        error("accept()");
    }

    return rc;
}

//Закрывает файловый дескриптор, который после этого не ссылается ни на один и файл и может быть использован повторно.
void Close(int fd)
{
    int rc;

    for (;;) {
        rc = close(fd);
        if (!rc) break;
        if (errno == EINTR) continue;
        error("close()");
    }
}

//Вызов read() пытается прочитать count байт из файлового дескриптора fd в буфер, начинающийся по адресу buf.
size_t Read(int fd, void* buf, size_t count)
{
    ssize_t rc;

    for (;;) {
        //количество успешно прочитанных байтов (не более count)
        rc = read(fd, buf, count);
        if (rc != -1) break;
        if (errno == EINTR) continue;
        error("read()");
    }

    return rc;
}

//Пишет до count байт из буфера, на который указывает buf, в файле, на который ссылается файловый дескриптор fd.
size_t Write(int fd, const void* buf, size_t count)
{
    ssize_t rc;

    for (;;) {
        //В случае успеха возвращается количество записанных байтов.
        rc = write(fd, buf, count);
        if (rc != -1) break;
        //EINTR - Системный вызов прервал сигналом, который поступил до момента прихода допустимого соединения
        if (errno == EINTR) continue;
        error("write()");
    }
    //сколько записали (не более count)
    return rc;
}

//Распределяет size байтов и возвращает указатель на распределенную память. Память при этом не "очищается".
void* Malloc(size_t size)
{
    void* rc;

    rc = malloc(size);
    if (rc == NULL) error("malloc()");

    return rc;
}

//Функция получает в качестве аргументов указатель на поток, переменную типа pthread_t, в которую,
//в случае удачного завершения сохраняет id потока. pthread_attr_t – атрибуты потока.
//В случае если используются атрибуты по умолчанию, то можно передавать NULL.
//start_routin – это непосредственно та функция, которая будет выполняться в новом потоке.
//arg – это аргументы, которые будут переданы функции.
//Поток может выполнять много разных дел и получать разные аргументы.Для этого функция,
//которая будет запущена в новом потоке, принимает аргумент типа void*.
//За счёт этого можно обернуть все передаваемые аргументы в структуру.
//Возвращать значение можно также через передаваемый аргумент.

//В случае успешного выполнения функция возвращает 0.
//Если произошли ошибки, то могут быть возвращены следующие значения
//EAGAIN – у системы нет ресурсов для создания нового потока, или система не может больше создавать потоков,
// так как количество потоков превысило значение PTHREAD_THREADS_MAX(например, на одной из машин,
// которые используются для тестирования, это магическое число равно 2019)
//EINVAL – неправильные атрибуты потока(переданные аргументом attr)
//EPERM – Вызывающий поток не имеет должных прав для того, чтобы задать нужные параметры или политики планировщика.
void Pthread_create(pthread_t* thread, pthread_attr_t* attr,
    void* (*start_routine)(void*), void* arg)
{
    int rc;

    rc = pthread_create(thread, attr, start_routine, arg);
    if (rc) {
        errno = rc;
        error("pthread_create()");
    }
}

/*
 * Чтение строки из сокета.
 */
 //откуда + куда + сколько 
size_t reads(int socket, char* s, size_t size)
{
    char* p;
    size_t n, rc;

    /* Проверить корректность переданных аргументов. */
    if (s == NULL) {
        errno = EFAULT; //неверный адрес
        error("reads()");
    }
    //если хотели считать 0, ничего читать не нужно, уходим
    if (!size) return 0;

    p = s;
    size--;
    n = 0;
    while (n < size) {
        //читаем по одному байту
        rc = Read(socket, p, 1);
        if (rc == 0) break;
        //с новой строки не читаем
        if (*p == '\n') {
            p++;
            n++;
            break;
        }
        p++;
        n++;
    }
    *p = 0;

    return n;
}

/*
 * Запись count байтов в сокет.
 */
 //куда + откуда + сколько записывать
size_t writen(int socket, const char* buf, size_t count)
{
    const char* p;
    size_t n, rc;

    /* Проверить корректность переданных аргументов. */
    if (buf == NULL) {
        errno = EFAULT; //неверный адрес
        error("writen()");
    }

    //теперь указываем на переданное "откуда"
    p = buf;
    //переданное "сколько"
    n = count;
    while (n) {
        rc = Write(socket, p, count);
        //отнимает количество байт, которое удалось записать
        n -= rc;
        //сдвигаем указатель на начало незаписанных байтов
        p += rc;
    }

    return count;
}

void* serve_client(void* arg)
{
    int socket;
    //char s[MAXLINE];
    //ssize_t rc;

    /* Перевести поток в отсоединенное (detached) состояние. */
// когда он завершается, все занимаемые им ресурсы освобождаются и мы не можем отслеживать его завершение
//pthread_self - получение потоком своего идентификатора
    pthread_detach(pthread_self());

    //забираем дескриптор сокета из аргумента
    socket = *((int*)arg);
    free(arg);

    char *random_message = malloc(sizeof(char) * (rand() % 11 + 1));
    for (int i = 0; i < rand() % 11; i++) {
    	random_message[i] = 'a' + rand() % ('z' - 'a' + 1);
    }
    random_message[rand() % 10] = '\0';


    writen(socket, random_message, 64);

    Close(socket);

    return NULL;
}

int main(void)
{
    srand(time(NULL));

    //прослушивающий сокет ждет запроса на соединение, ни с кем не соединен
    int lsocket;    /* Дескриптор прослушиваемого сокета. */

    //активный сокет, соединен с удаленным активным сокетом через открытое соединение данных
    //уничтожится при закрытии соединения
    int csocket;    /* Дескриптор присоединенного сокета. */

    //adress_family + sin_port + sin_addr + sin_zero (не используется)
    struct sockaddr_in servaddr;

    int* arg;

    //идентификатор потока (по сути число)
    pthread_t thread;

    /* Создать сокет. */
//PF_INET - IP версии 4 (PF_UNIX, PF_LOCAL - протокол Unix для локального взаимодействия)
//SOCK_STREAM - надежный двусторонний обмен потоками байтов 
//(SOCK_DGRAM - ненадежный обмен на основе передачи датаграмм без установления соединения)
//Третий параметр указывает номер конкретного протокола в рамках указанного семейства для указанного типа сокета. 
//Как правило, существует единственный протокол для каждого типа сокета внутри каждого семейства.

//Вернет дескриптор сокета (некоторый идентификатор, например, номер записи в системной таблице)
    //PF_INET - Протоколы Интернет IPv4
    //SOCK_STREAM - jбеспечивает создание двусторонних, надёжных потоков байтов на основе установления соединения.
    //Может также поддерживаться механизм внепоточных данных.
    lsocket = Socket(PF_INET, SOCK_STREAM, 0);

    /* Инициализировать структуру адреса сокета сервера. */
//заполняем нулями
    memset(&servaddr, 0, sizeof(servaddr));
    //AF_INET соответствует Internet-домену (AF -> address family) 
    //(AF_UNIX для передачи данных используется файловая система ввода/вывода Unix)
    servaddr.sin_family = AF_INET;
    //htons преобразует u_short из хоста в сетевой порядок байтов TCP/IP (сетевой - человеческий, в памяти - обратный).
    servaddr.sin_port = htons(PORT);
    //аналогично для целого
    //INADDR_ANY - любой локальный интерфейс (= 0)
    //если нужен конкретный адрес = inet_addr ("192.168.78.2")
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Связать сокет с локальным адресом протокола. */
//После создания с помощью socket(2), сокет появляется в адресном пространстве (семействе адресов), но без назначенного адреса. 
//bind() назначает адрес, заданный в addr, сокету, указываемому дескриптором файла sockfd.
    Bind(lsocket, (SA*)&servaddr, sizeof(servaddr));

    /* Преобразовать неприсоединенный сокет в пассивный. */
//Вызов listen() помечает сокет, указанный в sockfd как пассивный,
//то есть как сокет, который будет использоваться для приёма запросов входящих соединений
    Listen(lsocket, BACKLOG);

    for (;;) {

        //извлекает первый запрос на соединение из очереди ожидающих соединений прослушивающего сокета,
        //создаёт новый подключенный сокет и и возвращает новый файловый дескриптор, указывающий на сокет
        csocket = Accept(lsocket, NULL, 0);

        arg = Malloc(sizeof(int));
        *arg = csocket;

        //указатель на поток + атрибуты потока + функция для выполнения + аргументы для функции
        Pthread_create(&thread, NULL, serve_client, arg);
    }

    return 0;
}
