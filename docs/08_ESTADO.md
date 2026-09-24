# 08 — Estado actual y pendientes

**Corte:** 2026-08-13. Último `Release/ByxStorm.dll` construido: 2026-08-12.

---

## 8.1 Qué está hecho y verificado in-game

- **Exploits de red**: SpeedHack, CustomSpeedHack, RapidFire (`CRapidFire`), RapidFireCustom,
  Bleed ×2, Choke ×2, Fakelag, SendMoveOverride, AirStuck, Roll, NoFallDamage, NameBug, ChargerTurn.
  El RapidFire fue probado y **aprobado explícitamente** por el usuario.
- **Aimbot** con sus dos modos, AutoShove, TraceFilter, WeaponState, BulletSim.
- **ESP** de jugadores + estados + armas; ItemESP y OffScreen probados in-game (dos rondas de fixes
  a partir del log `[Byx][ITEM]`).
- **Visuals**: Chams, Glow, watermark/FPS/velocímetro.
- **Effects**: NoVomit, NoSmoke, NoSpitterAcid, modo de rocas de Tank.
- **Camera**: Thirdperson (con suavizado orbital), ViewModel, FreeLook.
- **Movement**: Bunnyhop, AutoStrafe, AutoDuck.
- **MiscTools**: NetworkInfo, Watchers, DamageIndicator, EnemyChat, KillNotifier, HitSounds,
  ChatSpam, NameStealer, VirtualKeyboard, MovementRecorder, AutoClicker.
- **Scripting Lua** con LuaJIT, pestaña propia, API documentada, 7 ejemplos.
- **Sistema de configs con nombre**, guardado automático de `default` al descargar.
- **Crash handler** con volcado a `ByxStorm/crashes/`.
- **Hook de `IDirect3DDevice9::Reset`** (arregló el freeze con ReShade).

---

## 8.2 Compila limpio pero **falta verificar in-game**

> Esto viene del registro de sesiones. **Confirmalo con el usuario antes de darlo por probado** —
> puede haberse testeado después del corte de este documento.

| Qué | Estado registrado |
|---|---|
| Rediseño del menú (barra lateral con iconos vectoriales, favoritos, contador de activos por tarjeta, toasts, escala de UI, animaciones de fundido/cascada) | build limpio, harness de UI en verde, **no probado in-game** |
| Arreglo de fuentes del ESP (sin antialias + usaba la fuente ráster de Win 3.1) | idem |
| Port del aimbot desde la referencia (BulletSim, gates de `WeaponState`, AutoShove, tope de superficie) | 0 errores, **no probado in-game** |
| `CTraceFilterAim` reemplazando `CTraceFilterSkipEntity` en Aimbot/AimInfected/AutoShove | 0 errores, **no probado in-game** |
| Multi-punto + exigir-hitgroup ahora activos en modo Rage | 0 errores, **no probado in-game** |
| Reescritura de Thirdperson: hook de `IClientMode::OverrideView` (idx 19) que mueve `pSetup->origin/angles` — arregla la cámara anclada que no orbitaba | 0 errores, **no probado in-game** |
| Exploits: `tick_base--` en comandos sintéticos, guard `choked_packets > 0` en SendMove, multiplicador `RapidFire::nInterpolate` (ApplyExtraScaling), y rebobinado de return address opt-in (`SpeedHack::bRewind`) | 0 errores, **no probado in-game** (requiere servidor dedicado, R4) |
| Backtrack del aimbot (`bBacktrack`: pose del server vía `m_flSimulationTime`, no la interpolada) | 0 errores, **no probado in-game**. ⚠️ el `SetupBones` directo que introdujo crasheaba Rage (los jugadores lo overriden); corregido usando el virtual con tiempo explícito (`06_TRAPS.md` A3). |
| Fire-timing del aimbot (`bIgnoreCooldown`: salta solo el gate de cooldown del arma, que los exploits desincronizan) | 0 errores, **no probado in-game** |
| Portado de funciones del juego por firma+fallback (`GameFn` en `src/SDK/L4D2/GameFunctions.*`): `GetInterpolationTime`/`SetupBones`/`CanAttack`/`FireBullet`/`UpdateSpread`/`SharedRandomFloat`/`GetActiveWeapon`/`GetWeaponData`/`EyePosition`/`SequenceDuration`/`SelectSequence`/`PerformShoveTrace`/`GetStudioHeader`/`PredictionSeed`. Cableado: `GetInterpolationTime`→backtrack del aimbot, `SharedRandomFloat`+`PredictionSeed`→NoSpread, `SetupBones`→pose del backtrack (directo, sin vtable). Resueltas pero SIN cablear: `CanAttack`/`FireBullet`/`UpdateSpread`/etc. (falta la firma de llamada). | 0 errores, **no probado in-game** |

