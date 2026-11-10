# Bitcoin duomenų analizė

## 1. Projektas

VU Blokų grandinių technologijų papildomas darbas: tikro Bitcoin formato analizė su Libbitcoin, Bitcoin Core ir Python.
Mokomoji grandinė išsaugota; šio darbo žyma – `a3-final`.

## 2. Paleidimas

Reikia Nix, Python 3.12+, Git ir GPG. Komandos vykdomos repozitorijos šaknyje.
Visa demonstracija: diegimas, kompiliavimas, laboratorija, analizė, testai ir saugus mazgo sustabdymas:

```sh
nix-shell assignment3/shell.nix --run 'bash assignment3/scripts/demo.sh'
```

Atskiros programos, kai aplinka jau paruošta:

```sh
assignment3/.venv/bin/python assignment3/scripts/node.py start
assignment3/.venv/bin/python assignment3/scripts/lab.py run
assignment3/.venv/bin/python assignment3/python/rpc_example.py
assignment3/.venv/bin/python assignment3/python/rpc_transaction.py
assignment3/.venv/bin/python assignment3/python/rpc_block.py
assignment3/.venv/bin/python assignment3/scripts/run_tests.py --live
assignment3/.venv/bin/python assignment3/scripts/node.py stop
```

Offline testai ir atskiras Merkle pavyzdys:

```sh
assignment3/.venv/bin/python assignment3/scripts/run_tests.py
assignment3/build/assignment3/cpp/bitcoin_merkle --file assignment3/fixtures/block100000-txids.txt
```

Patikrinta NixOS 26.05 x86_64: Core 32.1.0, Python 3.13.15, `python-bitcoinlib` 0.12.2, GCC 12.2.0 ir CMake 3.25.3.
Libbitcoin v3.6.0 (`52f2469`) upstream metaduomenyse dar nurodo 3.4.0; tikslūs šaltiniai ir priklausomybės – [libbitcoin.json](evidence/libbitcoin.json).
Core archyvo SHA256 ir 8 leidimo parašai patikrinti; originalūs binarai nekeičiami.

## 3. Veikimo eiga

**Mazgas → piniginės → mokėjimas → mempool → blokas → analizė.**
Regtest veikia tik lokaliai: RPC `127.0.0.1:18443`, P2P išjungtas, autentifikacija per cookie.
Privatūs duomenys laikomi `~/bitcoin-regtest-mif`; galima keisti `BGT3_DATADIR`, `BGT3_RPC_PORT` ir `BGT3_EVIDENCE`.

`MIF` piniginė kasa ir siunčia, `student` gauna. Po 101 bloko naujoje grandinėje MIF turi 50 BTC trusted ir 5000 BTC immature.
Coinbase atlygiui reikia 100 blokų brandos. Mokėjimas pirmiausia stebimas mempool su 0 patvirtinimų, po dar vieno bloko – su 1.
Užbaigto bandymo pakartojimas naujo mokėjimo nekuria; sustabdžius mazgą piniginės ir grandinė išlieka.

| Failai | Paskirtis |
|---|---|
| `cpp/` | Atskiras Bitcoin Merkle modulis; mokomoji pasirinktinė maiša nepakeista |
| `python/rpc_example.py` | Tinklas, blocks, headers, IBD, Core versija ir UTC laikas |
| `python/rpc_transaction.py` | Vin/vout, gavėjas, grąža, mokestis ir legacy / SegWit palyginimas |
| `python/rpc_block.py` | Bloko transakcijos, išvesčių suma ir 80 baitų antraštė |
| `evidence/`, `fixtures/` | Užfiksuoti RPC atsakymai ir istoriniai testiniai duomenys |

RPC naudojamas oficialus `bitcoin-cli`, o JSON sumos skaitomos kaip `Decimal`.
`RawProxy` NixOS aplinkoje nerado OpenSSL; bibliotekos `CTransaction` sėkmingai naudojama baitų analizei.
Nauji bandymai rašomi į Git ignoruojamą `.runtime/`; privatūs raktai, piniginės ir cookie į repozitoriją nepatenka.

