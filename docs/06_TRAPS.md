# 06 — Trampas conocidas

> **Leé esto antes de debuggear algo raro.** Cada entrada costó entre horas y días.
> Están agrupadas por área. Si tu síntoma se parece a alguno, empezá acá.

---

## A. SDK y vtables

### A1. Los headers de `src/SDK/L4D2/` tienen defectos. Tres confirmados.
- `IGameEventListener2` no declaraba `GetEventDebugID()` (slot 2). El motor la llama en **todo**
  listener registrado, antes de `FireGameEvent`, y compara el resultado contra `42`. Una clase
  derivada de la versión de 2 slots produce una vtable de 2 entradas; el motor lee más allá del
  final y llama a basura → **access violation de EXECUTE en 0x0 al entrar a una partida**. Se
  disparaba aunque la feature estuviera deshabilitada, porque el crash ocurre en el **despacho**
  del evento, antes de que corra el chequeo de "habilitado" del listener.
- `IGameEvent::GetString` estaba en el slot 8; el real es **9**. `GetInt`/`GetBool` siempre
  funcionaron, así que el desfase tenía que estar entre medio. Verdad de terreno desde `client.dll`:
  `[ev+20]`=GetBool(5), `[ev+24]`=GetInt(6), `[ev+36]`=GetString(**9**), `[ev+4]`=GetName(1).
  Se insertó `GetUint64` en el slot 7 para corregir.
- `iclientmode.h` está **6 slots corto**, pero los 6 no están todos arriba: `OverrideView` va en
  **19** (el header lo contaría en 16, o sea 3 slots de más hasta ahí) y `CreateMove` en **27**
  (header 21, 3 slots de más entre medio). Verificado 13/08 contra la vtable real de
  `FullscreenTerrorClientMode` (0x105BCDD4) en IDA.

`iinput.h`, en cambio, se verificó correcto (`CAM_ToThirdPerson` = 32).

### A2. Triaje de crash sin debugger
No hay WinDbg/cdb instalado. Un `.mdmp` se puede parsear a mano con Python + `struct`
(MINIDUMP_HEADER → directorio de streams → ExceptionStream / ModuleListStream / ThreadListStream,
todas estructuras estándar de `dbghelp.h`). **El `ThreadContext` incrustado en el exception stream
es el estado de registros autoritativo**, no la entrada genérica del `ThreadListStream`.
La dirección de retorno en la pila dice qué módulo llamó — eso solo ya apunta al culpable.

### A3. Llamar `SetupBones` por su dirección directa crashea en jugadores
`GameFn::SetupBones` (client.dll `0x3C380`) es `C_BaseAnimating::SetupBones`, la implementación
**base**. `C_TerrorPlayer` / `C_BaseAnimatingOverlay` la **overriden** en su vtable. Saltarse el
vtable y llamar a `0x3C380` directo (`GameFn::SetupBonesDirect`) ejecuta la versión base contra un
objeto derivado → **crash de Rage en cuanto apunta**. Síntoma: Rage andaba, el trabajo de Backtrack
cambió `GetHitboxPositions()` (virtual, curtime) por `GetHitboxPositionsAt()` (directo, con tiempo
explícito) y Rage empezó a crashear; el CrashHandler **no** deja `.mdmp` (el fallo puede no llegar
al SEH). **Solución:** `SetupBones(BoneMatrix, n, mask, flTime)` **virtual** (despacha al override
correcto — la misma vía que el ESP ya usa in-game), conservando el tiempo explícito para la pose
server-truth. No usar `SetupBonesDirect` sobre entidades jugador.

---

## B. Hooks y firmas

### B1. Escanear una función que también hookeás → falla garantizada
Ver R7 en `docs/05_RULES.md`. Síntoma: la firma resuelve `OK` al arrancar y `FB-REJECT` + `FAIL`
más tarde **en la misma sesión y en la misma RVA**.

### B2. Un hook sobre `nullptr` era un no-op silencioso
MinHook rechaza direcciones nulas sin decir nada, y la feature quedaba muerta sin señal.
`CHook::Create` ahora saltea explícitamente, y `Memory::ResolveFn` ya dejó en el log el motivo.

### B3. Hookear un constructor puede disparar demasiado tarde
Se intentó hookear el constructor de `CHudChat` para obtener el puntero. Corre después de lo que se
necesitaba. Solución: buscar `gHUD` directamente en `client.dll + 0x757740`.

---

## C. Red y exploits

