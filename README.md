# Vortex TV Remote — Flipper Zero (Unleashed)

Aplicație pentru Flipper Zero care emulează o telecomandă IR pentru
televizoare Vortex, folosind protocolul RCA (adresă `0x0F`).

## Conținut

```
vortex_remote/
├── application.fam    # manifestul aplicației (.fap)
├── remote.cpp          # logica + interfața aplicației
├── vortex_10px.png     # iconița afișată în meniul Infrared
└── README.md
```

## Compilare (ufbt)

Ai nevoie de [ufbt](https://github.com/flipperdevices/flipperzero-ufbt)
instalat (`pip install ufbt`).

```bash
# în interiorul folderului vortex_remote/
ufbt update           # sincronizează SDK-ul cu firmware-ul curent (Unleashed)
ufbt                  # compilează -> dist/vortex_remote.fap
ufbt launch           # compilează + trimite direct pe Flipper (USB conectat)
```

Dacă folosești sursele complete de firmware în loc de `ufbt`, pune folderul
`vortex_remote/` în `applications_user/` și rulează scriptul de build al
firmware-ului (`./fbt fap_vortex_remote`).

## Instalare manuală (fără compilare)

Dacă ai deja fișierul `.fap` compilat:

1. Conectează Flipper-ul prin USB și deschide qFlipper.
2. Copiază `vortex_remote.fap` în `SD Card/apps/Infrared/`.
3. Pe Flipper: **Apps → Infrared → Vortex TV Remote**.

## Note tehnice

- Codurile de comandă din `remote.cpp` (`rca_commands[]`) sunt cele preluate
  din analiza inițială a telecomenzii Vortex. Dacă un buton nu funcționează
  pe televizorul tău, verifică valoarea corectă cu **Infrared → Learn New
  Remote** pe Flipper și actualizează valoarea hex corespunzătoare.
- Trimiterea semnalului folosește API-ul public `InfraredSignal`
  (`infrared_signal_alloc/set_message/transmit/free`), disponibil în
  firmware-ul Unleashed ≥ 070.
- La fiecare trimitere, LED-ul se aprinde scurt cyan și dispozitivul
  vibrează, ca feedback că semnalul a fost emis.

## Compatibilitate

Testat conceptual pe Unleashed 089. Dacă la compilare apare o eroare de tip
`infrared_signal.h: No such file or directory`, header-ul respectiv nu e
expus în SDK-ul de `.fap`-uri din versiunea ta de firmware — în acest caz,
copiază `infrared_signal.c` / `.h` din sursele firmware-ului
(`lib/infrared/`) direct în acest folder și adaugă-le ca surse suplimentare
în `application.fam` (`sources=["*.c", "*.cpp"]`).
