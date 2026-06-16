> 🇮🇹 **Traduzione di cortesia — NON autoritativa.** L'originale in inglese ([MANUAL.md](MANUAL.md)) fa fede: in caso di dubbio o discordanza vale quello, e questa copia potrebbe non essere aggiornata.

# Controllore stazione di pompaggio di aggottamento — Manuale di installazione e uso

Questo quadro gestisce automaticamente **due pompe di aggottamento che condividono un unico tubo di mandata**.
Fa funzionare una sola pompa alla volta, le alterna in modo che si usurino in modo uniforme, sorveglia
la corrente elettrica di ciascuna pompa per individuare una pompa bloccata o guasta, passa
automaticamente alla pompa funzionante e attiva un allarme locale quando qualcosa richiede
attenzione. È un controllore **autonomo** (senza rete/Wi-Fi).

> ⚠️ **Sicurezza.** I motori delle pompe funzionano a tensione di rete. Tutto il cablaggio di rete, i relè
> di commutazione e la protezione motore devono essere installati da un elettricista qualificato e
> conformi alle normative locali. Il controllore pilota **solo bobine di relè e circuiti di
> segnale** — non commuta direttamente la corrente del motore. Sezionare sempre
> l'alimentazione prima di cablare.

---

## 1. Come funziona (in parole semplici)

Tre interruttori a galleggiante si trovano a tre altezze nel pozzetto:

- **MIN** (il più basso) — "abbastanza vuoto, ferma la pompa."
- **1/2** (intermedio) — "l'acqua è salita, avvia una pompa."
- **3/4** (il più alto) — "ALLARME acqua alta." *(Opzionale — montalo quando sei pronto; il controllore lo supporta già.)*

Ciclo normale: l'acqua sale fino al galleggiante **1/2** → una pompa parte → l'acqua scende fino al
galleggiante **MIN** → la pompa si ferma. La volta successiva funziona l'*altra* pompa (si
alternano).

Un **sensore di corrente** (YHDC SCT010T-D) su ciascuna pompa indica al controllore quanti
ampere assorbe la pompa. Se una pompa in funzione assorbe troppo poco (non sta effettivamente
pompando, a secco o intervenuta una protezione) o troppo (inceppata), il controllore dichiara quella pompa
**guasta**, passa all'altra pompa e ritenta quella guasta più tardi. Se una pompa
continua a guastarsi viene **bloccata** finché qualcuno non interviene e non preme RIPRISTINO (RESET).

Se invece una pompa assorbe corrente **normale** ma l'acqua continua a non
scendere (il galleggiante 1/2 non si libera), la pompa è a posto — sta solo
**perdendo contro l'afflusso** (per esempio pioggia intensa) o è leggermente
ostruita. Il controllore **non** la ferma (fermare una pompa funzionante durante
un temporale sarebbe la cosa peggiore); la mantiene in marcia e accende il
**lampeggiante come avviso** — non la sirena. Se l'acqua continua a salire fino
al galleggiante 3/4, quella di per sé diventa una vera emergenza.

---

## 2. Cosa c'è sul quadro

**Spie integrate sul controllore** — il CONTROLLINO ha un LED di stato per
ogni ingresso e uscita. Mostrano direttamente il funzionamento normale (senza spie aggiuntive):
- ogni LED del **relè pompa** = quella pompa è **in marcia**;
- i tre LED degli **ingressi galleggiante** = il **livello dell'acqua** (e se uno più alto è acceso mentre uno più basso è spento, quel galleggiante è guasto);
- il LED del **relè lampeggiante** = lo stato di **allarme**.

**Spie di quadro (cablate alle uscite)** — solo gli stati che non sono ovvi dai LED integrati:

| Spia | Significato |
|------|-------------|
| Pompa 1 / Pompa 2 **GUASTO** | **fissa** = guasto temporaneo, ritenterà automaticamente · **lampeggiante** = bloccata, richiede RIPRISTINO (RESET) |
| Livello **MIN / 1-2 / 3-4** | ripetizione del livello dell'acqua per una lettura a colpo d'occhio · una spia di livello **lampeggiante** = quel galleggiante sembra guasto |
| **PRE-SVUOTAMENTO (PRE-EMPTY)** | lampeggia mentre è in corso uno svuotamento manuale di pre-svuotamento, e per ~5 s dopo che hai premuto il pulsante (per confermare la pressione) |

