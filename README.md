# Blokų grandinės simuliacija

Reikia C++20 kompiliatoriaus ir Bash. Komandas vykdykite projekto aplanke.

Kompiliavimas ir visas paleidimas:

```sh
bash build.sh
./build/blockchain
```

Numatytieji nustatymai: 1 000 naudotojų, 10 000 pervedimų, sėkla `20261029`, sudėtingumas `3`.

Trumpas bandymas:

```sh
./build/blockchain --users 5 --transactions 12 --seed 42 --difficulty 3
```

Parametrai: `--users` (bent 2), `--transactions`, `--seed`, `--difficulty` (0–64).

Testai ir pagalba:

```sh
./build/tests
./build/blockchain --help
```

Maiša: [BGT_1 V0.2](https://github.com/Mizerd/BGT_1/tree/c73dc9b010d33aca0606b0d8f5235067efeac945) (`c73dc9b`).
