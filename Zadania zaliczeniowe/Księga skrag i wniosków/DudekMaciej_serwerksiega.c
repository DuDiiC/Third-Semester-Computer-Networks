/**
  * @author Maciej Dudek
  *
  * kompilacja z flaga -Wall przebiega poprawnie
  *
  * Rozwiazanie wariantu A, obsluga pamieci dzielonej przy uzyciu zbioru semaforow:
  *     - pierwszy semafor przechowuje ilosc dostepnych jeszcze slotow pamieci
  *     - poszczegolne semafory maja wartosci 2, 1, 0, odpowiednio:
  *         > mozna wpisac komunikat
  *         > klient zarezerwowal slot pamieci na wpisanie wiadomosci, ale jeszcze tego wpisu nie dokonal
  *         > klient wpisal wiadomosc do tego slotu, semafor jest opuszczony i blokuje dostep do tego slotu pozostalym klientom
  * Zaimplementowana zostala pelna obsluga bledow, przy konczeniu programu nie zostawia w pamieci zadnych utworzonych
  * uprzednio obiektow IPC.
  */

#include <stdio.h>
#include <stdlib.h>

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

int i;

/* elementy pamieci dzielonej */
int *liczbaSlotow;
struct wpis {
    uid_t UIDKlienta;
    char txt[ROZMIAR_WIADOMOSCI];
} *wpisy;

/* FUNKCJE - pelny opis przy implementacji */
void sygnalSIGINT(int signal);
void sygnalSIGTSTP(int signal);

key_t generujKlucz(char *nazwaPliku, int liczba);
int generujIDPamieciDzielonej(key_t klucz);
int generujIDZbioruSemaforow(key_t klucz, int liczbaSemaforow);
int wartoscSemafora(key_t id, int numer);
char *nazwaUzytkownika(uid_t uid);
char *formatWpisu(int rozmiar);

void usunSegmentPamieciDzielonej();
void usunZbiorSemaforow();

int main(int argc, char *argv[]) {

    /* SPRAWDZENIE LICZBY ARGUMENTOW */
    if(argc != 3) {
        printf("Nieprawidlowa liczba argumentow\n");
        exit(-1);
    }

    /* OBSLUGA SYGNALOW */
    signal(SIGINT, sygnalSIGINT);
    signal(SIGTSTP, sygnalSIGTSTP);

    struct shmid_ds infoPamieciDzielonej; /* zawiera podstawowe informacje o segmencie pamieci dzielonej */
    /* zapisanie informacji o ilosci slotow w ksiedzie */
    int sloty = atoi(argv[2]);
    liczbaSlotow = &sloty;

    printf("[Serwer]: ksiega skarg i wnioskow (WARIANT A)\n");

    /* KLUCZ */
    printf("[Serwer]: tworze klucz... ");
    kluczIPC = generujKlucz(argv[1], 1);
#if defined (__linux)
    printf("OK (klucz: %d)\n", kluczIPC); /*  linux wymaga, zeby key_t bylo wyswietlane jako int */
#else
    printf("OK (klucz: %ld)\n", kluczIPC); /* np. FreeBSD wymaga, zeby key_t bylo wyswietlane jako long */
#endif
    /* PAMIEC WSPOLDZIELONA */
    printf("[Serwer]: tworze segment pamieci wspolnej dla ksiegi na %d %s...\n", *liczbaSlotow, formatWpisu(*liczbaSlotow));
    IDPamieciDzielonej = generujIDPamieciDzielonej(kluczIPC);
    if(shmctl(IDPamieciDzielonej, IPC_STAT, &infoPamieciDzielonej) == -1) {
        perror("blad shmctl (nie pobrano informacji o pamieci dzielonej do struktury shmid_ds");
        usunSegmentPamieciDzielonej();
        exit(errno);
    }
    printf("          OK (id: %d, rozmiar: %zub)\n", IDPamieciDzielonej, infoPamieciDzielonej.shm_segsz);

    printf("[Serwer]: Dolaczam pamiec wspolna... ");
    /* podlaczanie miejsca z iloscia slotow */
    if((liczbaSlotow = (int*)shmat(IDPamieciDzielonej, (void*)0, 0)) == (int*)-1) {
        perror("blad shmat (nie podlaczono pamieci dzielonej do wskaznika na liczbe slotow)");
        usunSegmentPamieciDzielonej();
        exit(errno);
    }
    /* przesuniecie do poczatku 'tablicy' struktur przechowujacych informacje o wpisach */
    wpisy = (struct wpis*)(liczbaSlotow+sizeof(int));
    printf("OK (adres slotow: %lX)\n", (long int)liczbaSlotow);

    *liczbaSlotow = atoi(argv[2]);

    /* SEMAFORY */
    printf("[Serwer]: Tworze zbior semaforow dla ksiegi na %d %s... ", *liczbaSlotow, formatWpisu(*liczbaSlotow));
    IDZbioruSemaforow = generujIDZbioruSemaforow(kluczIPC, (*liczbaSlotow)+1);

    /* nadanie wartosci poczatkowych semaforom */
    union semun {
        int val;
        struct semid_ds *buf;
        ushort *array;
    } uniaSemafora;

    /* pierwszy semafor */
    uniaSemafora.val = *liczbaSlotow;
    if(semctl(IDZbioruSemaforow, 0, SETVAL, uniaSemafora) == -1) {
        perror("blad semctl (nie ustawiono wartosci pierwszego semafora)");
        usunSegmentPamieciDzielonej();
        usunZbiorSemaforow();
        exit(errno);
    }

    /* pozostale semafory */
    uniaSemafora.val = 2;
    for(i = 1; i <= *liczbaSlotow; i++) {
        if(semctl(IDZbioruSemaforow, i, SETVAL, uniaSemafora) == -1) {
            perror("blad semctl (nie ustawiono wartosci jednego z semaforow)");
            usunSegmentPamieciDzielonej();
            usunZbiorSemaforow();
            exit(errno);
        }
    }

    printf("OK (id: %d)\n", IDZbioruSemaforow);

    /* OCZEKIWANIE NA SYGNALY */
    printf("[Serwer]: nacisnij Ctrl^Z by wyswietlic stan ksiegi...\n");
    printf("          nacisnij Ctrl^C aby zakonczyc prace serwera\n");

    while(1) {}

    return 0;
}

