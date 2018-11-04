/**
  * @author Maciej Dudek
  *
  * kompilacja z flaga -Wall zarowno na ultra60 jak i na aleks-2 przebiega bez problemow
  *
  * dodatkowe funkcjonalnosci:
  *     > sortowanie alfabetyczne po nazwie listowanych plikow (zasstosowany algorytm sortowania babelkowego wg kodow ASCII znakow)
  *     > dynamiczne ustalenie szerokosci poszczegolnych kolumn (ilosci dowiazan twardych, nazwa uzytkownika, nazwy grupy, rozmiaru pliku)
  *     > przy wypisywaniu daty (w wersji bez argumetu) program uwzglednia czy plik ostatnio byl modyfikowany w aktualnym roku (2018), czy
  *       wczesniej -  w pierwszym wypadku wyswietlona zostanie godzina, w drugim rok przy wypisywaniu daty ostatniej modyfikacji
  */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>

typedef struct { /* struktura przechowujaca informacje do wyswietlenia dla kazdego pliku */

    char prawaDostepu[11];
    int linkiTwarde;
    char wlasciciel[35];
    int dlugoscNazwyWlasciciela;
    char grupa[35];
    int dlugoscNazwyGrupy;
    int rozmiar;
    int dzien;
    char miesiac[4];
    int rok;
    int godzina;
    int minuta;
    int sekunda;
    char nazwa[256];
    char nazwaZrodla[256];

} informacjeDoWypisania;

/* funkcje do czesci pierwszej */
void odczytajLiczbeTwardychDowiazan(nlink_t dowiazanTwardych, informacjeDoWypisania *elementTablicy);
void odczytajNazweWlasciciela(uid_t idWlasciciela, informacjeDoWypisania *elementTablicy);
void odczytajNazweGrupy(gid_t idGrupy, informacjeDoWypisania *elementTablicy);
void odczytajDate(time_t data, informacjeDoWypisania *elementTablicy);
void odczytajNazwe(char *nazwa, informacjeDoWypisania *elementTablicy);
void posortujPlikiWTablicy(informacjeDoWypisania tablica[], int rozmiar);
int dlugoscPolaLinkow(informacjeDoWypisania tablica[], int rozmiar);
int dlugoscPolaWlasciciela(informacjeDoWypisania tablica[], int rozmiar);
int dlugoscPolaGrupy(informacjeDoWypisania tablica[], int rozmiar);
int dlugoscPolaRozmiaru(informacjeDoWypisania tablica[], int rozmiar);
void wypiszTablice(informacjeDoWypisania tablica[], int rozmiar, int dlLinku, int dlUzytkownika, int dlGrupy, int dlRozmiaru);

/* funkcje do czesci drugiej */
void wypiszInformacje(char *nazwa, informacjeDoWypisania info, struct stat *plik);
char* typPliku(char typ);
void sciezkaDoPliku(char* nazwa);
void sciezkaNaLink(char* nazwa);
char* bajty(int rozmiar);
char* uprawnienia(char prawaDostepu[11]);
void wypiszCzas(struct stat *plik);
void podajCzas(time_t t);
void drukujZawartosc(char* nazwaPliku);

/* funkcje do obu czesci */
void odczytajTypIPrawaDostepu(mode_t typIUprawnienia, informacjeDoWypisania *info);
void odczytajRozmiarPliku(off_t rozmiar, informacjeDoWypisania *info);

