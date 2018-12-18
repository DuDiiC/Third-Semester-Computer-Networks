/**
  * @author Maciej Dudek
  *
  * kompilacja z flaga -Wall przebiega poprawnie
  *
  * Rozwiazanie wariantu A, obsluga pamieci dzielonej przy uzyciu zbioru semaforow (szczegolowy opis w programie serwera)
  *
  * Klient dba o odpowiednia zmiane wartosci semaforow, w przypadku przerwania programu sygnalem Ctrl^C rezerwacja semafora
  * jest anulowana. Mozliwe jest pisanie przez wielu klientow na raz - maksymalnie przez tylu, ile zostalo jeszcze wolnych
  * slotow na wpisanie wiadomosci.
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>

#define ROZMIAR_WIADOMOSCI 128

/* ZMIENNE GLOBALNE */
key_t kluczIPC;
int IDPamieciDzielonej;
int IDZbioruSemaforow;

int pozycjaWolnegoSlotu = -1;
char trescWiadomosci[ROZMIAR_WIADOMOSCI];

int i;

/* elementy pamieci dzielonej */
int *liczbaSlotow;
struct wpis {
    uid_t UIDKlienta;
    char txt[ROZMIAR_WIADOMOSCI];
} *wpisy;

/* FUNKCJE - pelny opis przy implementacji */
void sygnalSIGINT(int signal);
key_t generujKlucz(char *nazwaPliku, int liczba);
int pobierzIDPamieciDzielonej(key_t klucz);
int pobierzIDZbioruSemaforow(key_t klucz, int liczbaSemaforow);
int wartoscSemafora(key_t id, int numer);
char *formatWpisu(int rozmiar);
char *formatWolnego(int rozmiar);

int main(int argc, char *argv[]) {

    /* SPRAWDZENIE LICZBY ARGUMENTOW */
    if(argc != 2) {
        printf("Nieprawidlowa liczba argumentow\n");
        exit(-1);
    }

    /* OBSLUGA SYGNALOW */
    signal(SIGINT, sygnalSIGINT);

    /* KLUCZ */
    kluczIPC = generujKlucz(argv[1], 1);

    /* PAMIEC WSPOLDZIELONA */
    IDPamieciDzielonej = pobierzIDPamieciDzielonej(kluczIPC);
    /*podlaczenie miejsca z iloscia slotow */
    if((liczbaSlotow = (int*)shmat(IDPamieciDzielonej, (void*)0, 0)) == (int*)-1) {
        perror("blad shmat (nie podpieto pamieci do wskaznika na liczbe slotow)");
        exit(errno);
    }
    /* przesuniecie na poczatek tablicy struktur z informacjami z ksiegi */
    wpisy = (struct wpis*)(liczbaSlotow+sizeof(int));

    /* SEMAFORY */
    IDZbioruSemaforow = pobierzIDZbioruSemaforow(kluczIPC, (*liczbaSlotow)+1);

     /* powitanie */
    printf("Klient ksiegi skarg i wnioskow wita!\n");
    printf("[%s %d %s (na %d)]\n",  formatWolnego(wartoscSemafora(IDZbioruSemaforow, 0)),
                                    wartoscSemafora(IDZbioruSemaforow, 0),
                                    formatWpisu(wartoscSemafora(IDZbioruSemaforow, 0)),
                                    *liczbaSlotow);

    /* szukanie wolnego slotu w pamieci dzielonej na wpisanie wiadomosci do ksiegi */
    for(i = 1; i <= *liczbaSlotow; i++) {
        if(wartoscSemafora(IDZbioruSemaforow, i) == 2) {
            pozycjaWolnegoSlotu = i;
            break;
        }
    }

    /* jesli nie znaleziono wolnego slotu (wartosc zmiennej pozycjaWolnegoSlotu ustawiona domyslnie na -1), zamkniecie programu */
    if(pozycjaWolnegoSlotu == -1) {
        printf("[Klient]: Brak miejsca w ksiedze na dokonanie wpisu\n");
        return 0;
    }

    printf("Napisz co ci lezy na watrobie:\n> ");

    /* deklaracja struktur odpowiedzialnych za operacje na semaforach */
    struct sembuf operacjaPierwszegoSem, operacjaWolnegoSem;

    /* zdefiniowanie jakiego rodziaju operacja ma byc wykonywana na semaforze blokujacym dostep do odpowiedniego slotu ksiegi */
    operacjaWolnegoSem.sem_num = pozycjaWolnegoSlotu;
    operacjaWolnegoSem.sem_op = -1;
    operacjaWolnegoSem.sem_flg = 0;
    /* rezerwacja miejsca dla wpisu na okreslonej pozycji */
    if(semop(IDZbioruSemaforow, &operacjaWolnegoSem, 1) == -1) {
        perror("blad semop (nie wykonano rezerwacji miejsca dla wpisu w semaforze)");
        exit(errno);
    }

    /* pobranie wiadomosci od klienta i zapisanie ich do struktury w pamieci dzielonej */
    /* obsluga przypadku, gdy klient probuje podac pusta wiadomosc */
    fgets(trescWiadomosci, ROZMIAR_WIADOMOSCI, stdin);
    trescWiadomosci[strcspn(trescWiadomosci, "\n")] = 0; /* fgets zapisuje rowniez znak konca linii, ktorego nie chcemy miec w wiadomosci */
    while(strlen(trescWiadomosci) == 0) { /*prosimy o ponowne wpisanie wiadomosci, dopoki klient podaje tylko pusta linie */
        printf("Nie mozna wprowadzac pustych wiadomosci, prosze podac niepusty komunikat lub nacisnac Ctrl^Z\n>");
        fgets(trescWiadomosci, ROZMIAR_WIADOMOSCI, stdin);
        trescWiadomosci[strcspn(trescWiadomosci, "\n")] = 0;
    }

    strcpy(wpisy[pozycjaWolnegoSlotu-1].txt, trescWiadomosci);
    wpisy[pozycjaWolnegoSlotu-1].UIDKlienta = getuid(); /* potrzebne aby wyswietlic nazwe uzytkownika, ktory wpisal wiadomosc */

    /* potwierdzenie rezerwacji przez wyzerowania semafora na okreslonej pozycji */
    if(semop(IDZbioruSemaforow, &operacjaWolnegoSem, 1) == -1) {
        perror("blad semop (nie potwierdzono rezerwacji miejsca dla wpisu w semaforze)");
        exit(errno);
    }

    /* zmniejszenie ilosci dostepnych slotow w pierwszym semaforze */
    operacjaPierwszegoSem.sem_num = 0;
    operacjaPierwszegoSem.sem_op = -1;
    operacjaPierwszegoSem.sem_flg = 0;
    if(semop(IDZbioruSemaforow, &operacjaPierwszegoSem, 1) == -1) {
        perror("blad semop (nie potwierdzono zajecia miejsca w ksiedze w semaforze przechowujacym ilosc wolnych slotow)");
        exit(errno);
    }

    printf("Dziekuje za dokonanie wpisu do ksiegi\n");

    return 0;
}

