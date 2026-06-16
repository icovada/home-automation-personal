> 🇮🇹 **Traduzione di cortesia — NON autoritativa.** L'originale in inglese ([RECOVERY.md](RECOVERY.md)) fa fede: in caso di dubbio o discordanza vale quello, e questa copia potrebbe non essere aggiornata.

# Recupero — riprogrammare la centralina da zero

Per quando la CONTROLLINO deve essere **sostituita** o **riprogrammata** e chi lo
fa **non ha mai usato Arduino**. La prima volta calcola circa 1 ora. Ti serve un
computer Windows/Mac/Linux e un **cavo USB** (la MAXI usa USB-B, quello quadrato
tipo "stampante" — controlla sull'unità).

> Questa procedura riprogramma soltanto la **logica della centralina**. **Non**
> tocca il cablaggio di rete/pompe. Tutto ciò che riguarda il lato potenza è
> compito di un **elettricista**.

> ✅ **Strada più veloce — sostituisci, non riprogrammare.** Se nella borsa dei
> ricambi c'è un **Controllino di ricambio già programmato** (vedi [BOM.it.md](BOM.it.md) → "Ricambi"),
> non ti serve nulla di tutto questo: togli tensione, sposta i fili **etichettati**
> dall'unità guasta al ricambio, ridai tensione ed esegui i controlli in
> [MANUAL.it.md](MANUAL.it.md) §7. I passi qui sotto servono solo per **preparare**
> quel ricambio, oppure se non esiste un ricambio già programmato. Dopo aver usato
> il ricambio, programmane e imbusta uno nuovo così non rimani mai senza.

---

## 0. Procurati il codice sorgente

Si trova in due posti (vedi il cassetto della documentazione):

- **Chiavetta USB** nel cassetto della documentazione — copia sul computer l'intera cartella `eli_pompe`.
- **GitHub:** _________________________________________________ → "Code" → "Download ZIP", poi decomprimila.

Ti serve la cartella **`eli_pompe`** che contiene almeno `eli_pompe.ino` e
`pompe.h`. (La cartella `tests/` non serve per la programmazione.) Il nome della
cartella **deve** rimanere `eli_pompe` e deve contenere `eli_pompe.ino`.

---

## 1. Installa l'Arduino IDE

Scarica da <https://www.arduino.cc/en/software> e installa l'**Arduino IDE 2.x**.
Aprilo.

## 2. Aggiungi il supporto per la scheda CONTROLLINO

1. **File → Impostazioni (Preferences).**
2. In **"Additional boards manager URLs"** (URL aggiuntivi per il gestore schede),
   incolla l'URL del pacchetto CONTROLLINO. Trovi l'URL aggiornato su
   <https://www.controllino.com> (cerca "Arduino IDE board installation"). Al
   momento in cui scriviamo è:
   `https://raw.githubusercontent.com/CONTROLLINO-PLC/CONTROLLINO_Library/master/Boards/package_ControllinoHardware_index.json`
   *(Se quel link non funziona più, sul sito CONTROLLINO trovi quello aggiornato.)*
3. **OK.**
4. Apri **Strumenti → Scheda → Gestore schede (Tools → Board → Boards Manager)**,
   cerca **CONTROLLINO** e clicca **Install** (Installa).

## 3. Installa la libreria CONTROLLINO

Apri **Strumenti → Gestione librerie (Tools → Manage Libraries)** (oppure
**Sketch → #include libreria → Gestione librerie / Sketch → Include Library →
Manage Libraries**), cerca **CONTROLLINO** e fai **Install** (Installa). Questo
fornisce `Controllino.h`, di cui lo sketch ha bisogno.

> Questo sketch è **autonomo**: non gli serve **nessun'altra libreria** (niente
> Ethernet, niente MQTT). Se l'IDE si lamenta che manca una libreria diversa, hai
> lo sketch sbagliato — assicurati di aver aperto `eli_pompe` e non una delle
> altre centraline nel repository.