## 4. Pagrindinės patikros

**Merkle.** Txid iškoduojamas į 32 baitus ir apverčiamas į vidinę tvarką. Gretimų vaikų 64 baitams taikoma SHA256 du kartus.
Neporiniame lygyje paskutinis mazgas dubliuojamas; vienas lapas jau yra šaknis, tuščias sąrašas atmetamas.
Galutiniai baitai apverčiami RPC rodymui. Naudojamas txid, ne wtxid.

**Mokestis.** Įvesties vertė randama pagal `(txid, vout)` iš prieš mokėjimą išsaugotų prevout duomenų.
Šiame bandyme: `5000000000 − 4899999718 − 100000000 = 282 sat`; `282 / 141 vB = 2 sat/vB`.
Rezultatas sutampa su absoliučia siuntėjo piniginės fee verte. Gavėjas atpažįstamas pagal adresą, grąža – pagal piniginės informaciją.

| Dydis | Istorinis legacy | Laboratorijos SegWit |
|---|---:|---:|
| Total / base size | 259 / 259 B | 222 / 113 B |
| Weight / vsize | 1036 WU / 259 vB | 561 WU / 141 vB |
| txid = wtxid | Taip | Ne |

`weight = 3 × base_size + total_size`, `vsize = ceil(weight / 4)`; base size gaunamas serializuojant be witness.
Šiame SegWit mokėjime parašas ir pubkey pateikiami witness, o txid jo neapima. Vien SegWit išvestis dar neįrodo witness įvestyje; bloko riba – 4 000 000 WU.

**Antraštė.** Iš version, previousblockhash, merkleroot, time, bits ir nonce savarankiškai surenkami 80 baitų.
Skaičiai rašomi little-endian, RPC maišos apverčiamos. Baitai ir SHA256d sutampa su Core; pakeitus nonce hash pasikeičia.
Tai serializacijos ir hash patikra, ne savarankiška pilna konsensuso validacija.

## 5. Rezultatai ir testai

```text
Merkle 100000: f3e94742aca4b5ef85488dc37c06c3282295ffec960994b2c0d5ac2a25a95766
Mokėjimo txid: da3ff5265282fad130d2f9b9ed535511366626ca3214f0602d36c503046b495a
Blokas 102:    14adc4f7befad3ee4b02a3a9730b026887f3466a8a64340bf1d27e7770e662db
```

| Patikra | Rezultatas |
|---|---|
| Istorinis ir regtest Merkle | Sutampa; Bitcoin ir mokomosios maišos režimai atskirti |
| Mokėjimas | 1 BTC, gavėjas vout=1, grąža vout=0; mokestis 282 sat |
| Regtest išvesčių suma | 100 BTC, įskaitant coinbase ir grąžą; tai nėra naujai išleistų monetų suma |
| Istorinis blokas 277316 | 419 transakcijų, 10322.07722534 BTC išvesčių |
| Testai | 17 C++ Merkle patikrų, 37 Python offline, 6 live; CTest 5/5 |
| Ankstesnis darbas | 47 C++ grupės ir 12 CLI atvejų praeina |

Tikra išvestis – [test-results.json](evidence/test-results.json), galutinės patikros – [audit.json](evidence/audit.json).
Mainnet RPC nenaudotas: 100000 tikrintas pagal užduoties vektorių, kiti istoriniai duomenys gauti per HTTP iš Blockstream.
Naujų piniginių raktai ir laikai keičia ID; atkuriama procedūra ir patikros. Offline duomenys veikia ir be paleisto Core.

## 6. DI ir šaltiniai

Codex (GPT-6 šeima; tikslus modelio leidimas neužfiksuotas), 2026-11-10: įgyvendinimas, peržiūra ir testai.
Rezultatai tikrinti prieš realų Core RPC, Libbitcoin ir Python skaičiavimus bei piniginės fee.
Šaltiniai: [Core](https://bitcoincore.org/en/download/), [Libbitcoin](evidence/libbitcoin.json),
[python-bitcoinlib](https://pypi.org/project/python-bitcoinlib/0.12.2/), [BIP 141](https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki).