/**
  * Sygnal konczacy program, sprzata semafory i segment pamiec dzielonej,
  * aby nie zostawic po programie zadnych smieci (nieusunietych obiektow IPC).
  */
void sygnalSIGINT(int signal) {
    printf("\n[Serwer]: dostalem SIGINT => koncze i sprzatam...\n");
    printf("(odlaczenie pamieci... %s, usuniecie semaforow... %s)\n",
                (shmctl(IDPamieciDzielonej, IPC_RMID, (void *)0) == 0) ? "OK" : "blad shmctl",
                (semctl(IDZbioruSemaforow, IPC_RMID, 0) == 0) ? "OK" : "blad semctl");
    exit(0);
}

/**
  * Wypisuje aktualna zawartosc ksiegi. Jesli ksiega jest pusta rowniez wypisuje stosowna inforamcje.
  */
void sygnalSIGTSTP(int signal) {

    if(wartoscSemafora(IDZbioruSemaforow, 0) == *liczbaSlotow) {
        printf("\nKsiega skarg i wnioskow jest pusta\n");
    } else {
        printf("\n___________  Ksiega skarg i wnioskow:  ___________\n");
        for(i = 1; i <= *liczbaSlotow; i++) {
            /* jesli semafor jest opuszczony wypisz informacje z pamieci dzielonej */
            if(wartoscSemafora(IDZbioruSemaforow, i) == 0) {
                printf("[%s] %s\n", nazwaUzytkownika(wpisy[i-1].UIDKlienta), wpisy[i-1].txt);
            }
        }
    }
}

/**
  * Zwraca klucz pozwalajacy na utworzenie obiektow IPC.
  * Uzywa do tego funkcji ftok(), jako argument przyjmujacej nazwe pliku i zmienna calkowitoliczbowa.
  */
key_t generujKlucz(char *nazwaPliku, int liczba) {

    key_t klucz;
    if((klucz = ftok(nazwaPliku, liczba)) == -1) {
        perror("blad  funkcji ftok (nie utworzono klucza dla obiektow IPC)");
        exit(errno);
    }
    return klucz;
}

/**
  * Zwraca identyfikator do utworzonego segmentu pamieci dzielonej.
  * Tworzy segment o podanym kluczu, rozmiarze, rozmiarze i flagach przy uzyciu funkcji smhget().
  */
int generujIDPamieciDzielonej(key_t klucz) {

    int id;
    if((id = shmget(klucz, sizeof(int)+sizeof(struct wpis)*(*liczbaSlotow), 0666 | IPC_CREAT | IPC_EXCL)) == -1) {
        perror("blad shmget (nie utworzono segmentu pamieci dzielonej, nie zapisano ID tej pamieci)");
        exit(errno);
    }
    return id;
}

/**
  * Zwraca identyfikator do utworzonego zbioru semaforow.
  * Tworzy zbior o podanym kluczu, ilosci semaforow w zbiorze i flagach przy uzyciu funkcji semget()
  */
int generujIDZbioruSemaforow(key_t klucz, int liczbaSemaforow) {

    int id;
    if((id = semget(klucz, liczbaSemaforow, 0666 | IPC_CREAT | IPC_EXCL)) == -1) {
        perror("blad semget (nie utworzono zbioru semaforow, nie pobrano ID tego zbioru)");
        usunSegmentPamieciDzielonej();
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
        perror("blad funkcji semctl (nie pobrano wartosci pola var w unii semun semafora)");
        usunSegmentPamieciDzielonej();
        usunZbiorSemaforow();
        exit(errno);
    }
    return wartoscSemafora;
}

/**
  * Funkcja przyjmujaca jako argument UID, przy jego pomocy zwraca nazwe uzytkownika.
  */
char *nazwaUzytkownika(uid_t uid) {
    struct passwd *pws;
    pws = getpwuid(uid);
    return pws->pw_name;
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
  * Funkcja uzywana przy obsludze bledow do usuniecia segmentu pamieci dzielonej,
  * aby nie pozostala w systemie po blednym zakonczeniu programu.
  */
void usunSegmentPamieciDzielonej() {
    if(shmctl(IDPamieciDzielonej, IPC_RMID, (void *)0) == -1) {
        perror("blad shmctl (nie usunieto segmentu pamieci dzielonej)");
        exit(errno);
    }
}

/**
  * Funkcja uzywana przy obsludze bledow do usuniecia zbioru semaforow,
  * aby nie pozostal w systemie po blednym zakonczeniu programu.
  */
void usunZbiorSemaforow() {
    if(semctl(IDZbioruSemaforow, IPC_RMID, 0) == -1) {
        perror("blad semctl (nie usunieto zbioru semaforow)");
        exit(errno);
    }
}
