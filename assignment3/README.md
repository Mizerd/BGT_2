# 3-oji papildoma užduotis: Bitcoin duomenų analizė

## 1. Apimtis

VU Blokų grandinių technologijos, 2026. Įgyvendintos visos trys dalys; balus skiria dėstytojas.

| Dalis | Rezultatas | Galima iki |
|---|---|---:|
| Libbitcoin | Bitcoin SHA256d Merkle medis, istorinis ir regtest palyginimas | 0,25 |
| Bitcoin Core | Atskiras regtest, piniginės, 101 blokas ir patvirtintas mokėjimas | 0,5 |
| Python | RPC, prevout ir mokesčio analizė, SegWit, bloko antraštė | 1,25 |
| **Iš viso** | Dėstytojo vertinamas papildomas darbas | **2** |

Eiga: patikrintas Core → atskiras mazgas → piniginės → mokėjimas → trys Python programos → Merkle ir testai.
2-ojo darbo kodas ir žymos `v0.1`, `v0.2`, `v0.3` išsaugoti. Pateikimo žyma – `a3-final`.

## 2. Aplinka ir diegimas

| Įrankis | Patikrinta versija |
|---|---|
| OS | NixOS 26.05, Linux x86_64 |
| Bitcoin Core ir CLI | Oficialus 32.1 leidimas, vykdomųjų failų versija 32.1.0 |
| Python | 3.13.15; diegimo skriptui reikia 3.12+ |
| Python biblioteka | `python-bitcoinlib==0.12.2`, wheel SHA256 fiksuota `requirements.txt` |
| C++ / CMake | GCC 12.2.0, C++20 / 3.25.3, `Release` |
| Libbitcoin | `v3.6.0`, commit `52f24693f8bdd4c79d66839d90132179f486e7d8` |
| Boost / secp256k1 | 1.75.0 / 0.3.2 |
| Nixpkgs | `70bdadeb94ffc8806c0570eb5c2695ad29f0e421` |

Libbitcoin naudojamas per `bitcoin/bitcoin.hpp`, `bc::decode_base16` ir `bc::bitcoin_hash`.
Šio leidimo upstream `pkg-config libbitcoin` ir `version.hpp` dar rodo **3.4.0**;
tikslus v3.6.0 šaltinis ir Nix pataisa (`<cstddef>` įtraukimas) užfiksuoti [aplinkos įrodymuose](evidence/libbitcoin.json).
Nenaudojama nesuderinama v4 API ar kitos bibliotekos Merkle pakaitalas.

Reikia įdiegto Nix, Python 3.12+, Git ir GPG. Iš repozitorijos šaknies:

```sh
nix-shell assignment3/shell.nix
```

Toliau komandas vykdykite šiame shell. Visa demonstracija vienu paleidimu:

```sh
bash assignment3/scripts/demo.sh
```

Skriptas patikrina Core archyvą, sukompiliuoja, paleidžia laboratoriją, visas programas bei testus ir sustabdo tik savo mazgą.
Core SHA256 tikrinama pagal pasirašytą `SHA256SUMS`: patvirtinti 8 parašai ir tikėtinas
`E777299FC265DD04793070EB944D35F9AC3DB76A` fingerprint. NixOS naudoja glibc loader;
originalūs binarai nekeičiami ir palyginami su patikrinto archyvo baitais.

## 3. Atskiros paleidimo komandos

Diegimas ir C++ programa:

```sh
python3 assignment3/scripts/setup.py
cmake -S . -B assignment3/build -DBUILD_ASSIGNMENT3=ON -DCMAKE_BUILD_TYPE=Release
cmake --build assignment3/build -j 2
assignment3/build/assignment3/cpp/bitcoin_merkle --file assignment3/fixtures/block100000-txids.txt
assignment3/build/assignment3/cpp/bitcoin_merkle --compare-custom --file assignment3/fixtures/block100000-txids.txt
ctest --test-dir assignment3/build -R bitcoin_merkle_tests --output-on-failure
```

Mazgas ir tikras 1 BTC mokėjimas:

