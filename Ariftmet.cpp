#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>

using namespace std;

//Параметры
const uint32_t MAXINT = 0xFFFF;
const uint32_t POLOVINA = (MAXINT + 1) / 2;
const uint32_t CHETVERT = POLOVINA / 2;
const uint32_t TRICHETVERTI = CHETVERT * 3;

//Побитовая запись
class ZapisBitov {
    ofstream& file_out;
    uint8_t bufer = 0;
    int zapolnenno = 0;
public:
    ZapisBitov(ofstream& f) : file_out(f) {}

    void dobavitBit(int b) {
        bufer = (bufer << 1) | (b & 1);
        zapolnenno++;
        if (zapolnenno == 8) {
            file_out.put((char)bufer);
            bufer = 0;
            zapolnenno = 0;
        }
    }

    void dobavitMnogo(int b, uint32_t skolko) {
        for (uint32_t i = 0; i < skolko; i++) dobavitBit(b);
    }

    void zakonchit() {
        if (zapolnenno) {
            bufer <<= (8 - zapolnenno);
            file_out.put((char)bufer);
            zapolnenno = 0;
            bufer = 0;
        }
    }
};

// Побитовое чтение
class ChtenieBitov {
    ifstream& file_in;
    uint8_t bufer = 0;
    int ostalos = 0;
public:
    ChtenieBitov(ifstream& f) : file_in(f) {}

    int prochitatBit() {
        if (ostalos == 0) {
            int c = file_in.get();
            if (c == EOF) return -1;
            bufer = (uint8_t)c;
            ostalos = 8;
        }
        int bit = (bufer >> (ostalos - 1)) & 1;
        ostalos--;
        return bit;
    }

    int prochitatBity(int n) {
        int v = 0;
        for (int i = 0; i < n; i++) {
            int b = prochitatBit();
            if (b == -1) return -1;
            v = (v << 1) | b;
        }
        return v;
    }
};

// Кодирование
bool zakodirovat(double& koeff_szhatiya) {
    ifstream vhod("text.txt", ios::binary);
    if (!vhod.is_open()) {
        cout << "text.txt ne naiden\n";
        return false;
    }

    array<uint32_t, 256> chastota{};
    uint64_t vsego_simvolov = 0;
    int simvol;

    while ((simvol = vhod.get()) != EOF) {
        chastota[(unsigned char)simvol]++;
        vsego_simvolov++;
    }

    ofstream vyhod("encoded.txt", ios::binary);
    if (!vyhod.is_open()) {
        cout << "Ne sozdalsya encoded.txt\n";
        return false;
    }

    // Заголовок
    vyhod.write((char*)&vsego_simvolov, sizeof(vsego_simvolov));
    vyhod.write((char*)chastota.data(), sizeof(uint32_t) * 256);

    if (vsego_simvolov == 0) {
        koeff_szhatiya = 1.0;
        return true;
    }

    // Суммы
    vector<uint32_t> summa(257, 0);
    for (int i = 0; i < 256; i++) summa[i + 1] = summa[i] + chastota[i];
    uint32_t obshaya_summa = summa[256];

    vhod.clear();
    vhod.seekg(0, ios::beg);

    ZapisBitov zapis(vyhod);

    uint32_t niz = 0, verh = MAXINT;
    uint32_t ozhidanie = 0;

    while ((simvol = vhod.get()) != EOF) {
        unsigned char sim = (unsigned char)simvol;
        uint32_t interval = verh - niz + 1;

        uint32_t noviy_niz = niz + (uint64_t)interval * summa[sim] / obshaya_summa;
        uint32_t noviy_verh = niz + (uint64_t)interval * summa[sim + 1] / obshaya_summa - 1;

        niz = noviy_niz;
        verh = noviy_verh;

        for (;;) {
            if (verh < POLOVINA) {
                zapis.dobavitBit(0);
                zapis.dobavitMnogo(1, ozhidanie);
                ozhidanie = 0;
            }
            else if (niz >= POLOVINA) {
                zapis.dobavitBit(1);
                zapis.dobavitMnogo(0, ozhidanie);
                ozhidanie = 0;
                niz -= POLOVINA;
                verh -= POLOVINA;
            }
            else if (niz >= CHETVERT && verh < TRICHETVERTI) {
                ozhidanie++;
                niz -= CHETVERT;
                verh -= CHETVERT;
            }
            else break;

            niz = (niz << 1) & MAXINT;
            verh = ((verh << 1) | 1) & MAXINT;
        }
    }

    ozhidanie++;
    if (niz < CHETVERT) {
        zapis.dobavitBit(0);
        zapis.dobavitMnogo(1, ozhidanie);
    }
    else {
        zapis.dobavitBit(1);
        zapis.dobavitMnogo(0, ozhidanie);
    }

    zapis.zakonchit();

    vhod.seekg(0, ios::end);
    vyhod.seekp(0, ios::end);
    double razmer_vhoda = vhod.tellg();
    double razmer_vyhoda = vyhod.tellp();
    koeff_szhatiya = razmer_vyhoda / razmer_vhoda;

    return true;
}