int main(int argc, char **argv) {

    /* PIERWSZA WERSJA BEZ PARAMETRU */
    if(argc == 1) {

        struct dirent *plik; /* struktura wskazujaca na konkretny element katalogu */
        DIR *sciezka; /* struktura reprezentujaca strumien sciezki */
        int liczbaPlikowWKatalogu = 0;

        /* USTALENIE ILOSCI PLIKOW W KATALOGU */
        sciezka = opendir(".");
        if( sciezka == NULL ) { /* otwarcie katalogu roboczego, aby sprawdzic w nim ilosc plikow, informacja ta jest potrzebna do deklaracji tablicy struktur */
            perror("Blad przy otwieraniu katalogu");
            exit(errno);
        }
        while((plik = readdir(sciezka)) != NULL) { /* zliczanie plikow w katalogu */
            liczbaPlikowWKatalogu++;
        }

        if(closedir(sciezka) != 0) { /* zamkniecie katalogu wraz z obsluga bledow */
            perror("Blad zamykania katalogu");
            exit(errno);
        }

        /* TWORZENIE TABLICY WSZYSTKICH PLIKOW WRAZ Z ZAWARTOSCIA DO WYPISANIA */
        informacjeDoWypisania tablicaPlikow[liczbaPlikowWKatalogu]; /* tablica na wszystkie struktury z informacjami o pliku */

        sciezka = opendir(".");
        if(sciezka == NULL) {
            perror("Blad przy otwieraniu katalogu");
            exit(errno);
        }

        int i = 0; /*iterowanie po elementach tablicy */
        while((plik = readdir(sciezka)) != NULL) {
            struct stat *informacjeOPliku; /* struktura zawierajaca informacje o pliku */
            informacjeOPliku = malloc(sizeof(struct stat)); /* przydzielenie pamieci strukturze */
            if(informacjeOPliku == NULL && errno != 0) {
                perror("Blad alokacji pamieci dla struktury stat");
                exit(errno);
            }
            if(lstat(plik->d_name, informacjeOPliku) != 0) { /* pobieranie informacji o pliku do struktury */
                perror("Blad wczytywania danych do struktury stat");
                exit(errno);
            }

            /* WCZYTYWANIE DO STRUKTURY INFORMACJI POTRZEBNYCH DO WYPISANIA PRZEZ PROGRAM */
            odczytajTypIPrawaDostepu(informacjeOPliku->st_mode, &tablicaPlikow[i]);

            odczytajLiczbeTwardychDowiazan(informacjeOPliku->st_nlink, &tablicaPlikow[i]);

            odczytajNazweWlasciciela(informacjeOPliku->st_uid, &tablicaPlikow[i]);

            odczytajNazweGrupy(informacjeOPliku->st_gid, &tablicaPlikow[i]);

            odczytajRozmiarPliku(informacjeOPliku->st_size, &tablicaPlikow[i]);

            odczytajDate(informacjeOPliku->st_mtime, &tablicaPlikow[i]);

            odczytajNazwe(plik->d_name, &tablicaPlikow[i]);

            free(informacjeOPliku); /* zwolnienie pamieci dla niepotrzebnej juz struktury stat */
            ++i;
        }

        if(closedir(sciezka) != 0) {
            perror("Blad zamykania katalogu");
            exit(errno);
        }

        /* SORTOWANIE I WYPISANIE ODPOWIEDNIO ZFORMATOWANYCH DANYCH */
        posortujPlikiWTablicy(tablicaPlikow, liczbaPlikowWKatalogu);

        wypiszTablice(tablicaPlikow, liczbaPlikowWKatalogu,
                      dlugoscPolaLinkow(tablicaPlikow, liczbaPlikowWKatalogu),
                      dlugoscPolaWlasciciela(tablicaPlikow, liczbaPlikowWKatalogu),
                      dlugoscPolaGrupy(tablicaPlikow, liczbaPlikowWKatalogu),
                      dlugoscPolaRozmiaru(tablicaPlikow, liczbaPlikowWKatalogu));

    }
    /* DRUGA WERSJA Z PARAMETREM */
    else if(argc == 2) {

        /* analogicznie jak wyzej, tworzenie struktur stat oraz informacjeDoWypisania i przydzielenie pamieci dla stat */
        informacjeDoWypisania infoOPliku;
        struct stat *informacjeOPliku;
        informacjeOPliku = malloc(sizeof(struct stat));
        if(informacjeOPliku == NULL && errno != 0) {
            perror("Blad alokacji plamieci dla struktury stat");
            exit(errno);
        }
        if(lstat(argv[1], informacjeOPliku) != 0) { /* zapisanie do struktury informacji o pliku */
            perror("Blad wczytywania danych do struktury stat");
            exit(errno);
        }

        /* zapisanie potrzebnych do wyswietlenia informacji */
        odczytajTypIPrawaDostepu(informacjeOPliku->st_mode, &infoOPliku);
        odczytajRozmiarPliku(informacjeOPliku->st_size, &infoOPliku);

        /* wypisanie odpowiednio zformatowanych danych */
        wypiszInformacje(argv[1], infoOPliku, informacjeOPliku);

        free(informacjeOPliku); /* zwolnienie pamieci dla niepotrzebnej juz struktury stat */

    } else {
        printf("Nieprawidlowa liczba argumentow");
        exit(errno);
    }

    return 0;
}