### C1. `GetClientState()` ya suma +8
`engine.dll+0x82CA0` es literalmente `return dword_104268EC + 8;`. Cualquier offset publicado
relativo al `client_state` **crudo** hay que bajarlo 8 antes de usarlo con este accesor.
`choked_commands` = `client_state + 19024` = **`GetClientState() + 19016`**.
Escribir `+19024` sobre el resultado del accesor apunta 8 bytes más allá. Este bug se introdujo,
se "arregló" mal, y se volvió a arreglar bien. Triple confirmación independiente de que es 19016.

### C2. Tres gates, no uno
Que una llamada extra a `CL_Move` transmita de verdad requiere forzar **los tres**. Cada uno tiene
un síntoma distinto y arreglar solo uno produce un falso "casi funciona":

| Gate | Síntoma si falta |
|---|---|
| `m_fClearTime` (netchannel+0xB8) | ninguna llamada extra llega al servidor |
| `clientstate + 112` | retransmite el **mismo** comando; `curtime` plano |
| `bFinalTick` (forzar a 1) | ráfaga entera en `SetChoked()`: outseq sube, `curtime` congelado |

### C3. `CL_RunPrediction` es obligatorio después de cada llamada extra
Sin eso, los comandos extra llegan al servidor (el movimiento funciona) pero el estado de disparo
predicho nunca se re-simula: los tiros extra **parecen** salir localmente y no registran.

### C4. Restaurar ack/choked incondicionalmente rompe RapidFire con el tiempo
Si `m_nOutSequenceNrAck` avanzó legítimamente **durante** el `CALL_ORIGINAL` (llegó un ack real),
restaurar el snapshot previo lo revierte, y el ack real nunca puede avanzar mientras se sostenga la
tecla. Se acumula: rompe "después de un rato", no de entrada.
**Solo restaurar si el valor sigue siendo el que forzamos** (`== -1` / `== 255`).

### C5. Dueño único del spoof de netchannel
Cuando Bleed(raw), Bleed(Improved) y Choke tenían **cada uno** su propio snapshot de "último ack
genuino", el primero que corría envenenaba a los demás: veían el `-1` que el vecino acababa de
escribir, su guarda `ack != -1` nunca pasaba, y su snapshot se congelaba. El daño aparecía al
apagar: se escribía un ack viejo, tirando el número de secuencia ~324 paquetes hacia atrás.
**Una captura, un spoof, un restore** — tomado *antes* de que ninguna feature escriba, y liberado
solo cuando **todas** están apagadas.

### C6. Orden: el bloque Bleed/Choke va ANTES del `CALL_ORIGINAL` real
Si va después, el paquete real del frame nunca ve el valor inflado (ya se envió con estado sobrante
del frame anterior), y lo primero que corre después — el loop de llamadas extra — vuelve a poner el
campo en cero. Resultado: el agrupamiento de Bleed/Choke quedaba **completamente anulado** cada
frame en cuanto SpeedHack/RapidFire estaban activos. Explicaba exacto el reporte de "no multiplica".

### C7. El bucket de RapidFire necesita el campo en CERO
`clientstate+19016` inflado hace que el acumulador propio de RapidFire se rompa —
"curtime se congela sólido". Su bucket mantiene el reset incondicional a 0; los demás buckets
reciben el valor inflado de Bleed/Choke.
**Tradeoff conocido y aceptado por el usuario:** eso produce ticks duplicados también en los buckets
de SpeedHack/CustomSpeedHack, o sea que los contadores de paquetes/pps se ven más grandes de lo que
son en ticks reales. Está así a pedido explícito.

### C8. `SetChoked()` no lee ack ni choked
Es solo `++m_nOutSequenceNr; ++m_nChokedPackets;`. Y `CanPacket()` es solo
`net_time > m_fClearTime`. Meses de teorías sobre ack/choked murieron con esas dos líneas.

### C9. El buffer de historial de comandos se da vuelta
Una vez que el backlog de comandos transmitidos-pero-sin-ack llena el buffer de capacidad fija,
`CL_RunPrediction` resincroniza el tickbase predicho bruscamente **hacia atrás** (~150 ticks / 5 s) y
toda llamada extra posterior re-simula estado viejo. Se detecta como un salto hacia atrás de
`curtime` mayor a 1 s y se corta la ráfaga por ese frame. No hace falta un tope fijo de llamadas.

Es el síntoma de "mandando basura": los comandos extra por encima de lo que el server ackea no
aportan nada y solo cuestan FPS local (re-simulación + el stall del resync). Desde 2026-08-13 se
expone en vivo en el overlay de Network Info (`Exploit: extra=N wraps=N/s BASURA/limpio`): si
`wraps/s > 0` estás tirando basura — bajá SpeedHack/Choke hasta que vuelva a 0 y quedás en el
máximo sostenible.