// Декодирование
bool raskodirovat() {
    ifstream vhod("encoded.txt", ios::binary);
    if (!vhod.is_open()) {
        cout << "encoded.txt ne naiden\n";
        return false;
    }

    uint64_t vsego_simvolov = 0;
    array<uint32_t, 256> chastota{};
    vhod.read((char*)&vsego_simvolov, sizeof(vsego_simvolov));
    vhod.read((char*)chastota.data(), sizeof(uint32_t) * 256);

    vector<uint32_t> summa(257, 0);
    for (int i = 0; i < 256; i++) summa[i + 1] = summa[i] + chastota[i];
    uint32_t obshaya_summa = summa[256];

    ofstream vyhod("decoded.txt", ios::binary);
    if (!vyhod.is_open()) {
        cout << "Ne sozdalsya decoded.txt\n";
        return false;
    }

    ChtenieBitov chtenie(vhod);
    int pervie16 = chtenie.prochitatBity(16);
    if (pervie16 < 0) pervie16 = 0;

    uint32_t znachenie = pervie16;
    uint32_t niz = 0, verh = MAXINT;

    for (uint64_t i = 0; i < vsego_simvolov; i++) {
        uint32_t interval = verh - niz + 1;
        uint32_t masshtab = (uint64_t)(znachenie - niz + 1) * obshaya_summa - 1;
        masshtab /= interval;

        int L = 0, R = 256;
        while (R - L > 1) {
            int M = (L + R) / 2;
            if (summa[M] <= masshtab) L = M;
            else R = M;
        }

        int simvol = L;
        vyhod.put((char)simvol);

        uint32_t noviy_niz = niz + (uint64_t)interval * summa[simvol] / obshaya_summa;
        uint32_t noviy_verh = niz + (uint64_t)interval * summa[simvol + 1] / obshaya_summa - 1;

        niz = noviy_niz;
        verh = noviy_verh;

        for (;;) {
            if (verh < POLOVINA) {
                // ничего
            }
            else if (niz >= POLOVINA) {
                znachenie -= POLOVINA;
                niz -= POLOVINA;
                verh -= POLOVINA;
            }
            else if (niz >= CHETVERT && verh < TRICHETVERTI) {
                znachenie -= CHETVERT;
                niz -= CHETVERT;
                verh -= CHETVERT;
            }
            else break;

            niz = (niz << 1) & MAXINT;
            verh = ((verh << 1) | 1) & MAXINT;

            int bit = chtenie.prochitatBit();
            if (bit < 0) bit = 0;
            znachenie = ((znachenie << 1) | bit) & MAXINT;
        }
    }

    return true;
}

int main() {
    cout << "1 - Zakodirovat text.txt -> encoded.txt\n";
    cout << "2 - Raskodirovat encoded.txt -> decoded.txt\n";
    cout << "Vibor: ";

    int rezhim;
    cin >> rezhim;

    auto nachalo = chrono::high_resolution_clock::now();

    if (rezhim == 1) {
        double koeff = 0.0;
        if (!zakodirovat(koeff)) return 0;

        auto konec = chrono::high_resolution_clock::now();
        double vremya = chrono::duration<double>(konec - nachalo).count();

        cout << "Kodirovanie zaversheno.\n";
        cout << "Koefficient szhatiya = " << koeff << endl;
        cout << "Vremya = " << vremya << " sec\n";
    }
    else if (rezhim == 2) {
        if (!raskodirovat()) return 0;

        auto konec = chrono::high_resolution_clock::now();
        double vremya = chrono::duration<double>(konec - nachalo).count();

        cout << "Dekodirovanie zaversheno.\n";
        cout << "Vremya = " << vremya << " sec\n";
    }
    else {
        cout << "Neverniy vibor\n";
    }

    return 0;
}