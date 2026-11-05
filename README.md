# BGT 2 užduotis: blokų grandinės simuliacija · v0.3

## 1. Projektas

Mokomoji blokų grandinės simuliacija, parašyta C++20. Programa generuoja vartotojus
ir transakcijas, kasa blokus, juos tikrina ir atnaujina balansus.
ID, blokams ir jų transakcijų šaknims naudojama pirmos užduoties pasirinktinė maiša.
Tai supaprastintas sąskaitų modelis, ne Bitcoin realizacija.

## 2. Paleidimas

Reikia C++20 kompiliatoriaus, CMake 3.20+ ir Make arba Ninja.
Komandos vykdomos projekto aplanke:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake
```

Visas rinkinys ir mažas pavyzdys:

```sh
./build/cmake/blockchain
./build/cmake/blockchain --users 5 --transactions 12 --seed 42 --difficulty 3
```

Testai ir parametrų pagalba:

```sh
ctest --test-dir build/cmake --output-on-failure
./build/cmake/blockchain --help
```

Python 3 reikalingas tik papildomiems CLI testams ir palyginimo įrankiui;
be jo programa ir abu C++ testų rinkiniai veikia. Papildomų Python paketų nereikia.

## 3. Architektūra

| Failai | Paskirtis |
|---|---|
| [`src/models.hpp`](src/models.hpp), [`src/models.cpp`](src/models.cpp) | Vartotojai, transakcijos, blokų duomenys, serializacija ir Merkle medis. |
| [`src/custom_hash.hpp`](src/custom_hash.hpp), [`src/custom_hash.cpp`](src/custom_hash.cpp) | Nepakeista pirmos užduoties maiša. |
| [`src/hashing.hpp`](src/hashing.hpp) | Teksto baitai, maišos formato tikrinimas ir hex iškodavimas. |
| [`src/chain.hpp`](src/chain.hpp), [`src/chain.cpp`](src/chain.cpp) | Fondas, kandidatai, kasimas, validacija ir patvirtinta būsena. |
| [`src/main.cpp`](src/main.cpp) | CLI, vykdymo ciklas ir statistika. |
| [`tests/test_blockchain.cpp`](tests/test_blockchain.cpp) | 38 bendros v0.2 ir v0.3 testų grupės. |
| [`tests/test_v03.cpp`](tests/test_v03.cpp), [`tests/test_cli.py`](tests/test_cli.py) | Naujos regresijos ir CLI patikra. |
| [`tools/compare_versions.py`](tools/compare_versions.py) | Testų ir simuliacijų palyginimas su v0.2. |
| [`CMakeLists.txt`](CMakeLists.txt), [`build.sh`](build.sh) | CMake ir alternatyvus Bash kompiliavimas. |

Vartotojai → transakcijos → fondas → blokų kandidatai → PoW → validacija → grandinė ir balansai.

## 4. Veikimo principas

Vartotojas turi vardą, unikalų imituojamą viešąjį raktą ir sveikąjį balansą.
Transakcija nurodo siuntėją, gavėją, teigiamą sumą, unikalų sekos numerį ir ID.
Generavimas balansų nekeičia; patekimas į fondą lėšų nerezervuoja.

| Duomenys | Tiksli maišos įvesties forma |
|---|---|
| Transakcija | `sender\|receiver\|amount\|sequence` |
| Bloko antraštė | `prev_hash\|timestamp\|version\|tx_root\|nonce\|difficulty` |

Maišomi UTF-8 baitai be papildomų tarpų ir eilutės pabaigos. Skaičiai dešimtainiai,
be papildomų pradinių nulių; identifikatoriuose negali būti `|`. ID į savo įvestį
neįtraukiamas. Maišos susieja transakcijų turinį su ID, o blokus – su pirmtakais.
Blokas saugo antraštę, transakcijų sąrašą ir apskaičiuotą antraštės maišą.
`H` grąžina 256 bitų maišą – 64 hex simbolius (`0–9`, `a–f`), išsaugant pradinius nulius.

Tikrinami žinomi ir skirtingi siuntėjas bei gavėjas, teigiama suma, teisingas
64 simbolių ID, dublikatai, sekos numeriai ir ankstesni patvirtinimai.
Kiekvienas kandidatas transakcijas vykdo savo laikinoje balansų kopijoje:
ankstesnė transakcija jau sumažina siuntėjo likutį. Tikrinamas lėšų pakankamumas
ir 64 bitų sveikųjų skaičių perpildymas.

PoW keičia tik `nonce`, kol antraštės maiša prasideda `000` (difficulty 3).
Prieš priimant bloką dar kartą tikrinami pirmtakas, versija, laikas, šaknis,
antraštės maiša, grandinės reikalaujamas sudėtingumas ir 1–100 transakcijų vykdymas.
Tik visas priimtas blokas pakeičia fondą, grandinę ir tikruosius balansus.
Atmestas blokas patvirtintos būsenos nekeičia.

Genesis tuščias: pirmtakas – 64 nuliai, laikas `1700000000`, versija `0.1`,
`nonce=0`, difficulty 0, šaknis `H(tušti baitai)`. Jis nekasamas.
Kito bloko laikas didėja viena sekunde. Pradiniai balansai saugomi atskirai;
priėmus transakciją ta pati suma nurašoma ir pridedama. Atlygių ir mokesčių nėra,
todėl bendra lėšų suma nekinta.

## 5. v0.1, v0.2 ir v0.3

| Versija | Transakcijų šaknis | Kandidatai ir pakeitimai |
|---|---|---|
| v0.1 | `H(ID1 + ID2 + ...)`, jungiami hex tekstai | Vienas kandidatas, PoW, bazinė validacija ir balansai. |
| v0.2 | Merkle medis iš tikrų maišos baitų | Penki kandidatai, išplėstinė validacija, biudžetai ir nonce tęsimas. |
| v0.3 | Tas pats Merkle medis ir bloko formatas `0.2` | Kasimo API patikra, fondo kopijos pašalinimas ir nauji testai. |

Merkle lapai – transakcijų ID jų bloko tvarka. Hex iškoduojamas į 32 baitus;
tėvas yra `H(kairės baitai || dešinės baitai)`. Nelyginiame lygyje, kuriame daugiau
nei vienas mazgas, paskutinis dubliuojamas. Vieno lapo šaknis yra jo ID;
tuščio medžio – `H(tušti baitai)`.

v0.2 ir v0.3 penkis kartus maišo fondo tvarką ir renka iki 100 tinkamų transakcijų.
Kandidatai turi tą patį pirmtaką ir patvirtintą pradinę būseną, bet atskiras kopijas.
Jų transakcijos gali sutapti. Kasama nuosekliai: pirmas biudžetas – 1000 bandymų
kandidatui; nesėkmės atveju kito raundo biudžetas dvigubinamas. Tęsiama nuo
nebandyto nonce, iki 1000000 bandymų kandidatui viename aukštyje.

Užbaigus raundą laimi mažiausio numerio tinkamą sprendimą radęs kandidatas.
Tik jo transakcijos patvirtinamos ir pašalinamos iš fondo; pralaimėtojų lieka.
Struktūriškai netinkami įrašai atmetami iš karto, nefinansuojami atidedami kitam
aukščiui. Kai visame fonde nėra įvykdomų transakcijų, likusios atmetamos su priežastimi.
Pasiekus kasimo ribą, fondas išsaugomas ir programa baigia darbą su kodu 1.

## 6. Testavimas

| Patikra | v0.1 | v0.2 | v0.3 |
|---|---|---|---|
| Ankstesnis C++ rinkinys | 20/20 | 38/38 | 38/38, failas nepakeistas |
| Maišos pastovumas, ID, transakcijos ir lėšos | Praėjo | Praėjo | Praėjo |
| PoW, dublikatai ir atmesti blokai | Praėjo | Praėjo | Praėjo |
| Merkle ir penkių kandidatų varžybos | – | Praėjo | Praėjo |
| Naujos v0.3 grupės | – | 5/9 | 9/9 |
| Tie patys papildomi CLI atvejai | – | 12/12 | 12/12 |
| ASan + UBSan šioje patikroje | – | 38/38 | 47/47 |

Keturios naujos grupės parodo v0.2 kasimo pagalbinių funkcijų klaidas su pakeistais
kandidatais; v0.2 bloko priėmimas ir anksčiau saugojo patvirtintą būseną.
v0.3 iš viso praeina 47 C++ grupes ir 12 CLI atvejų. Baigus simuliaciją visa
grandinė peržaidžiama iš pradinių balansų, tikrinami blokai, dublikatai ir lėšų suma.

## 7. Rezultatai

Abiejų ankstesnių versijų rezultatai pakartoti iš nepakeistų žymų.
v0.2 ir v0.3 naudoti vienodi duomenys, parametrai ir kompiliavimo nustatymai.

| Rodiklis | v0.1 | v0.2 | v0.3 |
|---|---:|---:|---:|
| Vartotojai / transakcijos | 1000 / 10000 | 1000 / 10000 | 1000 / 10000 |
| Priimti blokai, be genesis | 100 | 100 | 100 |
| Priimtos transakcijos | 9947 | 9971 | 9971 |
| Atmestos / likusios | 53 / 0 | 29 / 0 | 29 / 0 |
| Lėšos prieš / po | 496232748 / 496232748 | 496232748 / 496232748 | 496232748 / 496232748 |
| Kasimo bandymai / raundai | – | 711048 / 132 | 711048 / 132 |

v0.1 nefinansuojamas transakcijas atmeta tikrinant kandidatą; v0.2 ir v0.3 jas
atideda. v0.2 ir v0.3 sutampa visi blokai, maišos, laimėtojai ir bandymų skaičiai.
101-oji fondo patikra be kasimo atmetė 29 likusias transakcijas. Spartos pagerėjimo neteigiama.

Tikra v0.3 mažo pavyzdžio ištrauka:

```text
Block 1 | transactions=12 | difficulty=3 | nonce=146 | attempts=147 | mining=0.000103s
  block_hash=00073cbbdb8221a50693794bfe70808ac04011a589094b29924b16b0a7c0987b
