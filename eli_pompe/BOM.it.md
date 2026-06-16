> 🇮🇹 **Traduzione di cortesia — NON autoritativa.** L'originale in inglese ([BOM.md](BOM.md)) fa fede: in caso di dubbio o discordanza vale quello, e questa copia potrebbe non essere aggiornata.

# Distinta base (BOM) — stazione pompe di sollevamento

Compila gli spazi vuoti con i componenti **effettivamente** installati (numeri di
modello, valori nominali, impostazioni). Chi subentra per sostituire un componente
deve poterne acquistare/configurare uno identico. **Tienila salvata nel repo e
stampata nel cassetto.**

> ⚠️ Alcune impostazioni **devono corrispondere al firmware** (`eli_pompe/pompe.h`).
> Quelle righe sono contrassegnate con 🔧 — se sbagliate, il controllore legge male
> o si comporta in modo scorretto.

## Controllore e alimentazione

| Voce | Specifica / modello (DA COMPILARE) | Q.tà | Note |
|------|------------------------------------|------|------|
| PLC Controllino | **CONTROLLINO MAXI** (___ codice articolo) | 1 | Versione 24 V. Firmware: `eli_pompe`. |
| Alimentatore 24 V DC | ___ V/A, modello ___ | 1 | Dimensionato per scheda + tutte le spie 24 V + bobine relè + lampeggiante/sirena. |
| Fusibile / interruttore magnetotermico principale (controllo 24 V) | ___ A | 1 | |
| Quadro/contenitore | ___ | 1 | |

## Pompe e commutazione (lato rete — elettricista)

| Voce | Specifica / modello (DA COMPILARE) | Q.tà | Note |
|------|------------------------------------|------|------|
| Pompe di sollevamento | ___ kW, ___ A nominali | 2 | La corrente di esercizio definisce `NORMAL_AMP_MIN/MAX` 🔧 |
| Relè di commutazione pompe | **Finder**, montati su zoccolo, modello ___ , bobina ___ V, contatti ___ A | 2 | A innesto (sostituibili senza ricablare). Bobine pilotate da `R0` / `R1`. Dimensiona i contatti per la corrente di esercizio della pompa **+ spunto**. |
| Zoccoli relè + staffe di ritegno | Finder ___ | 2 | Così un relè guasto si estrae/inserisce senza ricablare. |
| Interblocco (opzionale) | elettrico tramite contatto ausiliario NC, oppure assente | 0–1 | **Non necessario** — il software garantisce il funzionamento di una sola pompa (MANUAL §5.1). Entrambe attive spreca solo portata, nessun danno. Le barre di interblocco meccanico non si adattano ai relè a innesto. |
| Relè termico / protezione motore | ___ A per pompa | 2 | |
| Fusibili/interruttori magnetotermici circuito pompe | ___ A | 2 | |

## Rilevamento

| Voce | Specifica / modello (DA COMPILARE) | Q.tà | Note |
|------|------------------------------------|------|------|
| Galleggianti | ___ , **normalmente aperti** (chiudono in salita) | 2–3 | MIN + 1/2 ora; 3/4 opzionale. Tipo NO ⇒ `FLOAT_ACTIVE_HIGH = true` 🔧 |
| Sensori di corrente | **YHDC SCT010T-D** (a nucleo apribile, 10 A → 0–10 V, ±2%, alimentato) | 2 | Uscita 0–10 V → direttamente agli ingressi analogici `A0` / `A1`. **Nessuna resistenza di shunt (burden).** Verifica la tensione di alimentazione (12 V / 24 V). |
| → fondo scala del sensore | **10 A** (SCT010T-D) | — | Scegli in modo che i ~4 A della pompa siano ~30–50% della scala. **Deve essere uguale ad `AMP_SPAN_A` (10.0)** nel firmware 🔧 |

## Segnalazione e allarme

La MARCIA (LED dei relè pompa), il livello dell'acqua (LED degli ingressi
galleggiante) e lo stato di allarme (LED del relè lampeggiante) si leggono dai
**LED di canale integrati** del controllore — nessuna spia per questi. Sono
cablate solo le spie qui sotto.

