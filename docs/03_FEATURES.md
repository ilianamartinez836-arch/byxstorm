# 03 — Inventario de features

Las pestañas del menú están declaradas en `src/App/Features/Menu/ImguiMenu.cpp`; buscá
`static const TabDef kTabs[]` (cerca del final del archivo). El orden de ese array es también el
que recorre el buscador de features.

```cpp
{"Aimbot",       DrawAimbotTab,      "COMBATE",  ICON_CROSSHAIR }
{"Aim Infected", DrawAimInfectedTab, "COMBATE",  ICON_TARGET    }
{"ESP",          DrawESPTab,         "VISUALES", ICON_EYE       }
{"Visuals",      DrawVisualsTab,     "VISUALES", ICON_SPARKLE   }
{"Movement",     DrawMovementTab,    "JUEGO",    ICON_MOVE      }
{"Exploits",     DrawExploitsTab,    "JUEGO",    ICON_BOLT      }
{"HUD",          DrawHudTab,         "JUEGO",    ICON_HUD       }
{"Misc",         DrawMiscTab,        "JUEGO",    ICON_GEAR      }
{"Lua",          DrawLuaTab,         "SISTEMA",  ICON_CODE      }
{"Configs",      DrawConfigsTab,     "SISTEMA",  ICON_SLIDERS   }
```

El grupo (`COMBATE`/`VISUALES`/`JUEGO`/`SISTEMA`) es puramente presentación: encabezado en la barra
lateral.

---

## COMBATE

### Aimbot — `src/App/Features/Aimbot/`
El módulo más trabajado del proyecto (7 rondas de auditoría/fixes documentadas).

- Dos modos: **Rage** y **Legit** (`cfg.nMode`, `MODE_LEGIT`).
- Struct de config con ~45 campos, registrador propio `Features::Aimbot::RegisterConfig()`.
- **Zonas por nombre de hueso**, no por HITGROUP: los HITGROUP de L4D2 están mal asignados en rigs
  custom, así que se resuelve el hueso por nombre.
- **Multi-punto** aplicado **solo a la caja preferida**, no al barrido de respaldo (7 traces ×
  cada hitbox de un objetivo tapado = el stutter del Vis Check del 24/07).
- **Exigir-hitgroup** y multi-punto se ejecutan en Rage *y* Legit. Durante un tiempo estuvieron
  escritos dentro de la rama `MODE_LEGIT`, así que en Rage — el modo por defecto — nunca corrían:
  un centro tapado hacía que Rage abandonara la zona configurada y barriera a otro hueso.
- Compensación de lag por interpolación, filtro de jugador local, humanización, smoothing.
- **Lock-on exacto (Legit, no-silent)** — `bDeadOnLock` (ON por defecto) + `flOnTargetDeg` (0.2°).
  El smooth proporcional es decaimiento exponencial: se acerca al target pero **nunca aterriza**,
  siempre deja un residuo (la mira queda a milésimas de grado al lado de la cabeza). Con esto, en
  cuanto un paso pone la mira a menos de `flOnTargetDeg` del punto, se clava **exacta** en él y el
  disparo solo se libera cuando está dead-on. Antes el gate de disparo estaba fijo en 1° (≈35 cm a
  20 m) y el "ease-in al cambiar target" no estaba cubierto (disparaba mientras la mira viajaba);
  ahora el gate también cubre el ease-in. `Random hitbox` clava en el punto aleatorio elegido.
- Submódulos:
  - `AutoShove/` — shove automático. Corre **antes** del aimbot (ver pipeline en `02`).
  - `BulletSim/` — simulación de bala.
  - `TraceFilter/` — `CTraceFilterAim`, filtro de traza con lista negra de clases
    (`CBaseAnimating`, `CFuncAreaPortalWindow`, `CFuncPlayerInfectedClip`,
    `CFuncPlayerGhostInfectedClip`, `CEnvPhysicsBlocker`). Puertas y rompibles bloquean balas pero
    no shoves (`bForShove`). Opción de ver a través de compañeros (`bAvoidTeammates`).
  - `WeaponState/` — gates de arma (`m_flNextAttack`, recarga).
- `bIgnoreDormant` (off por defecto) — apunta también a infectados **dormant** (fuera del PVS).
  Útil con speedhack/rapidfire + choke altos, donde los infectados quedan lejos y el servidor los
  duerme; sin esto el aimbot se queda "sin objetivo". Seguro: si `SetupBones` falla, `ComputeAim`
  descarta la entidad igual.