```sh
assignment3/.venv/bin/python assignment3/scripts/node.py start
assignment3/.venv/bin/python assignment3/scripts/lab.py run
assignment3/.venv/bin/python assignment3/python/rpc_example.py
assignment3/.venv/bin/python assignment3/python/rpc_transaction.py
assignment3/.venv/bin/python assignment3/python/rpc_block.py
assignment3/build/assignment3/cpp/bitcoin_merkle --file assignment3/.runtime/lab/block-txids.txt
assignment3/.venv/bin/python assignment3/scripts/run_tests.py --live
assignment3/.venv/bin/python assignment3/scripts/node.py stop
```

Offline testai ir išsaugoti duomenys, mazgui neveikiant:

```sh
assignment3/.venv/bin/python assignment3/scripts/run_tests.py
assignment3/.venv/bin/python assignment3/python/rpc_transaction.py --offline --evidence assignment3/evidence/lab
assignment3/.venv/bin/python assignment3/python/rpc_block.py --offline --evidence assignment3/evidence/lab
```

Numatytasis privatus katalogas – `~/bitcoin-regtest-mif`, RPC – tik `127.0.0.1:18443`.
P2P prievadas 18444 sukonfigūruotas, bet klausymas ir tinklas išjungti. Naudojama vietinė cookie autentifikacija.
Prireikus keiskite `BGT3_DATADIR`, `BGT3_RPC_PORT`, `BGT3_EVIDENCE`; piniginių ir cookie į Git nekelkite.

Užbaigtas bandymas pakartotinai nesiunčia mokėjimo ir nekasa blokų. Nebaigtas žurnalas automatiškai nekartojamas:
pirma reikia patikrinti piniginę ir žurnalą. Duomenų katalogas netrinamas; svetimas katalogas ar PID neperimamas.

## 4. Programos ir duomenys

| Vieta | Paskirtis |
|---|---|
| `cpp/` | Atskiras SHA256d Merkle modulis ir 17 C++ patikrų |
| `python/rpc_example.py` | Aktyvus tinklas, blocks, headers, IBD, Core versija ir UTC stebėjimo laikas |
| `python/rpc_transaction.py` | Vin/vout, gavėjas ir grąža, mokestis, legacy ir SegWit |
| `python/rpc_block.py` | Visos bloko išvestys, Merkle ir savarankiškai surinkta antraštė |
| `python/common.py`, `bitcoin_data.py` | RPC adapteris, tikslūs satoshi ir baitų skaičiavimai |
| `scripts/node.py`, `lab.py` | Tik projekto mazgas, piniginių sukūrimas arba įkėlimas, būsenų žurnalas |
| `tests/`, `scripts/run_tests.py` | Offline ir live testai su išsaugota tikra išvestimi |
| `evidence/`, `fixtures/` | Komituoti 2026-11-10 RPC stebėjimai ir aiškiai pažymėti istoriniai duomenys |
| `.runtime/`, `.deps/`, `.venv/` | Nauji bandymai, lokalūs įrankiai ir aplinka; Git ignoruojami |

`RawProxy` iš 0.12.2 numatytoje NixOS aplinkoje nerado OpenSSL (`BN_add` importo klaida).
Todėl RPC vykdomas per oficialų `bitcoin-cli` su tuo pačiu `-regtest`, katalogu ir prievadu;
JSON sumos skaitomos kaip `Decimal`. Bibliotekos `CTransaction` naudojama sėkmingai,
įskaitant serializaciją be witness. Bibliotekos kodas nekeistas; pasirinkimas užfiksuotas [JSON](evidence/python-client.json).

## 5. Merkle ir mazgo rezultatai

Txid hex iškoduojamas į 32 baitus ir apverčiamas į vidinę tvarką. Tėvui jungiami 64 baitai
ir taikoma SHA256(SHA256(data)). Neporiniame lygyje, turinčiame daugiau nei vieną mazgą,
paskutinis dubliuojamas. Vienas lapas jau yra šaknis, tuščias sąrašas atmetamas.
Galutinė šaknis apverčiama RPC rodymui; naudojamas **txid, ne wtxid**.