## 4. Apri lo sketch

**File → Apri (File → Open)**, vai alla cartella `eli_pompe` e apri
**`eli_pompe.ino`**. (`pompe.h` si apre automaticamente come seconda scheda —
deve trovarsi nella stessa cartella.)

## 5. Seleziona la scheda e la porta

- **Strumenti → Scheda → CONTROLLINO → CONTROLLINO MAXI (Tools → Board → CONTROLLINO → CONTROLLINO MAXI).**
- Collega la centralina al computer tramite USB.
- **Strumenti → Porta (Tools → Port) →** scegli la porta che compare quando la colleghi
  (Windows: `COMx`; Mac: `/dev/cu.usbmodem…` o `/dev/cu.usbserial…`).
  - Se non compare nessuna porta, potrebbe servirti il driver USB-seriale. La MAXI
    usa un chip USB **ATmega2560 / 16U2** — lo stesso di un Arduino Mega:
    installando l'Arduino IDE di solito il driver viene fornito. Su schede più
    vecchie potrebbe essere un chip **FTDI** o **CH340** — installa il driver di
    quel produttore se Windows mostra un dispositivo sconosciuto.

## 6. Compila (Verifica/Verify)

Clicca **✓ Verifica (Verify)** (in alto a sinistra). Deve terminare con **"Done
compiling."** Se dà errore, ricontrolla i passi 2–4 (pacchetto schede, libreria
CONTROLLINO, sketch giusto).

## 7. Carica (Upload)

Clicca **→ Carica (Upload)**. Aspetta **"Done uploading."** I LED TX/RX della
scheda lampeggiano durante il trasferimento.

## 8. Verifica che sia attiva

Apri **Strumenti → Monitor seriale (Tools → Serial Monitor)**, imposta **115200
baud**. Dovresti vedere:

```
Start eli_pompe
PompeManager ready
End setup
```

e poi una riga `[status] …` circa ogni 10 secondi.

## 9. Ricontrolla la taratura (importante)

I valori di taratura del sensore di corrente (`ADC_AT_0A` / `ADC_AT_FS` in
`pompe.h`) fanno parte del sorgente, quindi una copia corretta ha già i numeri
giusti. **Ma verificali:** con una pompa in funzione, il valore `A=` sul seriale
dovrebbe corrispondere a una pinza amperometrica entro circa 0,5 A. Se è sballato,
rifai **MANUAL.it.md §7 passo 2** e **salva i nuovi valori nel repository (commit)**
così la persona successiva li eredita.

## 10. Di nuovo in servizio

Metti entrambi i selettori su **AUTO** ed esegui le parti rilevanti della lista
di messa in servizio (**MANUAL.it.md §7**): prova ogni pompa in **MANUAL**, poi
un ciclo completo di riempimento/svuotamento in **AUTO**, poi fai scattare
l'allarme e verifica lampeggiante + sirena.

---

## (Facoltativo) Prova la logica su un PC senza l'hardware

Se un programmatore vuole confermare che la logica di controllo sia integra prima
di programmare:

```sh
cd eli_pompe/tests
make
```

Aspettati `64 checks, 0 failures · ALL TESTS PASSED`. Vedi `tests/README.md`.

---

## Riferimento rapido

| Cosa | Valore |
|-------|-------|
| Scheda | **CONTROLLINO MAXI** |
| Sketch | `eli_pompe/eli_pompe.ino` (+ `pompe.h` nella stessa cartella) |
| Baud seriale | **115200** |
| Librerie aggiuntive | **nessuna** (solo il pacchetto schede CONTROLLINO + la libreria CONTROLLINO) |
| Build con `arduino-cli` | `arduino-cli compile --fqbn CONTROLLINO_Boards:avr:controllino_maxi eli_pompe` |
| Upload con `arduino-cli` | `arduino-cli upload --fqbn CONTROLLINO_Boards:avr:controllino_maxi -p <port> eli_pompe` |