- `bBacktrack` (off por defecto) — apunta contra la **pose del servidor** (`m_flSimulationTime`) en
  vez de la pose interpolada del render (`GetHitboxPosAt`/`GetHitboxPositionsAt`, que posan el
  esqueleto en un tiempo explícito). Con speedhack/rapidfire + choke altos la interpolación se
  corrompe y el ángulo apuntaba a una pose vieja; con esto el ángulo y el `tick_count` del comando
  (que ya no suma `+interp`) son consistentes con lo que la lag compensation del server va a
  rebobinar. En juego normal con interp sana, OFF es lo correcto.
  También se activa solo si `DisableInterp` está ON: con `cl_interpolate 0` el render ya posa en
  `m_flSimulationTime`, así que el aim y el `tick_count` usan ese mismo timeline (ver `UsesServerTruth`).
- `bIgnoreCooldown` (off por defecto) — salta **solo** el gate de cooldown del arma
  (`m_flNextPrimaryAttack`). Con speedhack/rapidfire + choke altos, `ApplyExtraScaling` estira
  `m_flSimulationTime` y el `tickbase--` corre el reloj de la predicción, así que el cooldown
  PREDICHO deja de coincidir con el tick que el server va a aceptar y el bot deja de disparar
  justo cuando los exploits vuelcan tiros extra. El lockout de deploy/shove (`m_flNextAttack`) y
  el melee mid-swing se siguen respetando. Con semi-auto, deja que el RapidFire maneje el toggle
  0->1. En juego normal, OFF es lo correcto.
- Círculo de FOV dibujado en el hook de `Paint`. `cfg.flFOV` **ya es un semiángulo**: el círculo usa
  `tan(flFOV)`, no `tan(flFOV/2)`.

### Aim Infected — `src/App/Features/Aimbot/AimInfected.{h,cpp}`
Feature **separada** del aimbot principal, con su propia tecla de hold. Corre después de `Aimbot`
en `Copy_Command` para ganar `pCmd->viewangles` si los dos se activan a la vez.
Config: `bEnabled`, `nKey`, `flMaxDist`, `flSmooth`, `bVisCheck`.

---

## VISUALES

### ESP — `src/App/Features/ESP/`
- `ESP.cpp` — el ESP de jugadores: cajas dinámicas basadas en hitbox, huesos, barra y número de
  vida, snapline, head dot, nombres.
- `ESPSettings.cpp` — 7 paneles × ~24 campos, registrador propio.
- `States.cpp` — etiquetas `[DOWN]`, `[SMOKER]`, `[HANGING]`, `[GOD MODE]`.
- `Weapons.cpp` — mapa modelo→nombre de arma. Reutilizado por ItemESP y por el Kill Notifier.
- `ItemESP.cpp` — objetos tirados en el mapa. Categorías: Salud / Granadas / Munición / Armas /
  Objetos de mapa, cada una con toggle y color. Sin traza de visibilidad (deliberado).
- `OffScreen.cpp` — flechas en el borde de la pantalla para amenazas fuera de vista.

### Visuals — `src/App/Features/Visuals/`
- `Glow/` — brillo sobre entidades vía `CGlowProperty`.
- `Chams/` — materiales custom vía `DrawModelExecute` + `FindMaterial`.
- `Visuals.h` — overlays: watermark, contador de FPS, velocímetro.

### Effects — `src/App/Features/Effects/`
`bNoVomit`, `bNoSmoke`, `bNoSpitterAcid`, `nTankRocks`.
Este último es un modo con 4 valores (`ROCKS_OFF` / `ROCKS_PARTICLES` / `ROCKS_DEBRIS` /
`ROCKS_ALL`), no un bool: antes era `bNoTankRocks` y una config vieja con esa clave simplemente se
ignora. `ROCKS_ALL` incluye ocultar el **modelo** de la roca en vuelo (vía `EF_NODRAW`), no solo
las partículas.
**Toca cvars globales del juego** → `Effects::Restore()` es obligatorio en `Shutdown`.

---

## JUEGO

### Movement — `src/App/Features/Movement/`
`Bunnyhop`, `AutoStrafe`, `AutoDuck`. Globales planas `Features::bBunnyhop` etc. (no namespaces
por feature — es la excepción histórica del proyecto).

### Camera — `src/App/Features/Camera/`
- `Thirdperson.{h,cpp}` — hookea `IClientMode::OverrideView` (vtable idx **19**) y escribe
  `pSetup->origin/angles` ahí (el último hook antes del render). Dibuja el modelo escribiendo el
  flag crudo `m_fCameraInThirdPerson` (0xB1). Con suavizado orbital opcional. `Restore()` obligatorio
  en `Shutdown`.
- `ViewModel.{h,cpp}` — posición del arma en pantalla.
- `FreeLook/` — mirar sin girar. Corre **antes** del aimbot.

### Exploits — `src/App/Features/Exploits/`
La pestaña de trucos de paquete/tick. **Solo va acá lo que manipula paquetes o ticks** (regla R3).