```text
Blokas 100000: f3e94742aca4b5ef85488dc37c06c3282295ffec960994b2c0d5ac2a25a95766
Regtest txid: da3ff5265282fad130d2f9b9ed535511366626ca3214f0602d36c503046b495a
Blokas 102:   14adc4f7befad3ee4b02a3a9730b026887f3466a8a64340bf1d27e7770e662db
Regtest root: 361ee02fdee8c1c9fbb6ceb14dfac10b8b19a7cd4a03f11fd1706b2aa5574f40
```

Istorinė šaknis sutampa su užduoties vektoriumi. Tam pačiam keturių ID rinkiniui mokomoji maiša duoda
`aef2e8316423fe0ade32b3199ff04637a3702b2928fd5febc852b8e7c9143c81`, todėl režimai atskirti.
Regtest šaknis sutampa su Core ir apima coinbase bei mokėjimą.

| Stebėjimas | Tikras rezultatas |
|---|---|
| Naujas mazgas | `regtest`, blocks=0, headers=0, ryšiai=0 |
| Piniginės | `MIF` kasa ir siunčia; `student` gauna; vardai nėra atskira valiuta |
| Po 101 bloko | MIF trusted=50 BTC, immature=5000 BTC |
| Prieš patvirtinimą | Mokėjimas mempool, 0 patvirtinimų |
| Po dar vieno bloko | Aukštis 102, 1 patvirtinimas, mokėjimo mempool nebėra |

Coinbase reikia 100 blokų brandos; šiame naujos grandinės scenarijuje piniginė leidžia išleisti pirmą atlygį.
Pakartotinai naudotai grandinei 50/5000 sumos nežadamos. Nulis peers ar senas regtest genesis laikas savaime nėra gedimas:
tai izoliuota valdoma grandinė. Aktyvus tinklas nustatomas iš `chain`, o 320100 yra Core programos versija.
Regtest pradinis atlygis yra 50 BTC ir mažėja kas 150 blokų; tai nėra dabartinis mainnet atlygio grafikas.

## 6. Mokestis, SegWit ir antraštė

Kiekviena įvestis siejama su ankstesne išvestimi pagal **(txid, vout)**. `prevouts.json` išsaugotas prieš siuntimą;
vertės papildomai sutikrintos su ankstesnės transakcijos dekodavimu. Jau išleistoms išvestims `gettxout` nenaudojamas.

```text
5000000000 - (4899999718 + 100000000) = 282 sat
282 / 141 vB = 2 sat/vB
```

Tai sutampa su `abs(MIF.gettransaction.fee)` = 282 sat. Gavėjas šiame bandyme yra **vout=1**, grąža – vout=0;
programa jų neparinko pagal vietą, o pagal gavėjo adresą ir piniginės `ismine` / `ischange` informaciją.
Coinbase įprasto prevout neturi, todėl jai ši mokesčio formulė netaikoma. Išvestis be adreso irgi apdorojama.

| Dydis | Legacy iš bloko 100000 | Laboratorijos SegWit |
|---|---:|---:|
| Total / base size | 259 / 259 B | 222 / 113 B |
| Weight / vsize | 1036 WU / 259 vB | 561 WU / 141 vB |
| txid = wtxid | Taip | Ne |
| scriptSig / witness | Parašas ir pubkey / nėra | Tuščias / parašas ir pubkey |

`weight = 3 × base_size + total_size`; `vsize = ceil(weight / 4)`. Base size gaunamas tikra serializacija be witness
ir tikrinamas su bibliotekos bei Core rezultatais. Legacy atveju weight=4×size ir vsize=size.
SegWit išvesties adresas dar neįrodo, kad įvestis turi witness. Txid/wtxid yra duomenų identifikatoriai;
parašui naudojamas atskiras SIGHASH pranešimas. Bloko riba – 4 000 000 WU, ne paprasti 4 MB.

Antraštė surenkama iš dekoduotų laukų, ne permaišant gautą raw header:

| Laukai | Formatas |
|---|---|
| version | 4 B little-endian, išlaikomas 32 bitų laukas |
| previousblockhash, merkleroot | Po 32 B, apverčiant RPC rodomą tvarką |
| time, bits, nonce | Po 4 B little-endian; bits pirmiausia skaitomas kaip hex skaičius |

