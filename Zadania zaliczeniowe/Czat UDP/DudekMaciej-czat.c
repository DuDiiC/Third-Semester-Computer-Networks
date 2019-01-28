/**
  * @author Maciej Dudek
  */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

/**
 * Ustawiony na sztywno port, ktory bedzie uzywany w programie.
 */
#define PORT 6007

/**
 * Maksymalny rozmiar napisu przechowujacego nazwe uzytkownika czatu.
 */
#define USR_SIZE 24

/**
 * Maksymalny rozmiar wiadomosci wysylanej/odbieranej w trakcie dzialania czatu.
 */
#define MSG_SIZE 256

/**
 * Zmienna przechowujaca deskryptor gniazda, ktore bedzie mialo zostac zamkniete w przypadku pojawienia sie bledu,
 * badz zakonczenia programu planowo lub po otrzymaniu sygnalu SIGINT.
 */
int socket_descriptor;

/**
 * message_data jest struktura przechowujaca wiadomosc do wyslania/odebrania
 * - user odpowiada za nazwe uzytkownika podana podczas wywolywania programu/automatycznie ustalona na "NN"
 * - message jest tablica przechowujaca wiadomosc
 * - join_to_conv przechowuje informacje czy rozmowca wlasnie dolacza do rozmowy (wartosc 1) czy jest juz jej uczestnikiem (wartosc 0)
 */
struct message_data {
    char user[USR_SIZE];
    char message[MSG_SIZE];
    int join_to_conv;
};

/**
 * end_of_the_program jest funkcja do obslugi sygnalu zakonczenia procesu w funkcji signal.
 * Patrz zmienna {@link socket_descriptor}
 * @param signal bedzie parametrem przyjmujacym flage sygnalu SIGINT
 */
void end_of_the_program(int signal) {
    if(socket_descriptor) {                                 /* jesli zostala juz wywolana funkcja socket() */
        close(socket_descriptor);
    }
    exit(0);
}