| Carpeta | Mecanismo |
|---|---|
| `SpeedHack/` | llamadas extra a `CL_Move`. Movimiento solo: se le quitan `IN_ATTACK`/`IN_ATTACK2` |
| `CustomSpeedHack/` | ídem pero sin quitar los botones de ataque |
| `RapidFire/` | algoritmo `CRapidFire`: acumulador propio + cash-in por progreso de ack |
| `RapidFireCustom/` | **no** usa ticks extra: pone `weapon+2400` (cooldown) a 0 cada comando |
| `BleedExploit/` + `BleedExploitImproved/` | escriben `clientstate+19016` para que el motor agrupe comandos de respaldo |
| `ChokeExploit/` + `ChokeExploit2/` | mismo campo; los dos suman si están ambos activos |
| `Fakelag/` | sube `m_fClearTime` para cerrar el gate de envío de `CL_Move` |
| `SendMoveOverride/` | reemplaza la construcción del paquete (`nMaxNew`, `nMaxBackup`, `nBias`) |
| `AirStuck/` | quedarse en el aire |
| `RollExploit/` | roll |
| `NoFallDamage/` | sin daño de caída (`flThreshold`, `nShift`, `flGravity`) |
| `NameBug/` | nombre con caracteres/saltos de línea inyectados |
| `ChargerTurn/` | byte patch sobre `client.dll`, no toca comandos |
| `Reference/` | **par A/B**: `SpeedHackRef` + `RapidFireRef` + `RefDriver`, copia aparte con estado propio para comparar lado a lado con las nuestras. Claves de config propias |

`ExploitsDiag.h` — toggles de diagnóstico (`bLogBleedChoke`), **no** persistidos a propósito.
También telemetría de "basura" (wraps del buffer de comandos por segundo, escrita por `CL_Move.cpp`
y mostrada en Network Info para afinar los sliders al máximo sostenible).

### HUD — `src/App/Features/MiscTools/`
`NetworkInfo/` (latencia/pérdida/choke/pps en vivo + indicador "BASURA" de wrap del buffer de
comandos — la herramienta para verificar los exploits y afinar el máximo sostenible),
`Watchers/` (observadores de netvars), `DamageIndicator/`, `VirtualKeyboard/`, `MovementRecorder/`.

> Network Info y Watchers se dibujan en el **pipeline de ImGui**, no en el de VGUI, para poder ser
> ventanas movibles/redimensionables de verdad.

### Misc — `src/App/Features/Misc/` y parte de `MiscTools/`
`AutoPistol`, `NoSpread`, `NoRecoil` (en `Weapon/`), `DisableInterp`, `AutoClicker/`
(izq+der con CPS configurable y gate de "no mientras curás"), `Binds/`.
Troll: `ChatSpam/`, `NameStealer/`.
Chat/eventos: `EnemyChat/` (spy de chat enemigo), `KillNotifier/`, `HitSounds/`.

> Precedente establecido: los toggles de arma sin pestaña propia (NoRecoil, AutoPistol) van bajo
> **Misc > Combat**. Seguí usándolo.

---

## SISTEMA

### Lua — `src/App/Features/Lua/`
`LuaEngine.{h,cpp}` (~2300 líneas) + `LuaShim.{h,cpp}`. La pestaña muestra la versión de LuaJIT
cargada, la consola de salida coloreada (rojo si la línea contiene "error", ámbar si "warn"), la
lista de scripts y sus paneles propios (`byx.tab`).

### Configs — `ImguiMenu.cpp::DrawConfigsTab`
Guardar/cargar/borrar configs con nombre en `ByxStorm/configs/*.json`.

---

## Menú: detalles de implementación que conviene saber

- Abre/cierra con **INSERT** (`k=45`). Se cierra solo si el juego pierde el foco.
- Se cierra por **tres** caminos (INSERT, la X del header, el botón Unload) y los tres tienen que
  restaurar el cursor — se sincroniza contra el estado del frame anterior en `Run()`.
- **ESC durante la captura de tecla = DESBINDEAR** (guarda tecla 0), no cancelar. El 0 es el valor
  "sin tecla" que todas las features entienden.
- El estilo se re-aplica comparando 5 floats por frame (acento RGB + opacidad + escala de UI) en vez
  de acordarse de llamar `SetStyle()` en cada punto que los toca.
- Grid de tarjetas (`BeginCard`/`EndCard`), tooltips de ayuda, buscador entre pestañas, favoritos,
  contador de activos por tarjeta, toasts, escala de UI, animación de cascada.
  **Invariante a chequear después de editar el menú:** los `BeginCard(` y `EndCard()` tienen que
  dar el mismo conteo, cada pestaña tiene que llamar a `CardsEnd()`, y los Push/Pop de estilo
  tienen que estar balanceados (el grep crudo se ve desparejo por los `PopStyleColor(6)` /
  `PopStyleColor(2)` y el `PushFontIf` condicional — contá argumentos, no líneas).
- El botón **"Unload" solo cierra el menú**, no descarga el DLL (comportamiento preexistente).
  Para descargar: **F11**.