**Segnali remoti (le uniche uscite esterne)**

- **Grande lampeggiante rosso** — si accende ogni volta che c'è *un qualsiasi* problema (guasto di una pompa, guasto di un galleggiante o un'emergenza). "Vieni a controllare."
- **Sirena** — suona solo in una vera **emergenza** (acqua alta, o entrambe le pompe non disponibili). "Agisci subito."

**Comandi**

- Selettore **Manuale–Spento–Auto** per pompa (vedi §6).
- Pulsante **SILENZIA (SILENCE)** — silenzia la **sirena**; il lampeggiante resta acceso.
- Pulsante **RIPRISTINO (RESET)** — azzera i guasti e riabilita una pompa bloccata (dopo aver risolto la causa).
- Pulsante **PRE-SVUOTAMENTO (PRE-EMPTY)** — svuota subito la vasca per fare spazio prima di un temporale (vedi §6).

---

## 3. Cosa serve

- **CONTROLLINO MAXI**, alimentato a 24 V DC.
- Due relè di commutazione pompa — **relè Finder montati su zoccolo** (a innesto, così uno guasto si sostituisce senza ricablare), dimensionati per la corrente della pompa. Un interblocco hardware tra i due è **opzionale** (vedi §5.1).
- 3 interruttori a galleggiante (normalmente aperti, che chiudono al salire dell'acqua). Il galleggiante 3/4 è per ora opzionale.
- 2 × sensori di corrente a nucleo apribile (a pinza) **YHDC SCT010T-D** (uscita 0-10 V — 10 A → 10 V, alimentati a 24 V), uno applicato a pinza sull'alimentazione di ciascuna pompa. **Senza resistenze di shunt (burden)** — l'uscita 0-10 V va direttamente a un ingresso analogico. (Scegli il fondo scala in base alla pompa; vedi §5.3.)
- 6 × spie da 24 V (3 di livello, 2 di guasto pompa, 1 di pre-svuotamento), un lampeggiante da 24 V (consigliato di tipo autolampeggiante) e una sirena. (La marcia e il livello sono mostrati anche dai LED integrati del controllore.)
- Selettori MOA (Manuale, Spento, Auto) e tre pulsanti momentanei (Silenzia, Ripristino, Pre-svuotamento).

---

## 4. Assegnazione morsetti / pin

Cabla ai morsetti del CONTROLLINO MAXI come indicato sotto (corrisponde alla mappa dei pin del firmware).

**Uscite relè**

| Morsetto | Collegare a |
|----------|-------------|
| `R0` | Bobina relè di commutazione Pompa 1 (Finder) |
| `R1` | Bobina relè di commutazione Pompa 2 (Finder) |
| `R2` | Lampeggiante (grande spia rossa lampeggiante) |
| `R3` | Sirena |

**Uscite digitali (spie da 24 V)** — la marcia e lo stato di allarme sono sui LED dei relè integrati del controllore, quindi nessuna spia per questi.

| Morsetto | Spia |
|----------|------|
| `D0` / `D1` / `D2` | Livello MIN / 1-2 / 3-4 |
| `D3` / `D4` | GUASTO Pompa 1 / Pompa 2 |
| `D5` | PRE-SVUOTAMENTO attivo (lampeggiante) |

**Ingressi**

| Morsetto | Ingresso |
|----------|----------|
| `A0` / `A1` | Sensore di corrente Pompa 1 / Pompa 2 (YHDC SCT010T-D, **0-10 V — senza resistenza di shunt (burden)**) |
| `A2` / `A3` / `A4` | Galleggiante MIN / 1-2 / 3-4 |
| `A5` | Pulsante Silenzia |
| `A6` | Pulsante Ripristino |
| `A7` | Pulsante Pre-svuotamento |
| `A8` / `A9` | MANUALE / AUTO Pompa 1 (dal suo selettore MOA) |
| `IN0` / `IN1` | MANUALE / AUTO Pompa 2 (dal suo selettore MOA) |

Tutti i 12 ingressi (A0–A9 + IN0/IN1) sono utilizzati — non ci sono ingressi liberi.

---

## 5. Note di cablaggio

### 5.1 Relè di commutazione pompa — l'interblocco è opzionale
Solo una pompa dovrebbe funzionare alla volta (condividono un unico tubo di mandata). **Il
firmware lo garantisce via software**: comanda sempre e solo una pompa accesa, e
la suite di test verifica che le due uscite non siano mai eccitate insieme. Far funzionare entrambe
contemporaneamente le fa solo contendere il tubo e sposta meno acqua — non
danneggia nulla — quindi un interblocco hardware è una **precauzione opzionale (belt-and-suspenders)**,
non obbligatorio.

Questa realizzazione usa **relè Finder montati su zoccolo** su `R0` / `R1`, scelti così che un
relè guasto si stacca e si reinnesta senza ricablare. Dimensiona ciascun relè per la corrente di
funzionamento **e di spunto** della pompa (la corrente di avviamento di un motore è varie volte la sua
corrente di funzionamento).

Se in futuro vuoi la garanzia in più, l'opzione più economica è un **interblocco
elettrico**: cabla il contatto ausiliario/di scorta NC di ciascun relè nell'alimentazione della bobina
dell'altro, così che l'eccitazione di uno disecciti l'altro. (Esiste una barra di interblocco meccanico per
i dispositivi tipo contattore, ma non è applicabile ai relè a innesto.)

