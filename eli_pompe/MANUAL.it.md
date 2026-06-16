> 🇮🇹 **Traduzione di cortesia — NON autoritativa.** L'originale in inglese ([MANUAL.md](MANUAL.md)) fa fede: in caso di dubbio o discordanza vale quello, e questa copia potrebbe non essere aggiornata.

# Controllore stazione di pompaggio di aggottamento — Manuale di installazione e uso

Questo quadro gestisce automaticamente **due pompe di aggottamento che condividono un unico tubo di mandata**.
Fa funzionare una sola pompa per volta, le alterna in modo che si usurino in modo uniforme, sorveglia
la corrente elettrica di ciascuna pompa per rilevare una pompa bloccata o guasta, commuta automaticamente
sulla pompa funzionante e attiva un allarme locale quando qualcosa richiede
attenzione. È un controllore **autonomo** (senza rete/Wi-Fi).

> ⚠️ **Sicurezza.** I motori delle pompe funzionano a tensione di rete. Tutto il cablaggio di rete, i relè
> di commutazione e la protezione del motore devono essere installati da un elettricista qualificato e
> rispettare le normative locali. Il controllore pilota **solo bobine di relè e circuiti di
> segnale** — non commuta direttamente la corrente del motore. Togliere sempre tensione
> prima di cablare.

---

## 1. Come funziona (in parole semplici)

Tre galleggianti sono posizionati a tre altezze nel pozzetto:

- **MIN** (il più basso) — "abbastanza vuoto, ferma la pompa."
- **1/2** (intermedio) — "l'acqua è salita, avvia una pompa."
- **3/4** (il più alto) — "ALLARME acqua alta." *(Opzionale — installalo quando sei pronto; il controllore lo supporta già.)*

Ciclo normale: l'acqua sale fino al galleggiante **1/2** → una pompa parte → l'acqua scende fino al
galleggiante **MIN** → la pompa si ferma. La volta successiva funziona l'*altra* pompa (si
alternano).

Una **pinza amperometrica** (Seneca T201) su ciascuna pompa indica al controllore quanti
ampere sta assorbendo la pompa. Se una pompa in funzione assorbe troppo poco (in realtà non
sta pompando, è a secco o è scattata la protezione), troppo (bloccata), o non riesce ad abbassare il livello dell'acqua in un
tempo ragionevole (intasata), il controllore dichiara quella pompa **guasta**,
commuta sull'altra pompa e ritenta più tardi quella guasta. Se una pompa
continua a guastarsi viene **bloccata** finché qualcuno non interviene e preme RIPRISTINO (RESET).

---

## 2. Cosa c'è sul quadro

**Spie luminose**

| Spia | Significato |
|------|---------|
| Pompa 1 / Pompa 2 **MARCIA** (RUN) | quella pompa è alimentata in questo momento |
| Pompa 1 / Pompa 2 **GUASTO** (FAULT) | **fissa** = guasto temporaneo, ritenterà automaticamente · **lampeggiante** = bloccata, richiede RIPRISTINO (RESET) |
| Livello **MIN / 1-2 / 3-4** | quale galleggiante è attualmente bagnato · una spia di livello **lampeggiante** = quel galleggiante sembra guasto |
| **ALLARME** (ALARM, quadro) | lampeggia durante un'emergenza |

**Segnali remoti**

- **Grande lampeggiante rosso** — si accende ogni volta che c'è *un qualsiasi* problema (un guasto di una pompa, un guasto di un galleggiante o un'emergenza). "Vieni a controllare."
- **Sirena** — suona solo in una vera **emergenza** (acqua alta, o entrambe le pompe non disponibili). "Agisci ora."

**Comandi**

- Selettore **Manuale–Spento–Auto** per ciascuna pompa (vedi §6).
- Pulsante **SILENZIA (SILENCE)** — disattiva la **sirena**; il lampeggiante e le spie restano accesi.
- Pulsante **RIPRISTINO (RESET)** — cancella i guasti e riabilita una pompa bloccata (dopo aver risolto la causa).

---

## 3. Cosa serve