```

Kandidatų 1–5 bandymai: `1000, 147, 1000, 188, 1000`; sprendimą rado 2 ir 4,
laimėjo 2. Laikas – šio vieno paleidimo matavimas, ne spartos palyginimas.

## 8. Atkartojamumas

| Parametras | Numatytoji reikšmė |
|---|---|
| `--users`, `--transactions` | `1000`, `10000` |
| `--seed`, generatorius | `20261029`, `std::mt19937_64` |
| Pradiniai balansai / transakcijų sumos | `100–1000000` / `1–10000`, sveikieji skaičiai |
| `--difficulty` | `3`, leidžiama `0–64` |
| `--attempt-budget`, `--max-attempts` | `1000`, `1000000`; abu teigiami |

`--users` turi būti bent 2, `--transactions` ir `--seed` – neneigiami.
Patikrinta Linux x86_64, GCC 15.2.0, CMake 4.1.6; v0.2 ir v0.3 – `Release`,
`-O3 -DNDEBUG`. Ta pati sėkla ir C++ standartinė biblioteka atkuria duomenis ir
kandidatų tvarką; kitų bibliotekų `shuffle` gali skirtis. Laikai nėra atkuriami
bitas į bitą. Pirminė būsena ir blokai leidžia perskaičiuoti visus balansus.

## 9. DI naudojimas ir v0.3 patobulinimai

Codex padėjo įgyvendinti, peržiūrėti ir tikrinti visas tris versijas.
Šio pokalbio modelio šeima – GPT-6; tikslus modelio leidimas neužfiksuotas.

| Sritis | v0.3 pakeitimas | Patvirtinimas |
|---|---|---|
| Validacija | Tikrinamas išsaugotas PoW, kandidatų bendri laukai ir jau išnaudotas biudžetas. | 4 v0.2 regresijų klaidos v0.3 nepasikartoja. |
| Kodo struktūra | Prieš šalinimą patikrinama visų transakcijų narystė; nebekopijuojamas visas fondas. | Vėlyvas atmetimas ir fondo tvarkos testai praeina. |
| Testai | Atskirtos 9 regresijų grupės ir išsaugota CLI patikra. | Bendros 38 grupės nepakeistos, palyginimas pakartojamas įrankiu. |

Kompromisas: papildomi patikrinimai ir dvi fondo įrašo paieškos vietoj vienos
kopijoje. Spartos ar RAM matavimų nedaryta. Užklausų datos, priimti pasiūlymai,
nepriklausoma patikra ir palyginimo komandos – [v0.3 analizėje](docs/v0.3-analysis.md).

## 10. Išvados, apribojimai ir autorius

Veikia vietinis transakcijų apdorojimas, PoW, Merkle medis ir penkių kandidatų
varžybos. Viskas vyksta viename procese, todėl tai nėra decentralizuotas P2P
sutarimas. Nėra tinklo, parašų, realios kriptovaliutos, Bitcoin konsensuso ar
sudėtingumo perskaičiavimo; ID neįrodo siuntėjo tapatybės. Būsena laikoma atmintyje.

Autorius – **Rokas Smetonis (`Mizerd`)**, individualus darbas su nurodyta DI pagalba.
Išorinis kodas: pirmos užduoties [DI maiša V0.2](https://github.com/Mizerd/BGT_1/tree/c73dc9b010d33aca0606b0d8f5235067efeac945),
commit `c73dc9b`, failai `custom_hash.cpp` ir `custom_hash.hpp` iš `Rokas - AI/`.
Maišos algoritmas nekeistas ir kriptografiškai neanalizuotas.