Gauti **80 baitų tiksliai sutampa su Core raw header**, o SHA256d rodymo tvarka – su aukščiau pateiktu bloko hash.
Pakeitus nonce iš 0 į 1 gaunama `eb77774b7d9c5d7fc3e983ed06daf89a96720f51468f30c7070233f9f2aa5c9a`.
Tai patvirtina serializaciją ir hash rekonstrukciją, bet nėra savarankiška pilna konsensuso validacija.

Regtest visų išvesčių suma – **100.00000000 BTC**: įtraukti coinbase, gavėjas ir grąža.
Tai nėra nei 100 naujai išleistų BTC, nei mokėjimų ekonominė apimtis.
Papildomai atkurtas istorinis blokas 277316: 419 transakcijų, **10322.07722534 BTC** išvesčių.
Mainnet RPC neužklaustas: 100000 naudotas pateiktas vektorius, o legacy raw ir 277316 – pažymėti HTTP rinkiniai iš Blockstream.

## 7. Testai, atkuriamumas ir įrodymai

| Patikra | Rezultatas |
|---|---|
| C++ Merkle: 1/2/3/5 lapai, odd, hex, baitų tvarka, 100000 ir regtest | 17/17 |
| Python offline: vin/vout, prevout, sumos, klaidos, witness, header, istorinis blokas | 37/37 |
| Live RPC: tas pats mazgas, mokėjimo būsenos, fee, C++ root, header ir saugus pakartojimas | 6/6 |
| CTest, mazgui veikiant arba neveikiant | 5/5, niekas nepraleista |
| Ankstesnis 2 darbas | 47 C++ grupės ir 12 CLI atvejų praeina |
| Antra švari laboratorija | Patvirtintas naujas mokėjimas, 6 live testai; pirminiai fixtures nepakeisti |

Tikra komandų išvestis – [test-results.json](evidence/test-results.json), patikros pagal PDF – [audit.json](evidence/audit.json).
Visų stebėjimų UTC laikas ir kilmė pateikti JSON. `evidence/lab` yra užfiksuotas pirmas bandymas;
nauji bandymai rašomi į `.runtime/lab`. Naujos piniginės ir laikai duos kitus txid bei blokų hash – atkuriama procedūra ir patikros.
Privačios piniginės lieka už Git; sustabdžius mazgą duomenys išsaugomi.

## 8. DI ir šaltiniai

Codex (GPT-6 šeima; tikslus modelio leidimas neužfiksuotas), 2026-11-10: aplinkos parinkimas,
įgyvendinimas, testai ir klaidų taisymas. Pasiūlymai tikrinti su realiu Core RPC, Libbitcoin prieš užduoties
vektorių ir Python hashlib, prevout sumomis prieš piniginės fee, bei nepriklausomai surinktais header baitais.
Vertintų 2-ojo darbo žymų turinys nekeistas.

- Užduoties PDF: 8 puslapiai, SHA256 ir išsaugotos žymos [specification.json](evidence/specification.json).
- [Bitcoin Core diegimas ir parašų patikra](https://bitcoincore.org/en/download/), [Core RPC](https://bitcoincore.org/en/doc/32.0.0/).
- [Libbitcoin v3.6.0 šaltinis](https://github.com/libbitcoin/libbitcoin-system/tree/52f24693f8bdd4c79d66839d90132179f486e7d8), [SHA256d funkcija](https://github.com/libbitcoin/libbitcoin-system/blob/52f24693f8bdd4c79d66839d90132179f486e7d8/src/math/hash.cpp).
- [python-bitcoinlib 0.12.2](https://pypi.org/project/python-bitcoinlib/0.12.2/), [BIP 141](https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki).
- [Antraštės formatas](https://developer.bitcoin.org/reference/block_chain.html), [istorinių HTTP duomenų kilmė](evidence/historical-block277316.json).

Diegiklis ir proceso nuosavybės patikra pritaikyti patikrintai Linux x86_64 aplinkai.
Kitoms OS reikia atitinkamų oficialių Core binarų ir proceso patikros adaptavimo; mainnet grandinė nesinchronizuota.