void odczytajTypIPrawaDostepu(mode_t typIUprawnienia, informacjeDoWypisania *info) {

    /* funkja ta wykorzystuje odpowiednie funkcje maskujace dzialajace razem z mode_t, aby zwrocic informacje mozliwe do odczytania
       ze zmiennej tego typu, to znaczy typ pliku oraz uprawnienia go dotyczace */

    strcpy(info->prawaDostepu, "?---------"); /* domyslnie nie znamy typu pliku i ustawiamy uprawnienia na 000 */

    /* odczyt typu pliku */
    if( S_ISLNK(typIUprawnienia) ) {
        info->prawaDostepu[0] = 'l';
    } else if( S_ISDIR(typIUprawnienia) ) { /* katalog */
        info->prawaDostepu[0] = 'd';
    } else if( S_ISCHR(typIUprawnienia) ) { /* urzadzenie znakowe */
        info->prawaDostepu[0] = 'c';
    } else if( S_ISBLK(typIUprawnienia) ) { /* urzadzenie blokowe */
        info->prawaDostepu[0] = 'b';
    } else if( S_ISFIFO(typIUprawnienia) ) { /* potok z nazwa */
        info->prawaDostepu[0] = 'p';
    } else if( S_ISSOCK(typIUprawnienia) ) { /* gniazdo */
        info->prawaDostepu[0] = 's';
    } else if( S_ISREG(typIUprawnienia) ) { /* zwykly plik */
        info->prawaDostepu[0] = '-';
    }

    /*  sprawdzenie praw dostepu odbywa sie poprzez ustawienie maski z odpowiednia zdefiniowana
        stala - jesli wynik jest rozny od 0, nalezy ustawic uprawnienia */
    /* odczyt uprawnienien uzytkownika */
    if( typIUprawnienia & S_IRUSR )
        info->prawaDostepu[1] = 'r';
    if( typIUprawnienia & S_IWUSR )
        info->prawaDostepu[2] = 'w';
    if( typIUprawnienia & S_IXUSR )
        info->prawaDostepu[3] = 'x';
    if( typIUprawnienia & S_ISUID ) /* w przypadku ustawionego SUID */
        info->prawaDostepu[3] = 's';
    /* odczyt uprawnien grupy */
    if( typIUprawnienia & S_IRGRP )
        info->prawaDostepu[4] = 'r';
    if( typIUprawnienia & S_IWGRP )
        info->prawaDostepu[5] = 'w';
    if( typIUprawnienia & S_IXGRP )
        info->prawaDostepu[6] = 'x';
    if( typIUprawnienia & S_ISGID ) /* w przypadku ustawionego SGID */
        info->prawaDostepu[6] = 's';
    /* odczyt uprawnien pozostalych */
    if( typIUprawnienia & S_IROTH )
        info->prawaDostepu[7] = 'r';
    if( typIUprawnienia & S_IWOTH )
        info->prawaDostepu[8] = 'w';
    if( typIUprawnienia & S_IXOTH )
        info->prawaDostepu[9] = 'x';
    if( typIUprawnienia & S_ISVTX ) /* w przypadku ustawionego sticky bit */
        info->prawaDostepu[9] = 't';

    info->prawaDostepu[10] = '\0'; /* dopisanie na koncu znaku konca napisu */
}

void odczytajLiczbeTwardychDowiazan(nlink_t dowiazanTwardych, informacjeDoWypisania *elementTablicy) {
    elementTablicy->linkiTwarde = (int) dowiazanTwardych;
}