/**
  * Sygnal konczacy program, w przypadku wyslania go do programu, ktory juz zarezerwowal sobie semafor,
  * ale nie wpisal jeszcze wiadomosci, anuluje rezerwacje semafora na okreslonej pozycji
  */
void sygnalSIGINT(int signal) {
    if(pozycjaWolnegoSlotu != -1) {
        struct sembuf operacjaWolnegoSem;

        operacjaWolnegoSem.sem_num = pozycjaWolnegoSlotu;
        operacjaWolnegoSem.sem_op = 1;
        operacjaWolnegoSem.sem_flg = 0;
        if(semop(IDZbioruSemaforow, &operacjaWolnegoSem, 1) == -1) {
            perror("blad semop (nie anulowanie rezerwacji miejsca na wiadomosc w semaforze)");
            exit(errno);
        }
        printf("\n");
        exit(0);
    }
}

/**
  * Zwraca klucz pozwalajacy na utworzenie obiektow IPC.
  * Uzywa do tego funkcji ftok(), jako argument przyjmujacej nazwe pliku i zmienna calkowitoliczbowa.
  * !!! Do poprawnego dzialania wymaga identycznych argumentow co analogiczna funkcja w programie serwera,
  *     aby wygenerowac identyczny klucz.
  */
key_t generujKlucz(char *nazwaPliku, int liczba) {

    key_t klucz;
    if((klucz = ftok(nazwaPliku, liczba)) == -1) {
        perror("blad  ftok (nie utworzono klucza dla obiektow IPC)");
        exit(errno);
    }
    return klucz;
}

/**
  * Zwraca identyfikator do utworzonego przez serwer segmentu pamieci dzielonej.
  * Uzyskuje go poprzez uzycie funkcji shmget() z tym samym kluczem co na serwerze.
  */
int pobierzIDPamieciDzielonej(key_t klucz) {

    int id;
    if((id = shmget(klucz, 0, 0)) == -1) {
        perror("blad shmget (nie pobrano informacji o ID pamieci wspoldzielonej z serwerem)");
        exit(errno);
    }
    return id;
}

/**
  * Zwraca identyfikator do utworzonego przez serwer zbioru semaforow.
  * Uzyskuje go poprzez uzycie funkcji semget() z tym samym kluczem i liczba semaforow co na serwerze.
  */
int pobierzIDZbioruSemaforow(key_t klucz, int liczbaSemaforow) {

    int id;
    if((id = semget(klucz, liczbaSemaforow, 0)) == -1) {
        perror("blad semget (nie pobrano informacji o ID zbioru semaforow utworzonych w programie serwera)");
        exit(errno);
    }
    return id;
}

/**
  * Zwraca wartosc przechowywana w polu val unii semun dla semafora o podanym numerze,
  * w zbiorze o podanym ID. Osiaga to dzieki funkcji semctl(), ktora przy ustawieniu flagi
  * GETVAL zwraca wartosc wlasnie tego pola.
  */
int wartoscSemafora(key_t id, int numer) {

    int wartoscSemafora;
    if((wartoscSemafora = semctl(id, numer, GETVAL)) == -1) {
        perror("blad semctl (nie pobrano wartosci pola var w unii semun semafora)");
        exit(errno);
    }
    return wartoscSemafora;
}

/**
  * Formatowanie w zaleznosci od liczby wpisow
  */
char *formatWpisu(int rozmiar) {
    if(rozmiar == 1) {
        return "wpis";
    } else if(rozmiar%10 == 2 || rozmiar%10 == 3 || rozmiar%10 == 4) {
        if(rozmiar%100 != 12 && rozmiar%100 != 13 && rozmiar%100 != 14) {
            return "wpisy";
        } else {
            return "wpisow";
        }
    } else {
        return "wpisow";
    }
}

/**
  * Formatowanie w zaleznosci od liczby wpisow
  */
char *formatWolnego(int rozmiar) {
    if(rozmiar == 1) {
        return "Wolny";
    } else if(rozmiar%10 == 2 || rozmiar%10 == 3 || rozmiar%10 == 4) {
        if(rozmiar%100 != 12 && rozmiar%100 != 13 && rozmiar%100 != 14) {
            return "Wolne";
        } else {
            return "Wolnych";
        }
    } else {
        return "Wolnych";
    }
}