- CONTROLLINO MAXI, alimentato a 24 V DC.
- Due relè di commutazione pompa — **relè Finder montati su zoccolo** (a innesto, così uno guasto si sostituisce senza ricablare), dimensionati per la corrente della pompa. Un interblocco hardware tra di essi è **opzionale** (vedi §5.1).
- 3 galleggianti (normalmente aperti, che chiudono al salire dell'acqua). Il galleggiante 3/4 per ora è opzionale.
- 2 × trasduttori di corrente **Seneca T201** (uscita 4-20 mA), uno montato su ciascuna alimentazione delle pompe.
- 2 × **resistenze di shunt (burden) di precisione** (≈ 500 Ω, 0,1 %) — una per ciascun anello 4-20 mA (vedi §5.3).
- Spie a 24 V, un lampeggiante a 24 V (consigliato di tipo autolampeggiante) e una sirena.
- Selettori MOA (Manuale, Spento, Auto) e due pulsanti a impulso (Silenzia, Ripristino).

---

## 4. Assegnazione morsetti / pin

Cablare ai morsetti del CONTROLLINO MAXI come indicato di seguito (corrisponde alla mappa pin del firmware).

**Uscite relè**

| Morsetto | Collegare a |
|----------|------------|
| `R0` | Bobina relè di commutazione Pompa 1 (Finder) |
| `R1` | Bobina relè di commutazione Pompa 2 (Finder) |
| `R2` | Lampeggiante (grande lampada rossa lampeggiante) |
| `R3` | Sirena |

**Uscite digitali (spie 24 V)**

| Morsetto | Spia |
|----------|------|
| `D0` / `D1` | MARCIA (RUN) Pompa 1 / Pompa 2 |
| `D2` / `D3` | GUASTO (FAULT) Pompa 1 / Pompa 2 |
| `D4` / `D5` / `D6` | Livello MIN / 1-2 / 3-4 |
| `D7` | ALLARME (ALARM) quadro |

**Ingressi**

| Morsetto | Ingresso |
|----------|-------|
| `A0` / `A1` | Pinza amperometrica Pompa 1 / Pompa 2 (T201, 4-20 mA tramite resistenza di shunt) |
| `A2` / `A3` / `A4` | Galleggiante MIN / 1-2 / 3-4 |
| `A5` | Pulsante Silenzia |
| `A6` | Pulsante Ripristino |
| `A7` / `A8` | Pompa 1 MANUALE / AUTO (dal suo selettore MOA) |
| `A9` / `IN0` | Pompa 2 MANUALE / AUTO (dal suo selettore MOA) |
| `IN1` | di riserva |

---

## 5. Note di cablaggio

### 5.1 Relè di commutazione pompa — l'interblocco è opzionale
Deve funzionare una sola pompa per volta (condividono un unico tubo di mandata). **Il
firmware lo garantisce via software**: comanda l'accensione di una sola pompa per volta, e
la suite di test verifica che le due uscite non siano mai alimentate contemporaneamente. Farle funzionare entrambe
insieme fa solo sì che si contendano il tubo e spostino meno acqua — non
danneggia nulla — quindi un interblocco hardware è una **ridondanza opzionale "cintura e bretelle"**,
non obbligatoria.

Questa realizzazione usa **relè Finder montati su zoccolo** su `R0` / `R1`, scelti così che un
relè guasto si estrae e si reinserisce senza ricablare. Dimensionare ciascun relè per la corrente di esercizio
**e di spunto** della pompa (la corrente di spunto di un motore è diverse volte la sua
corrente di esercizio).

Se volessi l'assicurazione extra, l'opzione più economica è un **interblocco
elettrico**: cablare il contatto ausiliario/di riserva NC di ciascun relè nell'alimentazione della
bobina dell'altro, così che l'eccitazione di uno disecciti l'altro. (Esiste una barra di interblocco
meccanico per dispositivi tipo contattore, ma non è applicabile ai relè a innesto.)

### 5.2 Galleggianti
Usare galleggianti normalmente aperti che **chiudono verso +24 V al salire dell'acqua** (attivi-alto).
Portare i galleggianti MIN, 1/2 e (opzionale) 3/4 ad `A2`, `A3`, `A4`. L'ingresso 3/4 può
essere lasciato scollegato per ora — viene letto come "asciutto" e non provocherà un falso allarme.
(Se hai solo galleggianti normalmente chiusi, imposta `FLOAT_ACTIVE_HIGH` a `false` nel
firmware.)