### 5.2 Galleggianti
Usa galleggianti normalmente aperti che **chiudono verso +24 V al salire dell'acqua** (active-high).
Porta i galleggianti MIN, 1/2 e (opzionale) 3/4 ad `A0`, `A1`, `A2`. L'ingresso 3/4 può
restare scollegato per ora — legge "asciutto" e non causerà un falso allarme.
(Se hai solo galleggianti normalmente chiusi, imposta `FLOAT_ACTIVE_HIGH` a `false` nel
firmware.)

### 5.3 Sensori di corrente (YHDC SCT010T-D)
Ogni sensore è un trasduttore a nucleo apribile (a pinza) che fornisce un'uscita **0-10 V**
proporzionale alla corrente RMS della sua pompa — **10 A = 10 V** sull'SCT010T-D. Applica a pinza
uno attorno al filo di fase di ciascuna pompa e porta la sua uscita 0-10 V **direttamente a un ingresso
analogico** (`A0` / `A1`) — **senza resistenza di shunt (burden)**. È un sensore alimentato: forniscigli
l'alimentazione del quadro (verifica 12 V o 24 V sull'unità). Imposta `AMP_SPAN_A` nel firmware sugli
ampere di fondo scala del sensore (10 per l'SCT010T-D); scegli il fondo scala del sensore in modo che la
corrente di funzionamento della pompa si attesti intorno al 30-50% della scala (un sensore da 10 A si adatta bene a una
pompa da ~4 A). Nota: un sensore 0-10 V non ha "zero vivo", quindi un sensore scollegato
legge ~0 A e la pompa va semplicemente in guasto per sottocorrente (non c'è una rilevazione
separata di filo interrotto).

Poi **calibra** (§7).

### 5.4 Lampeggiante e sirena
Metti il **lampeggiante** su `R2` e la **sirena** su `R3` come indicato, così che il pulsante SILENZIA (SILENCE)
possa silenziare la sirena mentre il lampeggiante resta acceso. Usa un lampeggiante **autolampeggiante**
(lampeggia da solo; il relè si limita ad alimentarlo). Se preferisci, puoi
cablare la sirena in parallelo al lampeggiante su un unico relè — ma in tal caso SILENZIA (SILENCE)
silenzierà entrambi.

---

## 6. Funzionamento — Manuale / Spento / Auto

Ogni pompa ha un selettore a 3 posizioni:

- **AUTO** — funzionamento automatico normale (galleggianti + monitoraggio corrente). Lascia entrambi qui per il funzionamento incustodito.
- **OFF** — quella pompa è disabilitata e non funzionerà, nemmeno in emergenza.
- **MANUALE** — forza ora la marcia di quella pompa, ignorando i galleggianti (per prove/adescamento). L'interblocco a pompa singola resta valido; se entrambe sono su MANUALE, funziona solo la Pompa 1.

### Pre-svuotamento prima di un temporale
Premi il pulsante **PRE-SVUOTAMENTO (PRE-EMPTY)** (pompe in AUTO) per svuotare la vasca fino al livello
MIN **ora**, anche se l'acqua non ha raggiunto il galleggiante di avvio 1/2 — questo
libera capacità di riserva prima di una pioggia intensa.

- La **spia PRE-SVUOTAMENTO (PRE-EMPTY) lampeggia** per confermare che la pressione è stata accettata. Se c'è
  acqua da pompare, una pompa funziona e svuota fino a MIN, poi si ferma normalmente (e il ciclo
  successivo si alterna come al solito). Se la vasca è già bassa, non pompa nulla ma la
  spia lampeggia comunque per ~5 s così sai che il pulsante ha funzionato.
- È completamente monitorato (stessa protezione guasti/allarmi di un ciclo normale) e non
  funziona mai a secco (si ferma a MIN).
- Per annullare una richiesta in sospeso, premi **RIPRISTINO (RESET)**. (Uno svuotamento già in corso
  termina fino a MIN — è innocuo ed è proprio l'obiettivo.)
- Se entrambe le pompe sono su OFF o non disponibili, non funziona nulla; la spia lampeggiante significa solo
  che la richiesta è stata registrata.

---

## 7. Checklist di messa in servizio

1. **Accendi** con entrambi i selettori su **OFF**. Verifica che nessuna pompa funzioni e che le spie siano coerenti.
2. **Calibra le pinze di corrente** (fallo una volta, per ciascuna pompa):
   - Collega un portatile alla USB del CONTROLLINO e apri il Serial Monitor di Arduino a **115200 baud**. Una riga di stato viene stampata ogni ~10 s, inclusa `A=` (gli ampere misurati delle due pompe).
   - Con la pompa **spenta**, la lettura dovrebbe attestarsi vicino a **0 A**. Con la pompa **in funzione a un carico noto**, confronta gli ampere visualizzati con una pinza amperometrica.
   - Se non corrispondono, regola `ADC_AT_0A` / `ADC_AT_FS` in `pompe.h` (questi mappano la lettura analogica grezza su 0 A / fondo scala). Il metodo più semplice: annota il conteggio analogico grezzo con la pompa spenta (0 A) e a una corrente di funzionamento nota, e inseriscili. Riflasha e ricontrolla.
   - Imposta `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` per delimitare la corrente di funzionamento reale della tua pompa (predefinito 2-8 A) con un certo margine.
3. **Imposta `DRAIN_TIMER_MS`** a circa il tempo che una pompa funzionante impiega per abbassare il livello dal galleggiante 1/2 al galleggiante MIN — viene usato come riserva se il galleggiante MIN dovesse mai guastarsi.
4. **Prova ciascuna pompa in MANUALE**, verifica che il relè corretto si ecciti (si accende il suo LED di relè integrato) e che la corrente legga in modo sensato.
5. **Prova l'AUTO**: alza i galleggianti manualmente (o riempi il pozzetto) e verifica che una pompa parta a 1/2 e si fermi a MIN, e che la **pompa di testa si alterni** a ogni ciclo.
6. **Prova l'allarme**: aziona il galleggiante 3/4 (o il suo ingresso) e verifica lampeggiante **e** sirena; premi **SILENZIA (SILENCE)** e verifica che la sirena si fermi ma il lampeggiante resti.
7. **Prova il PRE-SVUOTAMENTO (PRE-EMPTY)**: con l'acqua tra MIN e 1/2, premi il pulsante → la spia PRE-SVUOTAMENTO (PRE-EMPTY) lampeggia e una pompa svuota fino a MIN poi si ferma. Premilo di nuovo con la vasca vuota → nessuna pompa, ma la spia lampeggia comunque ~5 s.
8. Lascia entrambi i selettori su **AUTO**.

---

## 8. Cosa fare quando qualcosa segnala

| Cosa vedi | Significato | Azione |
|-----------|-------------|--------|
| **Lampeggiante acceso, sirena spenta**, una spia GUASTO fissa | Una pompa è in guasto; l'altra sta coprendo. Ritenterà automaticamente tra ~10 min. | Indaga su quella pompa quando comodo (ostruzione, valvola di ritegno, interruttore). Nessuna fretta. |
| **Lampeggiante acceso**, una spia GUASTO **lampeggiante** | Quella pompa è **bloccata** dopo guasti ripetuti. | Ripara la pompa, poi premi **RIPRISTINO (RESET)**. |
| **Lampeggiante acceso, sirena spenta**, una pompa **in marcia**, nessuna spia di guasto | La pompa non riesce a smaltire la portata in ingresso (pioggia intensa) — sta funzionando, non è in guasto. | Tieni sotto controllo. Non è un'emergenza a meno che non degeneri fino alla sirena. |
| Una **spia di livello lampeggiante** | Quel galleggiante sembra guasto (un galleggiante più alto è bagnato ma questo dice asciutto). | Controlla/sostituisci quell'interruttore a galleggiante. Nel frattempo la stazione continua a funzionare. |
| **Sirena + lampeggiante** | **Emergenza**: acqua alta (3/4) e/o entrambe le pompe non disponibili. | Intervieni **subito**. Premi SILENZIA (SILENCE) per zittire la sirena mentre lavori. Controlla pompe, alimentazione e galleggianti. |

**Procedura di RIPRISTINO (RESET):** dopo aver risolto la causa, premi **RIPRISTINO (RESET)** una volta. Questo azzera i
guasti, riabilita qualsiasi pompa bloccata e toglie il silenziamento dell'allarme. Se il
problema di fondo non è risolto, la pompa andrà semplicemente di nuovo in guasto.

---

## 9. Risoluzione dei problemi

| Sintomo | Causa probabile | Verifica |
|---------|-----------------|----------|
| La pompa va in guasto subito (sottocorrente) a ogni avvio | La pompa non assorbe corrente, o il sensore è scollegato | Interruttore/protezione termica intervenuti, motore scollegato, cablaggio/alimentazione del sensore, fondo scala del sensore |
| La pompa va in guasto per **sovracorrente** | Girante inceppata / rotore bloccato | Bloccaggio meccanico; condizione del motore |
| Lampeggiante acceso, pompa **in marcia**, acqua che resta alta (nessun guasto) | La pompa non riesce a smaltire la portata in ingresso, oppure è ostruita | Di solito è solo pioggia intensa — tieni sotto controllo. Se persiste con tempo asciutto: aspirazione/girante intasata, valvola di ritegno bloccata o mandata ostruita |
| Una **spia GUASTO lampeggia** e la pompa non riparte | Bloccata (troppi guasti) | Ripara la pompa, premi **RIPRISTINO (RESET)** |
| Lettura di corrente errata su Serial | Non calibrato | Rifai il passo 2 del §7 (`ADC_AT_0A`/`ADC_AT_FS`) |
| La pompa non parte anche se l'acqua è alta | Selettore su OFF, o attesa anti-cicli rapidi, o bloccata | Controlla il selettore MOA e le spie GUASTO |
| Entrambe le pompe ferme e lampeggiante/sirena accesi | Entrambe non disponibili (in guasto/bloccate/OFF) | Controlla entrambe le spie GUASTO e i selettori |
| Spia di livello lampeggiante | Guasto di coerenza dei galleggianti | Sostituisci il galleggiante sospetto |
| Niente risponde / il quadro sembra bloccato | Il controllore si auto-resetta tramite watchdog se mai si blocca | Se persiste, spegni e riaccendi e controlla il log su Serial |

---

## 10. Manutenzione

- Verifica periodicamente che entrambe le pompe funzionino ancora (l'alternanza lo fa per te, ma controlla in MANUALE di tanto in tanto).
- Mantieni gli interruttori a galleggiante e il pozzetto liberi da detriti.
- Ricontrolla la calibrazione della corrente dopo qualsiasi cambio di pompa o pinza.
- Le tempistiche predefinite (ritenta dopo 10 min, anti-cicli rapidi 15 s, ecc.) sono adatte alla maggior parte delle
  installazioni; un tecnico può affinarle in `pompe.h` (vedi
  [README.md](README.md)) e deve poi rieseguire la suite di test.