### C10. `numbackup` está hardcodeado en 2, pero es un PISO
`backup_commands ≤ 7` con `max(2, extra)`. El 2 no es una constante, es el mínimo.
Tope del protocolo: **22 comandos por paquete**.

---

## D. UI (ImGui)

### D1. Nunca partas `OpenPopup`/`BeginPopup` a través de un `BeginTable`
`ImGui::BeginTable` hace `PushOverrideID(id)`. Un `OpenPopup("##x")` **adentro** de la tabla hashea a
un ID distinto que un `BeginPopupModal("##x")` **después** de `EndTable()` (medido: `0x2B066152` vs
`0xEA5C1E22`). El modal simplemente nunca se abre, sin error.
Se probó que es la tabla específicamente: un modal dentro de un child (sin tabla) sí funciona.
**Patrón correcto:** el botón solo setea `bWantDelete`; el `OpenPopup` corre fuera de la tabla, al
mismo nivel que el modal.
Generalizá a cualquier frontera de `PushID`.

### D2. Detectar "ancho completo" con `GetWindowContentRegionMax()` falla dentro de tablas
Adentro de una celda, eso devuelve el borde de la **ventana**, no el de la celda. Un slider de ancho
completo no se detectaba como tal y el marcador `(?)` de ayuda se dibujaba **fuera** de la celda.
Usá `ImGui::GetCurrentWindow()->WorkRect.Max.x` (consciente de celdas, y correcto también fuera de
tablas).
**Y ojo con el assert:** `IsItemVisible()` daba `true` igual porque una tira de ~1 px se solapaba con
el clip rect — un assert ingenuo de visibilidad da falso positivo. Asserteá
`GetItemRectMax().x <= WorkRect.Max.x`.

### D3. La opacidad de la ventana se guarda en `kWindowBg.w`
Un `Mix()` hacia `kWindowBg` interpola **también el alfa**. Por eso la etiqueta de la pestaña activa
se volvía translúcida al bajar la opacidad. Mezclá contra una copia solo-RGB.

### D4. Nombres de pestaña duplicados → conflicto de ID
`PillButton` hace `ImGui::PushID(label)`. Desde que los scripts pueden crear pestañas, un script
llamado "HUD" choca con la pestaña HUD del cheat. Solución: `ImGui::PushID(i)` (el índice) en el
loop de pestañas — único por construcción diga lo que diga el script.

### D5. El cursor quedaba forzado visible
El menú se cierra por **tres** caminos (INSERT, la X del header, el botón Unload) y solo INSERT
restauraba el cursor. Se sincroniza contra el estado del frame anterior en `Run()` para cubrir los
tres sin acordarse en cada punto de cierre.

---

## E. Texto y strings

### E1. UTF-8 se rompe en el camino de dibujo VGUI (no en ImGui)
`CDraw::String`, sobrecarga de `char*`, hace `wsprintfW(wstr, L"%hs", cbuffer)`. `%hs` es ANSI,
**un byte por carácter**. Los archivos fuente y los `.lua` son UTF-8, donde `·` es `C2 B7` y `ñ` es
`C3 B1` → salen como `Â·` y `Ã±`. **Es un bug de decodificación, no un glifo faltante.**
Solución: `Utils::ConvertUtf8ToWide()` y llamar a la sobrecarga de `wchar_t`.
**ImGui NO está afectado**: toma UTF-8 nativo y el atlas cubre `0x20-0xFF`. Las etiquetas del menú
pueden usar español real; solo el camino VGUI/`H::Draw` necesita conversión.

### E2. Nunca hagas `strstr` sensible a mayúsculas sobre una ruta de modelo de Source
Las rutas vienen en **mayúsculas mezcladas**: `models/w_models/Weapons/w_laser_sights.mdl`.
Un `strstr(szModel, "weapons")` nunca matchea. Pasá la ruta a minúsculas a un buffer local primero.

### E3. `H::Draw` no tiene función de medir texto
Para centrar un bloque usá `I::MatSystemSurface->GetTextSize(font.m_dwFont, wstr, w, h)`.

---

## F. Entidades, netvars y datos del juego

### F1. `GetClassId()` funde "sin clase" con 0, y **0 es el Boomer**
Leé `GetClientClass()` directamente cuando importe distinguir.