int main(int argc, char *argv[]) {

    /* OBSLUGA SYGNALU SIGINT */
    signal(SIGINT, end_of_the_program);

    /* SPRAWDZENIE POPRAWNOSCI ILOSCI ARGUMENTOW */
    if(argc != 2 && argc != 3) {
        printf("Niepoprawna liczba argumentow wywolania programu.\n");
        printf("Wymagany adres domenowy lub IP oraz (opcjonalnie) nick uzytkownika.\n");
        exit(-1);
    }

    /* DEKLARACJA POTRZEBNYCH ZMIENNYCH I STRUKTUR */
    struct message_data msg1, msg2;                         /* sa to dwie struktury odpowiedzialne odpowiednio za:
                                                            msg1 - wiadomosci wysylane na czacie
                                                            msg2 - wiadomosci odbierane na czacie */
    struct addrinfo *info;                                  /* struktura potrzebna w funkcji getaddrinfo(), aby ja
                                                            potem rzutowac na odpowiednia strukture sickaddr_in */
    struct sockaddr_in my_addr, his_addr, *temp_addr;       /* trzy struktury odpowiedzialne odpowiednio za:
                                                            my_addr - informacje o uzytkowniku czatu
                                                            his_addr - informacje o naszym rozmowcy
                                                            temp_addr - pomocnicza struktura przydatna przy rzutowaniu
                                                                        ze struktury addrinfo */
    socklen_t addr_len;                                     /* pomocnicza zmienna potrzebna w funkcji recvfrom() */
    int fork_result;                                        /* przechowuje informacje zwrocona przez fork() */

    /* USTALENIE INFORMACJI O PODANYM HOSCIE */
    if(getaddrinfo(argv[1], "6007", NULL, &info) != 0) {
        perror("Blad funkcji getaddrinfo - bledny adres domenowy / IP.");
        exit(errno);
    }

    /* USTALENIE NICKU UZYTKOWNIKA */
    if(argc == 3) {                                             /* podano nick uzytkownika */
        if(strlen(argv[2]) >= USR_SIZE) {                       /* jesli jest za dlugi */
            printf("Blad - maksymalna dlugosc nicku to 24 znaki.\n");
            exit(-1);
        } else {
            strcpy(msg1.user, argv[2]);
        }
    } else {                                                    /* nie podano nicku uzytkownika */
        strcpy(msg1.user, "NN");
    }

    /* TWORZENIE GNIAZDA */
    if((socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Blad funkcji socket - nie utworzono gniazda.");
        exit(errno);
    }

    temp_addr = (struct sockaddr_in *)info->ai_addr;            /* dodatkowa zmienna przechowujaca adres IP,
                                                                z ktorym bedzie tworzone polaczenie */

    /* ODPOWIEDNIE UZUPELNIENIE INFORMACJI O STRUKTURACH */
    my_addr.sin_family      = AF_INET;                          /* IPv4 */
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);                /* dowolny interfejs */
    my_addr.sin_port        = htons(PORT);                      /* port hosta ustalony na sztywno */

    his_addr.sin_family     = AF_INET;                          /* IPv4 */
    his_addr.sin_addr       = temp_addr->sin_addr;              /* docelowy IP hosta */
    his_addr.sin_port       = htons(PORT);                      /* port hosta ustalony na sztywno */

    /* REJESTRACJA GNIAZDA W SYSTEMIE */
    if(bind(socket_descriptor, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("Blad funkcji bind - nie przypisano gniazda do portu.");
        close(socket_descriptor);
        exit(errno);
    }

    /* DOLACZENIE DO CZATU */
    printf("Rozpoczytam czat z %s. Napisz <koniec> by zakonczyc czat.\n", inet_ntoa(his_addr.sin_addr));
    msg1.join_to_conv = 1;                                      /* dolacza do czatu */

    if(sendto(socket_descriptor, &msg1, sizeof(msg1), 0, (struct sockaddr *)&his_addr, sizeof(his_addr)) < 0) {
        perror("Blad funkcji sendto - nie wyslano informacji o dolaczeniu do czatu.");
        close(socket_descriptor);
        exit(errno);
    }
    msg1.join_to_conv = 0;                                      /* dolaczono do czatu */

    /* FORKOWANIE PROCESU, ABY JEDEN NASLUCHIWAL WIADOMOSCI A DRUGI WYSYLAL JE DO ROZMOWCY */
    if((fork_result = fork()) < 0) {
        perror("Blad funkcji fork - nie utworzono procesu potomnego.");
        close(socket_descriptor);
        exit(errno);
    } else if(fork_result == 0) {                               /* proces potomka odpowiedzialny za odbieranie wiadomosci */
        while(1) {
            /* POBRANIE WIADOMOSCI OD ROZMOWCY */
            if(recvfrom(socket_descriptor, &msg2, sizeof(msg2), 0, (struct sockaddr *)&his_addr, &addr_len) < 0) {
                perror("Blad funkcji recvfrom - nie pobrano wiadomosci.");
                close(socket_descriptor);
                exit(errno);
            }

            /* WLASCIWA OBSLUGA WYPISYWANIA W CZACIE */
            if(msg2.join_to_conv) {                             /* wypisuje informacje ze dolaczyl nowy uzytkownik */
                printf("\n[%s (%s) dolaczyl do rozmowy]\n", msg2.user, inet_ntoa(his_addr.sin_addr));
            } else if(strcmp(msg2.message, "<koniec>") == 0) {  /* wypisuje informacje o opuszczeniu czatu przez uzytkownika */
                printf("\n[%s (%s) zakonczyl rozmowe]\n", msg2.user, inet_ntoa(his_addr.sin_addr));
            } else {                                            /* wypisujemy zwykla wiadomosc */
                printf("\n[%s (%s)]: %s\n", msg2.user, inet_ntoa(his_addr.sin_addr), msg2.message);
            }

            /* ponowne wypisanie nazwy uzytkownika i znaku zachety i 'wypchniecie' wszystkich informacji na wyjscie */
            printf("[%s]> ", msg1.user);
            if(fflush(stdout) != 0) {
                perror("Blad funkcji fflush - nie mozna wypchnac informacji na standardowe wyjscie.");
                close(socket_descriptor);
                exit(errno);
            }
        }
    } else {                                                    /* proces rodzica odpowiedzialny za wysylanie wiadomosci */
        while(1) {
            /* POBRANIE WIADOMOSCI OD UZYTKOWNIKA */
            printf("[%s]> ", msg1.user);
            fgets(msg1.message, MSG_SIZE, stdin);
            msg1.message[strlen(msg1.message)-1] = '\0';

            /* WYSLANIE WIADOMOSCI DO ROZMOWCY */
            if(sendto(socket_descriptor, &msg1, sizeof(msg1), 0, (struct sockaddr *)&his_addr, sizeof(his_addr)) < 0) {
                perror("Blad funkcji sendto - nie wyslano wiadomosci do rozmowcy");
                close(socket_descriptor);
                exit(errno);
            }

            /* po wyslaniu wiadomosci trzeba zakonczyc proces potomny, aby nie zostawiac zombies */
            if(strcmp(msg1.message, "<koniec>") == 0) {
                kill(fork_result, SIGINT);
                close(socket_descriptor);
                exit(0);
            }
        }
    }

    return 0;
}