void odczytajNazweWlasciciela(uid_t idWlasciciela, informacjeDoWypisania *elementTablicy) {
    struct passwd *wlasciciel; /*struktura przechowuje informacje o wlascicielu */
    /*pobieranie do struktury informacji o wlascicielu */
    if((wlasciciel = getpwuid(idWlasciciela)) == NULL) { /* w tym przypadku nie byl mozliwy odczyt nazwy, zatem do zmiennej przypiszemy ID */
        printf("Blad wczytywania informacji o wlascicielu %d do struktury, zamiast nazwy zostanie wyswietlone ID\n", idWlasciciela);
        sprintf(elementTablicy->wlasciciel, "%d", idWlasciciela);

    } else sprintf(elementTablicy->wlasciciel, "%s", wlasciciel->pw_name); /* zapisanie nazwy do odpowiedniego elementu struktury */

    elementTablicy->dlugoscNazwyWlasciciela = strlen(elementTablicy->wlasciciel); /* przypisanie odpowiedniej wartosci do zmiennej struktury informacjeDoWypisania przechowujacej dlugosc napisu nazwy wlasciciela */
}

void odczytajNazweGrupy(gid_t idGrupy, informacjeDoWypisania *elementTablicy) {
    /* wszystkie operacje sa analogiczne jak w funkcji odczytajNazweWlasciciela, z ta roznica ze uzywamy struktury odpowiedzialnej za informacje o grupie */
    struct group *grupa; /* struktura przechowujaca informacje o grupie */
    /* pobieranie do struktury informacji o grupie */
    if((grupa = getgrgid(idGrupy)) == NULL) {
        printf("Blad wczytywania informacji o grupie %d do struktury, zamiast nazwy zostanie wyswietlone ID\n", idGrupy);
        sprintf(elementTablicy->grupa, "%d", idGrupy); /* zapisanie ID jako nazwy grupy */
    }
    else sprintf(elementTablicy->grupa, "%s", grupa->gr_name); /* zapisanie nazwy do odpowiedniego elementu struktury */

    elementTablicy->dlugoscNazwyGrupy = strlen(elementTablicy->grupa); /* przypisanie dlugosci napisu */
}

void odczytajRozmiarPliku(off_t rozmiar, informacjeDoWypisania *info) {
    info->rozmiar = (int) rozmiar;
}

void odczytajDate(time_t data, informacjeDoWypisania *elementTablicy) {

    struct tm *czasPoModyfikacji; /* aby moc odczytac dane ze zmiennej time_t trzeba przekonwertowac je do innej struktury */
    czasPoModyfikacji = localtime(&data); /* teraz wszystkie dane dotyczace daty znajduja sie w strukturze tm, zawierajacej zmienne
                                             integer dla kazdego elementu: dnia, miesiaca, godziny itd. */

    /* ustalenie dnia */
    elementTablicy->dzien = czasPoModyfikacji->tm_mday;

    /* ustalenie miesiaca */
    char miesiace[][12] = {"sty", "lut", "mar", "kwi", "maj", "cze", "lip", "sie", "wrz", "paz", "lis", "gru" };
    strcpy(elementTablicy->miesiac, miesiace[czasPoModyfikacji->tm_mon]);

    /* trzeba ustalic czy wypisac rok, czy wypisac dokladna godzine modyfikacji - zalezy to od tego, czy rok odpowiada aktualnemu */
    if(czasPoModyfikacji->tm_year == 118) { /* ustalenie godziny w formacie hh:mm, warunek jest taki, poniewaz zmienna przechowuje date od 1900 roku */
        elementTablicy->godzina = czasPoModyfikacji->tm_hour;
        elementTablicy->minuta = czasPoModyfikacji->tm_min;

        elementTablicy->rok = 0; /* rok "zerowy" bedzie warunkiem instrukcji warunkowej, zadecyduje czy wypisac godzine czy rok */

    } else { /* ustalenie roku */
        elementTablicy->rok = (czasPoModyfikacji->tm_year)+1900; /* aby uzyskac aktualna date, do zmiennej trzeba dodac 1900 */
    }
}