---

## 8.3 Features eliminadas a propósito (no las revivas sin preguntar)

| Feature | Cuándo | Nota |
|---|---|---|
| **LagExploit** | 24/07 | borrada por completo a pedido del usuario |
| **God Mode** (sobrescribir `m_iHealth`) | 06/08 | |
| **SpeedHack QPC** (escalado de reloj vía `QueryPerformanceCounter`) | 06/08 | era la variante de la pestaña Movement, distinta de la de Exploits |
| **Wallbang** | 06/08 | |
| **Spectator list** | 06/08 | |

---

## 8.4 Pendientes conocidos

### Marcados explícitamente como "más adelante"
- **Exploración del glitch de ~34k HP y del lag de servidor asociado.** El usuario dijo
  explícitamente: explorar **LUEGO, no ahora**. No lo empieces por iniciativa propia.

### Decisiones tomadas de no arreglar
- **Colisión M5** — dejado así por elección del usuario.
- **NoSpread de escopeta** — inarreglable con el enfoque actual.
- **NoSpread para armas que no sean escopeta** — las semillas nunca se encontraron.

### Bloqueos técnicos abiertos
- **Cálculo de daño**: vive en `server.dll`; llamarlo desde el cliente crashea. La alternativa ya
  identificada es **leer** `weaponinfo + 3144` / `+2512` en vez de calcular.
- **Compensación de ángulo por `Random_Type`**: crashea al llamarlo. El offset correcto es
  `client.dll + 0x1ACDB0` (el `0x1ACE30` que circulaba está 0x80 bytes corrido).
- **Backtrack (tickbase vía `Run_Command` + `Post_Network_Data_Received`)**: sigue **sin portar**. En
  su lugar se implementó `Aimbot::bBacktrack` (2026-08-13), que ataca el mismo síntoma por el lado de
  la pose: apunta contra `m_flSimulationTime` en vez de la interpolación del render (ver
  `docs/03_FEATURES.md`).
- **Fire-timing contra el snapshot del server**: el gate de cooldown del arma lee el
  `m_flNextPrimaryAttack` PREDICHO, que con los exploits queda desincronizado del reloj del server.
  No existe snapshot del server de uno mismo (el jugador local es predicción; no hay un estado de
  arma networkeado aparte), así que en su lugar se implementó `Aimbot::bIgnoreCooldown`
  (2026-08-13), que salta solo ese gate cuando el usuario lo pide. El fire-timing "de verdad"
  (leer el estado del arma desde el buffer de comandos del server) sigue **sin portar**.
- **Fórmula exacta de decaimiento de `m_healthBufferTime`**: nunca se reverseó directamente. El
  umbral de 120 s del indicador `[GOD MODE]` y el mecanismo de "timestamp en el futuro desactiva el
  decaimiento" están **inferidos del comportamiento**, no confirmados leyendo ese código. Si el
  indicador da falsos positivos/negativos, ahí es donde hay que mirar.

### Mejoras identificadas y no portadas
Ver `docs/07_EXTERNALS.md §7.4` — lista de 7 técnicas priorizadas del análisis de referencia,
con la de SpeedHack por rebobinado de dirección de retorno a la cabeza.

### Deuda técnica menor
- Dos copias de LuaJIT en el árbol (`LuaJIT/` y `_reference/lua/LuaJIT/`).
- `.vs/` (~4,8 GB) y `ByxStorm/` (~98 MB, intermedios de un build Debug obsoleto) son basura
  regenerable — ver `docs/00_OVERVIEW.md` para el comando de borrado.