| Voce | Specifica / modello (DA COMPILARE) | Q.tà | Note |
|------|------------------------------------|------|------|
| Spie livello (MIN/1-2/3-4) | 24 V, ___ | 3 | `D0` / `D1` / `D2` (ripetono il livello per una lettura a colpo d'occhio; lampeggio = galleggiante guasto) |
| Spie GUASTO pompa | 24 V, ___ (rosso?) | 2 | `D3` / `D4` (fisso = guasto, lampeggio = bloccata) |
| Spia PRE-SVUOTAMENTO | 24 V, ___ | 1 | `D5`, lampeggia durante un pre-svuotamento / per ~5 s dopo la pressione |
| Lampeggiante remoto | 24 V, **autolampeggiante** | 1 | `R2`. Autolampeggiante affinché il relè resti fisso (MANUAL §5.4). |
| Sirena / avvisatore acustico | 24 V, ___ | 1 | `R3` (relè separato così SILENCE la silenzia). |

## Comandi

| Voce | Specifica / modello (DA COMPILARE) | Q.tà | Note |
|------|------------------------------------|------|------|
| Selettore MOA | 3 posizioni **stabili** | 2 | Manuale / Off / Auto, cablato come 2 ingressi/pompa (A6+A7, A8+A9). |
| Pulsanti | momentanei, **normalmente aperti** | 3 | SILENCE (`A3`), RESET (`A4`), PRE-SVUOTAMENTO (`A5`). |
| Morsettiere, cablaggio, ferrule | — | — | |

---

## Corrispondenza firmware ↔ hardware (righe 🔧 sopra)

Se una di queste cambia, aggiorna la costante corrispondente in `eli_pompe/pompe.h`
e ri-esegui i test (`cd eli_pompe/tests && make`):

| Hardware | Costante firmware | Valore attuale |
|----------|-------------------|----------------|
| Fondo scala del sensore | `AMP_SPAN_A` | 10.0 A (SCT010T-D) |
| Corrente di esercizio normale pompa | `NORMAL_AMP_MIN` / `NORMAL_AMP_MAX` | 2.0 / 8.0 A |
| Zero/fondo scala ingresso 0–10 V (ADC grezzo) | `ADC_AT_0A` / `ADC_AT_FS` | **tarare in loco, poi salvare nel repo** (~0 / ~426) |
| Tipo contatto galleggiante (NO/NC) | `FLOAT_ACTIVE_HIGH` | `true` (NO) |

Le assegnazioni dei pin sono in `eli_pompe/eli_pompe.ino`; il dettaglio del
cablaggio in [MANUAL.it.md](MANUAL.it.md) §4–§5.

---

## Ricambi — il sacchetto "comprane due"

Compra un secondo esemplare di tutto e tienilo in un sacchetto etichettato
**accanto al quadro**. In caso di guasto, sostituisci invece di diagnosticare —
molto più veloce e richiede molta meno competenza. Le quantità qui sotto sono
*ricambi da tenere a scorta* (oltre a ciò che è installato).

| Ricambio | Q.tà | Difficoltà sostituzione | Note |
|----------|------|-------------------------|------|
| **CONTROLLINO MAXI — GIÀ PROGRAMMATO** | 1 | media (ricollegare i fili) | **Questo è quello importante.** Tienilo già programmato con questo firmware + la taratura salvata, così la sostituzione non richiede **né laptop né competenze Arduino** — basta spostare i fili. Vedi sotto. |
| Relè pompa Finder | 2 | **facile** (a innesto) | Su zoccolo → estrai/reinserisci, nessun ricablaggio. |
| Galleggiante | 2–3 | facile | Pochi morsetti. |
| Sensore di corrente YHDC SCT010T-D | 2 | facile | Stesso modello; uscita 0–10 V direttamente all'ingresso analogico (senza resistenza di shunt (burden)). |
| Spia / lampeggiante / sirena | alcuni | facile | |
| Selettore MOA, pulsante | 1–2 | facile | |
| Fusibili | diversi | facile | |

### Rendi il Controllino di ricambio un vero ricambio a innesto
Un Controllino vuoto in un sacchetto è inutile per chi non programma. Per rendere
la sostituzione priva di competenze richieste:

1. **Programma ora il ricambio** con questo firmware (vedi [RECOVERY.it.md](RECOVERY.it.md)),
   inclusi i valori **tarati** di `ADC_AT_0A`/`ADC_AT_FS`, ed etichettalo
   *"eli_pompe — programmato il <data>"*.
2. **Etichetta ogni filo** ai morsetti del Controllino (ferrule/targhette) così che
   ricollegarli sul ricambio sia un'operazione meccanica — fai corrispondere lo
   schema elettrico stampato.
3. A quel punto un guasto diventa: togli alimentazione → sposta i fili etichettati
   sul ricambio → ridai alimentazione → esegui le verifiche rapide in
   [MANUAL.it.md](MANUAL.it.md) §7. Nessun PC necessario.

Quando usi il ricambio, **comprane e programmane un altro** così il sacchetto non
è mai vuoto. `RECOVERY.md` è il piano di riserva per quando non esiste un ricambio
già programmato.