void odczytajNazwe(char *nazwa, informacjeDoWypisania *elementTablicy) {
    strncpy(elementTablicy->nazwa, nazwa, 32); /* przypisanie nazwy pliku */
    if(elementTablicy->prawaDostepu[0] == 'l') { /* w przypadku, gdy typem pliku jest link symboliczny,
                                                    chcemy rowniez uzyskac informacje na temat pliku na
                                                    ktory dowiazanie wskazuje */
        char zrodlo[256]; /* zmienna przechowujaca nazwe pliku na ktory wskazuje dowiazanie */
        int dlugosc = readlink(nazwa, zrodlo, sizeof(zrodlo)); /*   funkcja readlink zwraca dlugosc lancucha, ktory zostal
                                                                    wpisany do zmiennej jako nazwa pliku */
        zrodlo[dlugosc] = '\0'; /* readline przy przypisywaniu nazwy nie uwzglednia znaku konca napisu, trzeba go dodac recznie */
        strncpy(elementTablicy->nazwaZrodla, zrodlo, 32);
    } else {
        elementTablicy->nazwaZrodla[0] = '\0';
    }
}

void posortujPlikiWTablicy(informacjeDoWypisania tablica[], int rozmiar) { /* do posortowania tablicy uzywam algorytmu sortowania babelkowego */
    informacjeDoWypisania temp;
    int i, j;
    for(i = 0; i < rozmiar; i++) {
        for(j = i+1; j < rozmiar; j++) {
            if(strcmp(tablica[i].nazwa, tablica[j].nazwa) > 0) {
                temp = tablica[i];
                tablica[i] = tablica[j];
                tablica[j] = temp;
            }
        }
    }
}


int dlugoscPolaLinkow(informacjeDoWypisania tablica[], int rozmiar) {

    int najwiecejLinkow = tablica[0].linkiTwarde;
    int i;

    for(i = 1; i < rozmiar; i++) { /* ustalenie liczby dowiazan dla pliku, ktory zawiera ich najwiecej */
        if(najwiecejLinkow < tablica[i].linkiTwarde) {
            najwiecejLinkow = tablica[i].linkiTwarde;
        }
    }

    int iloscPol = 0;
    while(najwiecejLinkow != 0) { /* ustalenie ilosci cyfr liczby, ktora odpowiada za najwiecej dowiazan */
        iloscPol++;
        najwiecejLinkow /= 10;
    }

    return iloscPol;

}

int dlugoscPolaWlasciciela(informacjeDoWypisania tablica[], int rozmiar) {

    int najdluzszaNazwa = tablica[0].dlugoscNazwyWlasciciela;
    int i;

    for(i = 1; i < rozmiar; i++) {
        if(najdluzszaNazwa < tablica[i].dlugoscNazwyWlasciciela) { /* jesli najdlusza jak dotychczas nazwa jest krotsza niz aktualna, podmien */
            najdluzszaNazwa = tablica[i].dlugoscNazwyWlasciciela;
        }
    }

    return najdluzszaNazwa;
}

int dlugoscPolaGrupy(informacjeDoWypisania tablica[], int rozmiar) {
    int najdluzszaNazwa = tablica[0].dlugoscNazwyGrupy;
    int i;

    for(i = 1; i < rozmiar; i++) {
        if(najdluzszaNazwa < tablica[i].dlugoscNazwyGrupy) { /* jesli najdlusza jak dotychczas nazwa jest krotsza niz aktualna, podmien */
            najdluzszaNazwa = tablica[i].dlugoscNazwyGrupy;
        }
    }

    return najdluzszaNazwa;
}

int dlugoscPolaRozmiaru(informacjeDoWypisania tablica[], int rozmiar) {
    int najdluzszyRozmiar = tablica[0].rozmiar;
    int i;

    for(i = 1; i < rozmiar; i++) { /* ustalenie najwiekszego rozmiaru pliku */
        if(najdluzszyRozmiar < tablica[i].rozmiar) {
            najdluzszyRozmiar = tablica[i].rozmiar;
        }
    }

    int iloscPol = 0;
    while(najdluzszyRozmiar != 0) { /* ustalenie ilosci cyfr rozmiaru najwiekszego pliku */
        iloscPol++;
        najdluzszyRozmiar /= 10;
    }

    return iloscPol;
}