- `src/Storm/` es legado: `HooksStorm.cpp` es un bloque de comentario marcado `<None>`,
  `HookStormOthers.hpp` (2155 líneas) se incluye desde `DllMain.cpp`, `UtilsStorm.hpp` sí se usa de
  verdad (define macros crudas `max`/`min` — cuidado al incluirlo).
- Guarda de screenshot muerta en `CMenuImgui::Run()` (`static bool cs=false;` nunca se setea).
  Preexistente, se dejó a propósito.
- El botón "Unload" solo cierra el menú, no descarga el DLL. Preexistente.
- El harness headless de menú se perdió (vivía en un scratchpad de sesión).

---

## 8.5 Cómo continuar — backlog priorizado

Si retomás el proyecto y no sabés por dónde arrancar, este es el orden que tiene sentido.

### Primero: cerrar lo que ya está escrito pero no probado

Hay trabajo **terminado y compilando** esperando una sola sesión de juego para confirmarse o
caerse. Es lo más barato que podés hacer y lo que más incertidumbre saca del proyecto.

1. **Probar in-game todo lo de §8.2** (rediseño del menú, fuentes del ESP, port del aimbot,
   `CTraceFilterAim`, multi-punto en Rage). Anotá el resultado ahí mismo.
2. Si el aimbot cambió de comportamiento, el sospechoso número uno es `CTraceFilterAim`:
   su lista negra de clases es nueva y podría estar descartando algo que sí debía bloquear.

### Segundo: las mejoras ya identificadas y especificadas

Están detalladas en `docs/07_EXTERNALS.md §7.4`, en orden de valor. Las tres primeras:

1. **SpeedHack por rebobinado de la dirección de retorno** (`*move_ret_addr -= 5`) en vez del bucle
   de `CALL_ORIGINAL`. Es la más prometedora: elimina de raíz la pelea con los tres gates.
2. **`tick_base--` en los comandos sintéticos** — la corrección que este proyecto no tiene y que es
   candidata a explicar las desincronizaciones conocidas.
3. **`blacklist_time` en el aimbot** (no re-apuntar a algo ya disparado, ponderando latencia).

### Tercero: reconstruir el harness headless de menú

Se perdió. Encontró 3 bugs reales de UI que leer el código no encontró, y el menú se tocó mucho
desde entonces. Receta completa en `docs/01_BUILD.md`.

### No empieces por acá

- **El glitch de ~34k HP / lag de servidor.** Marcado explícitamente como "LUEGO, no ahora".
- **Re-trabajar `CRapidFire`.** Probado y aprobado; no lo toques sin evidencia nueva de un problema.
- **NoSpread de escopeta.** Ya se decidió que es inarreglable con el enfoque actual.

### Higiene, cuando quieras

- Borrar `.vs/` y `ByxStorm/` (≈4,9 GB de basura regenerable — ver `docs/00_OVERVIEW.md`).
- Consolidar las dos copias de LuaJIT (`LuaJIT/` y `_reference/lua/LuaJIT/`).
- Considerar `git init`. Eliminaría la regla R1 entera y su fricción.

---

## 8.6 Línea de tiempo comprimida

| Mes | Qué pasó |
|---|---|
| **may–jul 2026** | base: SDK, hooks, aimbot, ESP, menú |
| **jul 2026** | los exploits de red (`CL_Move`, Bleed/Choke, RapidFire); reorganización física del proyecto a un `.cpp`+`.h` por feature en subcarpeta propia; scrub de atribución externa |
| **fin jul – ago** | auditorías del aimbot (7 rondas), Chams/Glow, sistema de configs por puntero, Lua, rediseños del menú |
| **ago 2026** | ItemESP + OffScreen, análisis de referencia con la tabla de 1616 netvars, arreglo del freeze con ReShade, limpieza de features |

---

## 8.7 Dónde vive el conocimiento histórico completo

Estos `docs/` son un destilado. El registro completo — ~75 notas con el detalle de cada
investigación, cada sesión de IDA y cada bug — está fuera del proyecto, en el archivo de memoria del
agente anterior. **Contiene atribución externa y por eso NO puede vivir dentro del proyecto**
(regla R2). Ver `docs/09_MIGRACION.md`, **Paso 5**, para saber dónde está y cómo consultarlo.