### F2. Las clases base nunca llegan como class id
`CTerrorWeapon (235)` y `CBaseCombatWeapon (11)` son clases base: el juego siempre manda la hoja
concreta. Filtrar por ellas no matchea **nada**. Por eso los botiquines/píldoras/granadas
funcionaban (tienen id propio) y ninguna arma lo hacía.
**Generalizá: nunca asumas que una entrada base de `tf_shareddefs.h` va a matchear.**

### F3. El dueño de un arma NO es `m_hOwnerEntity`
Source guarda el dueño de un arma en `CBaseCombatWeapon::m_hOwner` (+2392), un netvar **distinto**.
Y en L4D2 esto **no es solo armas**: botiquines, píldoras, adrenalina, molotovs y bilis derivan de
`CBaseCombatWeapon` porque se "equipan" como armas. Filtrar solo por `m_hOwnerEntity` deja a cada
superviviente arrastrando la etiqueta de su botiquín pegada al cuerpo. Chequeá **ambos** handles.

### F4. `m_iHealth` no es vida cuando el jugador está incapacitado
Es el contador de desangrado. Salta a ~294 y baja. Ver `docs/04_OFFSETS.md §4.9`.
El ESP fuerza `hp=1, total=1` mientras `m_isIncapacitated()`.

### F5. El cliente no calcula daño
No sirve hookear buscando el daño. Hay que leer `weaponinfo + 3144` / `+2512`.

### F6. Cuidado con los mapas de prueba
En el mapa de prueba que se usó, la "spitter" y la "witch" **no son infectados**: son
`CDynamicProp` (52) y `CBaseCombatCharacter` (10) usando modelos de `models/infected/*.mdl`.
Un reporte de "los 2 tipos de witch no coinciden" nunca fue un bug de chams.

---

## G. Materiales, render y device

### G1. Falta el hook de `IDirect3DDevice9::Reset` → freeze con ReShade
Los buffers y el atlas de ImGui son `D3DPOOL_DEFAULT`. Sin liberarlos en `Reset`, **todo** reset
posterior del device falla, y falla para siempre. Ese era el freeze con ReShade.
Lo mismo aplica al `Shutdown`: si se descarga el DLL sin destruir el contexto ImGui, esos objetos
quedan vivos sin nadie que pueda liberarlos nunca.

### G2. `$ignorez` es un FLAG de material, no una var
`FindVar` devuelve `found == false` y la escritura no hace nada, **en silencio**. Se "arregló" dos
veces con `FindVar` antes de descubrirlo.

### G3. Las dos pipelines de dibujo no se mezclan
La surface de VGUI (`StartDrawing`/`FinishDrawing`, hook de `Paint`) **no puede** hospedar ventanas
ImGui. El drawlist de ImGui solo existe dentro del ciclo `NewFrame`/`Render` de `EndScene`.
Elegir la equivocada es un error recurrente. Regla práctica: si necesita ser movible o
redimensionable, es ImGui.

### G4. L4D2 pisa el estado de stencil
Hay que forzarlo **cada frame** para chams/glow que dependan de stencil.

---

## H. Configuración

### H1. `Config::Register` guarda el puntero al nombre, no una copia
Usá **siempre** literales. Un intento de construir claves en runtime dentro de un
`std::vector<std::string>` quedó colgando: claves de 15 caracteres entran en el buffer SSO de MSVC,
o sea que viven *dentro* del objeto string, y el `c_str()` registrado muere en cuanto el vector
realoca.

### H2. Registrar después de cargar = la config no se aplica, sin error
`RegisterAll()` va **antes** de `Config::Load`. Ver `docs/02_ARCHITECTURE.md §2.5`.

### H3. Renombrar una clave huerfaniza el valor guardado en silencio
Las claves de `ConfigRegistry.cpp` son formato en disco. Tratalas como estables.

### H4. Un `.cpp` que no está en el `.vcxproj` no se compila y no da error
Ver R8.

---

## I. Metodología

### I1. Un listen server no valida nada de red
Ver R4 en `docs/05_RULES.md`. Es la trampa metodológica más cara del proyecto.

### I2. Un log mal afinado produce conclusiones confiadas y falsas
Ver R5. En particular: verificá que la captura **contenga el sujeto** antes de sacar conclusiones.

### I3. El total de warnings no es señal de regresión
Ver `docs/01_BUILD.md`. Escala con la cantidad de `.cpp`.

### I4. Los logs por frame inflan el archivo de debug a más de 1 MB
Hubo bloques de `Debug::Log` que corrían **una vez por llamada extra** (no por frame). Se eliminaron.
Si agregás instrumentación temporal, ponele un gate y sacala cuando la investigación cierre.