void wypiszTablice(informacjeDoWypisania tablica[], int rozmiar, int dlLinku, int dlUzytkownika, int dlGrupy, int dlRozmiaru) {
    int i;

    for(i = 0; i < rozmiar; i++) {
        if(tablica[i].rok == 0) { /* wyswietlanie w przypadku, gdy rok jest aktualny */
            printf("%s %*d %*s  %*s %*d %02d %s %02d:%02d %s",
                   tablica[i].prawaDostepu,
                   dlLinku,
                   tablica[i].linkiTwarde,
                   dlUzytkownika,
                   tablica[i].wlasciciel,
                   dlGrupy,
                   tablica[i].grupa,
                   dlRozmiaru,
                   tablica[i].rozmiar,
                   tablica[i].dzien,
                   tablica[i].miesiac,
                   tablica[i].godzina,
                   tablica[i].minuta,
                   tablica[i].nazwa);
        } else { /* w przypadku plikow starszych niz z roku 2018 */
            printf("%s %*d %*s  %*s %*d %02d %s  %4d %s",
                   tablica[i].prawaDostepu,
                   dlLinku,
                   tablica[i].linkiTwarde,
                   dlUzytkownika,
                   tablica[i].wlasciciel,
                   dlGrupy,
                   tablica[i].grupa,
                   dlRozmiaru,
                   tablica[i].rozmiar,
                   tablica[i].dzien,
                   tablica[i].miesiac,
                   tablica[i].rok,
                   tablica[i].nazwa);
        }
        if(tablica[i].prawaDostepu[0] == 'l') { /* wypisanie zrodla do linku symbolicznego */
            printf(" -> %s", tablica[i].nazwaZrodla);
        }
        puts("");
    }
}

void wypiszInformacje(char *nazwa, informacjeDoWypisania info, struct stat *plik) {

    /* WYPISANIE WSZYSTKICH INFORMACJI POTRZEBNYCH W DRUGIEJ WERSJI PROGRAMU */
    printf("Informacje o %s:\n", nazwa);
    printf("Typ pliku:   %s\n", typPliku(info.prawaDostepu[0]));
    if(info.prawaDostepu[0] == 'l') { /* wypisanie rowniez sciezki na co wskazuje */

        printf("Sciezka:     ");
        sciezkaNaLink(nazwa);
        puts("");
        printf("Wskazuje na: ");
        sciezkaDoPliku(nazwa);
        puts("");

    } else {
        printf("Sciezka:     ");
        sciezkaDoPliku(nazwa);
        puts("");
    }

    printf("Rozmiar:     %d %s\n", info.rozmiar, bajty(info.rozmiar));
    printf("Uprawnienia: %s\n", uprawnienia(info.prawaDostepu));
    wypiszCzas(plik);

    if(info.prawaDostepu[0] == '-' && info.prawaDostepu[3] == '-'
            && info.prawaDostepu[6] == '-' && info.prawaDostepu[9] == '-') { /* wypisanie zawartosci pliku */
        printf("Poczatek zawartosci:\n");
        drukujZawartosc(nazwa);
    }
}

char* typPliku(char typ) { /* zwraca napis z nazwa typu pliku */
    if(typ == 'l') {
        return "link symboliczny";
    } else if(typ == 'd') {
        return "katalog";
    } else if(typ == 'c') {
        return "urzadzenie znakowe";
    } else if(typ == 'b') {
        return "urzadzenie blokowe";
    } else if(typ == 'p') {
        return "potok";
    } else if(typ == 's') {
        return "gniazdo";
    } else {
        return "zwykly plik";
    }
}

void sciezkaDoPliku(char* nazwa) { /* wypisuje sciezke do pliku lub (w przypadku linku symbolicznego) podaje na co wskazuje link */

    static char sciezka[PATH_MAX + 1]; /* zmienna na sciezke */
    char* wynik = realpath(nazwa, sciezka); /* przypisanie do zmiennej 'sciezka' sciezki do pliku 'nazwa'; jesli wskaznik wynik zostal ustawiony na NULL,
                                               to nie udalo sie pobrac sciezki */
    if(wynik == NULL) {
        perror("Blad funkcj realpath");
        exit(errno);
    }
    printf("%s", sciezka);
}

