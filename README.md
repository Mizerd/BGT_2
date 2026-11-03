# Blokų grandinės simuliacija

Mokomoji C++20 programa. Reikia CMake 3.20+ ir Make arba Ninja.
Komandas vykdykite projekto aplanke.

Kompiliavimas:

```sh
cmake -S . -B build/cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake
```

Trumpas bandymas:

```sh
./build/cmake/blockchain --users 5 --transactions 12 --seed 42 --difficulty 3
```

Visas rinkinys: 1 000 naudotojų, 10 000 pervedimų, sėkla `20261029`, sudėtingumas `3`:

```sh
./build/cmake/blockchain
```

Testai ir pagalba:

```sh
ctest --test-dir build/cmake --output-on-failure
./build/cmake/blockchain --help
```

Parametrai: `--users` (bent 2), `--transactions`, `--seed`, `--difficulty` (0–64),
`--attempt-budget` ir `--max-attempts` (abu teigiami).
Ankstesnis būdas su Bash ir C++ kompiliatoriumi taip pat veikia:

```sh
bash build.sh
./build/blockchain --users 5 --transactions 12 --seed 42
./build/tests
```

## v0.2

Pridėtas Merkle medis, fondo įrašų tikrinimas ir penkių kandidatų varžybos.
Naudojama ta pati nepakeista 256 bitų [BGT_1 DI maiša V0.2](https://github.com/Mizerd/BGT_1/tree/c73dc9b010d33aca0606b0d8f5235067efeac945)
(`c73dc9b`): 64 šešioliktainiai simboliai (`0–9`, `a–f`), išsaugant pradinius nulius.

**Merkle medis.** Lapai yra pervedimų ID jų bloko tvarka. ID iškoduojami į 32 baitus;
tėvas skaičiuojamas `H(kairės baitai || dešinės baitai)`. Kiekviename nelyginiame
lygyje paskutinis mazgas dubliuojamas. Vieno lapo šaknis yra jo ID, tuščio medžio —
`H(tušti baitai)`. Šešioliktainio teksto sujungimas tėvams nenaudojamas.

**Tikrinimas.** Į fondą patenka tik žinomų skirtingų naudotojų teigiami pervedimai
su teisingu 64 simbolių ID, nesikartojančiu ID ir sekos numeriu. Patvirtinti
pervedimai pakartotinai nepriimami. Patekimas į fondą lėšų nerezervuoja.
Kandidato ir bloko pervedimai vykdomi eilės tvarka laikinoje balansų kopijoje:
tikrinamas pakankamas likutis ir sveikųjų skaičių perpildymas.

Blokui tikrinami ankstesnio bloko maiša, versija `0.2`, didėjantis laikas, Merkle
šaknis, perskaičiuota antraštės maiša, grandinės reikalaujamas sudėtingumas ir PoW,
1–100 nesidubliuojančių pervedimų bei jų nuoseklus vykdymas. Tik visas tinkamas
blokas pakeičia grandinę, balansus ir fondą; atmestas blokas jų nekeičia.

**Kandidatai.** Kiekvienam aukščiui fondo tvarka maišoma penkis kartus. Kiekvienas
kandidatas turi tą patį pirmtaką ir laiką, bet atskirą patvirtintų balansų kopiją.
Tikrinama, kol surenkama 100 tinkamų pervedimų arba baigiasi fondas. Kandidatų
pervedimai gali sutapti. Kasama nuosekliai, be gijų.

`--attempt-budget` numatyta reikšmė yra 1 000 bandymų kandidatui pirmame raunde.
Neradus sprendimo, kito raundo biudžetas dvigubinamas, tęsiant nuo nepanaudoto
`nonce`. Kiti antraštės laukai nekinta. `--max-attempts` riboja bendrą kiekvieno
kandidato bandymų skaičių viename aukštyje (numatyta 1 000 000). Pasiekus ribą,
programa pateikia likusių pervedimų priežastį ir baigia darbą su kodu 1.
Sudėtingumas nemažinamas: numatyta maišos pradžia `000`.

Užbaigus visą raundą laimi mažiausio numerio sprendimą radęs kandidatas.
Jis dar kartą tikrinamas pagal patvirtintą būseną. Pritaikomi ir iš fondo pašalinami
tik jo pervedimai. Pralaimėjusių kandidatų pervedimai lieka fonde.
Struktūriškai netinkami įrašai atmetami iš karto su priežastimi. Nepakankamai
finansuojami pervedimai atidedami kitam aukščiui. Kai visame fonde nelieka nė vieno
įvykdomo pervedimo, likusieji atmetami dėl lėšų trūkumo ir programa baigia darbą.

**v0.1 išsaugojimas.** Žyma `v0.1` nepakeista: joje vienas kandidatas ir šaknis
`H(ID1 + ID2 + ...)`. Genesis taip pat nepakeistas: tušti pervedimai, 64 nulių
pirmtakas, laikas `1700000000`, versija `0.1`, `nonce=0`, sudėtingumas 0 ir tuščio
turinio maiša kaip šaknis. Genesis nekasamas; kitų blokų laikas didėja po sekundę.

Pervedimo forma lieka `sender|receiver|amount|sequence`, antraštės —
`prev_hash|timestamp|version|tx_root|nonce|difficulty`. Maišomi UTF-8 baitai be
papildomų tarpų ar eilutės pabaigos; skaičiai dešimtainiai. Pradiniai balansai
saugomi atskirai, valiutos sumos — 64 bitų sveikieji skaičiai. Ta pati sėkla ir
C++ standartinė biblioteka atkuria duomenis, kandidatų tvarką ir maišas; laikai skiriasi.

## Tikri rezultatai

Mažas bandymas, GCC 15.2.0, Linux, CMake Release:

| Kandidatas | Pervedimai | Bandymai | Kasimas, s | Rado nonce |
|---|---:|---:|---:|---|
| 1 | 12 | 1000 | 0.001438 | ne |
| 2 | 12 | 147 | 0.000210 | taip |
| 3 | 12 | 1000 | 0.001470 | ne |
| 4 | 12 | 188 | 0.000269 | taip |
| 5 | 12 | 1000 | 0.001435 | ne |

Laimėjo 2 kandidatas: aukštis 1, `nonce=146`, maiša
`00073cbbdb8221a50693794bfe70808ac04011a589094b29924b16b0a7c0987b`.
Priimti visi 12 pervedimų; pradinis ir galutinis lėšų kiekis — 3 186 056.

Viso numatytojo rinkinio rezultatai:

| Rodiklis | Reikšmė |
|---|---:|
| Naudotojai / sugeneruoti pervedimai | 1000 / 10000 |
| Priimti blokai, be genesis | 100 |
| Priimti / atmesti / likę pervedimai | 9971 / 29 / 0 |
| Pradinis / galutinis lėšų kiekis | 496232748 / 496232748 |
| Varžybos / kasimo raundai | 101 / 132 |
| Bendras bandymų skaičius | 711048 |
| Bendras kasimo laikas | 0.406909 s |
| Visas vykdymo laikas | 1.446296 s |

Kandidatų 1–5 pergalės: `36, 22, 18, 11, 13`. Paskutinė patikra be kasimo atmetė
29 likusius nefinansuojamus pervedimus. Visi blokai patikrinti atkuriant grandinę;
balansai neneigiami, lėšos išsaugotos, pervedimai nesikartoja. Laikai — vieno paleidimo.

**Testai:** 38/38 grupės, įskaitant visas 20 ankstesnių grupių su v0.2 blokų
duomenimis. Tikrinami 1/2/3/5 lapų medžiai, netaisyklingi ID, perpildymas, dvigubas
išleidimas, pakeisti blokai, nepriklausomi kandidatai, biudžetai, tęsiami nonce,
pralaimėtojų būsena, atidėti pervedimai ir baigtinumas. Praėjo ir AddressSanitizer
bei UndefinedBehaviorSanitizer patikra; papildomai patikrinta 12 CLI atvejų.

**Ribos:** visa būsena laikoma atmintyje. Nėra parašų, atlygių, mokesčių ar UTXO;
ID nepatvirtina siuntėjo tapatybės. Penki kandidatai nėra decentralizuota P2P
grandinė: jie veikia viename procese, be nepriklausomų mazgų, tinklo ar sutarimo.