### 5.3 Pinze amperometriche (Seneca T201) — l'unica parte delicata
Ciascun T201 fornisce un'uscita **4-20 mA** proporzionale alla corrente RMS della sua pompa (imposta il
campo del T201 su **0-10 A**, oppure regola `AMP_SPAN_A` nel firmware per farlo corrispondere). Gli
ingressi analogici del CONTROLLINO MAXI leggono **0-24 V** a fondo scala (NON 0-5 V), quindi inserisci una
**resistenza di shunt tra l'ingresso e massa** per convertire la corrente dell'anello in una
tensione che rientri in quel campo:

- ≈ **500 Ω** danno ~**2-10 V** per 4-20 mA — un buon valore di default: buona risoluzione,
  ampiamente all'interno del campo 24 V e dentro la capacità di pilotaggio del T201.
  (≈ 250 Ω → ~1-5 V funziona anche, ma usa una porzione minore del campo d'ingresso.)
- **Non** dimensionare la resistenza di shunt in modo che 20 mA si avvicinino a 24 V — lascia margine.
- Se il filo dell'anello si interrompe, l'ingresso legge ~0 V; il controllore lo rileva come
  guasto sensore "anello di corrente interrotto".

Poi **calibra** (§7).

### 5.4 Lampeggiante e sirena
Metti il **lampeggiante** su `R2` e la **sirena** su `R3` come mostrato, così che il pulsante SILENZIA
(SILENCE) possa zittire la sirena mentre il lampeggiante resta acceso. Usa un lampeggiante
**autolampeggiante** (lampeggia da solo; il relè si limita ad alimentarlo). Se preferisci, puoi
cablare la sirena in parallelo al lampeggiante su un unico relè — ma allora SILENZIA (SILENCE)
zittirà entrambi.

---

## 6. Funzionamento — Manuale / Spento / Auto

Ciascuna pompa ha un selettore a 3 posizioni:

- **AUTO** — funzionamento automatico normale (galleggianti + monitoraggio corrente). Lascia entrambi qui per il funzionamento incustodito.
- **SPENTO (OFF)** — quella pompa è disabilitata e non funzionerà, nemmeno in emergenza.
- **MANUALE (MANUAL)** — fa funzionare subito quella pompa, ignorando i galleggianti (per test/adescamento). L'interblocco a pompa singola vale comunque; se entrambe sono su MANUALE, funziona solo la Pompa 1.

---

## 7. Lista di controllo per la messa in servizio

1. **Accendi** con entrambi i selettori su **SPENTO (OFF)**. Verifica che nessuna pompa funzioni e che le spie siano coerenti.
2. **Calibra le pinze amperometriche** (da fare una volta, per ciascuna pompa):
   - Collega un portatile alla USB del CONTROLLINO e apri il Monitor Seriale di Arduino a **115200 baud**. Una riga di stato viene stampata ogni ~10 s, comprendente `A=` (gli ampere misurati delle due pompe).
   - Con la pompa **spenta**, la lettura dovrebbe attestarsi vicino a **0 A**. Con la pompa **in funzione a un carico noto**, confronta gli ampere visualizzati con una pinza amperometrica.
   - Se non corrispondono, regola `ADC_AT_4MA` / `ADC_AT_20MA` in `pompe.h` (mappano la lettura analogica grezza su 4 mA e 20 mA). Il metodo più semplice: annota i conteggi analogici grezzi a un valore noto di 4 mA (pompa spenta) e 20 mA (o fondo scala) e inseriscili. Riprogramma e ricontrolla.
   - Imposta `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` per delimitare la reale corrente di esercizio della tua pompa (default 2-8 A) con un certo margine.
3. **Imposta `DRAIN_TIMER_MS`** all'incirca al tempo che una pompa funzionante impiega per abbassare il livello dal galleggiante 1/2 al galleggiante MIN — viene usato come riserva nel caso il galleggiante MIN si guasti.
4. **Prova ciascuna pompa in MANUALE (MANUAL)**, verifica che il relè corretto si ecciti, che la spia MARCIA (RUN) si accenda e che la corrente sia letta in modo sensato.
5. **Prova l'AUTO**: alza i galleggianti manualmente (o riempi il pozzetto) e verifica che una pompa parta a 1/2 e si fermi a MIN, e che la **pompa di testa si alterni** ad ogni ciclo.
6. **Prova l'allarme**: fai scattare il galleggiante 3/4 (o il suo ingresso) e verifica lampeggiante **e** sirena; premi **SILENZIA (SILENCE)** e verifica che la sirena si fermi ma il lampeggiante resti acceso.
7. Lascia entrambi i selettori su **AUTO**.

---

## 8. Cosa fare quando qualcosa segnala

| Cosa vedi | Significato | Azione |
|---------|---------|--------|
| **Lampeggiante acceso, nessuna sirena**, una spia GUASTO (FAULT) fissa | Una pompa si è guastata; l'altra sta coprendo. Ritenterà automaticamente fra ~10 min. | Controlla quella pompa quando comodo (ostruzione, valvola di ritegno, interruttore). Nessuna fretta. |
| **Lampeggiante acceso**, una spia GUASTO (FAULT) **lampeggiante** | Quella pompa è **bloccata** dopo guasti ripetuti. | Ripara la pompa, poi premi **RIPRISTINO (RESET)**. |
| Una **spia di livello lampeggiante** | Quel galleggiante sembra guasto (un galleggiante più alto è bagnato ma questo segnala asciutto). | Controlla/sostituisci quel galleggiante. La stazione continua a funzionare nel frattempo. |
| **Sirena + lampeggiante**, spia ALLARME (ALARM) lampeggiante | **Emergenza**: acqua alta (3/4) e/o entrambe le pompe non disponibili. | Intervieni **ora**. Premi SILENZIA (SILENCE) per zittire la sirena mentre lavori. Controlla pompe, alimentazione e galleggianti. |

**Procedura di RIPRISTINO (RESET):** dopo aver risolto la causa, premi **RIPRISTINO (RESET)** una volta. Questo cancella i
guasti, riabilita qualsiasi pompa bloccata e disattiva il silenziamento dell'allarme. Se il
problema di fondo non è risolto, la pompa si guasterà semplicemente di nuovo.

---

## 9. Risoluzione dei problemi

| Sintomo | Causa probabile | Verifica |
|---------|--------------|-------|
| La pompa si guasta subito (sottocorrente) ad ogni avvio | La pompa non assorbe corrente | Interruttore/protezione termica scattata, motore scollegato, cablaggio pinza/resistenza di shunt, campo del T201 |
| La pompa si guasta per **sovracorrente** | Girante bloccata / rotore bloccato | Ostruzione meccanica; condizione del motore |
| La pompa si guasta come **"non drena"** | Pompa ma il livello non scende | Aspirazione/girante intasata, valvola di ritegno bloccata, mandata ostruita |
| Una **spia GUASTO (FAULT) lampeggia** e la pompa non riparte | Bloccata (troppi guasti) | Ripara la pompa, premi **RIPRISTINO (RESET)** |
| Lettura di corrente errata sulla Seriale | Non calibrata | Rifai §7 passo 2 (`ADC_AT_4MA`/`ADC_AT_20MA`) |
| La pompa non parte anche se l'acqua è alta | Selettore su SPENTO (OFF), o attesa anti-ciclaggio breve, o bloccata | Controlla il selettore MOA e le spie GUASTO (FAULT) |
| Entrambe le pompe ferme e lampeggiante/sirena accesi | Entrambe non disponibili (guaste/bloccate/SPENTO) | Controlla entrambe le spie GUASTO (FAULT) e i selettori |
| Spia di livello lampeggiante | Guasto di coerenza galleggianti | Sostituisci il galleggiante sospetto |
| Niente risponde / il quadro sembra bloccato | Il controllore si auto-ripristina tramite watchdog se mai si blocca | Se persiste, spegni e riaccendi e controlla il log Seriale |

---

## 10. Manutenzione

- Verifica periodicamente che entrambe le pompe funzionino ancora (l'alternanza lo fa per te, ma controlla ogni tanto in MANUALE).
- Mantieni i galleggianti e il pozzetto liberi da detriti.
- Ricontrolla la calibrazione della corrente dopo ogni cambio di pompa o pinza.
- Le tempistiche di default (ritentativo 10 min, anti-ciclaggio breve 15 s, ecc.) sono adatte alla maggior parte delle
  installazioni; un tecnico può metterle a punto in `pompe.h` (vedi
  [README.md](README.md)) e deve poi rieseguire la suite di test.