void sciezkaNaLink(char *nazwa) { /* wypisuje sciezke do linku symbolicznego */

    char sciezka[PATH_MAX + 1]; /* zmienna na sciezke */

    if(getcwd(sciezka, PATH_MAX+1) == NULL) { /* funckja pobiera sciezke do katalogu, w ktorym aktualnie wykonywany jest program do zmiennej 'sciezka' */
        perror("Blad funkcji getcwd");
        exit(errno);
    }
    printf("%s/%s", sciezka, nazwa); /* sciezka do linku w uproszczonej wersji bedzie sciezka aktualnego katalogu + nazwa linku */
}

char* bajty(int rozmiar) { /* ustalenie "koncowki" w napisie przy padawaniu rozmiaru pliku - przykladowo 4 bajTY, 12 bajtOW, 1 bajT */
    if(rozmiar == 1) {
        return "bajt";
    } else if(rozmiar%10 == 2 || rozmiar%10 == 3 || rozmiar%10 == 4) { /* gdy rozmiar konczy sie cyfra 2, 3 lub 4 */
        if(rozmiar%100 != 12 && rozmiar%100 != 13 && rozmiar%100 != 14) { /* gdy ostatnie dwie cyfry rozmiaru to 12, 13 lub 14 wypisuje 'bajTY',
                                                                             we wszystkich innych przypadkach 'bajtOW' */
            return "bajty";
        } else {
            return "bajtow";
        }
    } else {
        return "bajtow";
    }
}

char* uprawnienia(char prawaDostepu[11]) { /* funkcja zwraca napis zawierajacy tylko prawa dostepu, z pominieciem typu pliku */
    static char prawaDostepuKopia[10];
    int i;
    for(i = 0; i < 10; i++) {
        prawaDostepuKopia[i] = prawaDostepu[i+1];
    }
    return prawaDostepuKopia;
}

void wypiszCzas(struct stat *plik) { /* wypisuje wszystkie czasy potrzebne w wersji programu z parametrem */
    printf("Ostatnio uzywany        ");
    podajCzas(plik->st_atime);
    printf("Ostatnio modyfikowany   ");
    podajCzas(plik->st_mtime);
    printf("Ostatnio zmieniany stan ");
    podajCzas(plik->st_ctime);
}

void podajCzas(time_t t) { /* wypisuje konkretna date w zaleznosci od wartosci przekazanej jako parametr zmiennej t */

    struct tm *czasPoModyfikacji;
    czasPoModyfikacji = localtime(&t);
    char miesiace[12][13] = {  "stycznia","lutego","marca","kwietnia","maja","czerwca","lipca",
                               "sierpnia","wrzesnia","pazdziernika","listopada","grudnia"
                            };
    printf("%2d %s %04d roku o %02d:%02d:%02d\n",
           czasPoModyfikacji->tm_mday,
           miesiace[czasPoModyfikacji->tm_mon],
           czasPoModyfikacji->tm_year + 1900,
           czasPoModyfikacji->tm_hour,
           czasPoModyfikacji->tm_min,
           czasPoModyfikacji->tm_sec);
}

void drukujZawartosc(char* nazwaPliku) { /* drukuje poczatkowe 80 znakow zawartosci pliku przekazanego jako argument */
    char doDruku[80];
    int deskryptor;
    int koniecTekstu; /* przechowuje pozycje w tablicy doDruku[] na ktorym konczy sie napis tresc do wypisania */

    if((deskryptor = open(nazwaPliku, O_RDONLY)) < 0) { /* otwarcie pliku do odczytu */
        perror("Blad otwarcia pliku do odczytu");
        exit(errno);
    }

    if((koniecTekstu = read(deskryptor, doDruku, 80)) < 0) { /* wpisanie do tablicy pierszych 80 znakow pliku */
        perror("Blad funkcji read");
        exit(errno);
    }
    doDruku[koniecTekstu] = '\0'; /* trzeba dodac znak konca napisu */

    printf("%s\n", doDruku);

    close(deskryptor); /* zamkniecie pliku do odczytu */
}
