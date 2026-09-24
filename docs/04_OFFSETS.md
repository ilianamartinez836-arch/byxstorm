# 04 — Offsets, firmas, vtables y class IDs verificados

> Todo lo de acá está **confirmado** contra el binario real del juego (IDA, o parseo del PE en
> disco) o contra al menos dos fuentes independientes. Lo que sea suposición está marcado.
>
> **Regla que aplica a todo este archivo:** preferí `NETVAR(...)` sobre un offset crudo siempre que
> el campo sea un netvar. Los offsets crudos son para estado interno del motor que no viaja por red.

## 4.1 Funciones — firma + RVA de respaldo

Formato: nombre → módulo, RVA de respaldo usada por `Memory::ResolveFn`.

| Función | Módulo | RVA fallback |
|---|---|---|
| `CL_Move` | engine.dll | `0x07D120` |
| `CL_SendMove` | engine.dll | `0x7CEC0` |
| `CL_RunPrediction` | engine.dll | `0x80DA0` |
| `GetClientState` | engine.dll | `0x82CA0` |
| `CBasePlayer::CalcPlayerView` | client.dll | `0x020750` |
| `Copy_Command` | client.dll | `0x10B410` |
| `Set_Host` | client.dll | `0x145010` |
| `Draw_Effect` | client.dll | `0x15C1F0` |
| `CBaseHudChat::ChatPrintf` | client.dll | `0x116260` |

Funciones resueltas por firma en `GameFn::Init()` (`src/SDK/L4D2/GameFunctions.cpp`, portadas de
una referencia externa — solo direcciones, sin lógica). Todas usan `Memory::ResolveFn` (firma →
RVA de respaldo):

| Global `GameFn::` | Qué es | Módulo | RVA fallback |
|---|---|---|---|
| `GetInterpolationTime` | `float GetInterpolationTime()` | engine.dll | `0x91050` (594000) |
| `SetupBones` | `C_BaseAnimating::SetupBones` (0x3C380). ⚠️ es la implementación BASE: NO llamarla directo sobre jugadores (`C_TerrorPlayer`/`C_BaseAnimatingOverlay` la overriden → crash de Rage). Usar el `SetupBones` virtual con tiempo explícito (ver `06_TRAPS.md` A3). | client.dll | `0x3C380` (246656) |
| `CanAttack` | `bool CanAttack(player)` — thiscall, this=jugador, devuelve bool | client.dll | `0x26C880` (2541696) |
| `FireBullet` | `void __thiscall FireBullet(player, eyeX, eyeY, eyeZ, float* angles, int weaponId, void*)` — **cableada** en `BulletSim` (hardenada vía `GameFn`) | client.dll | `0x2F5C20` (3103776) |
| `UpdateSpread` | ⚠️ NO es `void UpdateSpread(weapon)`: es una función de tracking/debug del spread (`Msg("spread ...")`) con args en EDI/ESI — **no cableable desde C++**. Verificado en IDA. | client.dll | `0x30CAD0` (3197648) |
| `SharedRandomFloat` | `RandomFloat` con stream nombrado (spread) | client.dll | `0x1ACDB0` (1756592) |
| `GetActiveWeapon` | `GetActiveWeapon(entity)` | client.dll | `0x12240` (74304) |
| `GetWeaponData` | `GetWeaponData(weapon)` | client.dll | `0x151A0` (86432) |
| `EyePosition` | `EyePosition(entity)` | client.dll | `0x1A7E0` (108512) |
| `SequenceDuration` | `SequenceDuration` | client.dll | `0x2C0B0` (180400) |
| `SelectSequence` | thunk + `jmp` en +7 | client.dll | `0x318D0` (202896) |
| `PerformShoveTrace` | `PerformShoveTrace` | client.dll | `0x312420` (3220512) |
| `GetStudioHeader` | `GetStudioHeader` | client.dll | `0x2140` (8512) |
| `PredictionSeed` | int* global (seed de predicción, deref +12) | client.dll | `0x6BF868` (7075944) |

> ⚠️ **Discrepancia `FireBullet`**: acá se usa `0x2F5C20` (la de la referencia), pero más abajo
> (sección "Direcciones estáticas") el `Fire_Bullet` propio estaba documentado en `0x2F5D20` — 0x100
> bytes de diferencia. `FireBullet` aún NO está cableado a ninguna feature; verificar contra IDA
> antes de usarlo.

Firmas declaradas en `src/SDK/L4D2/Signatures.h` (sin RVA de respaldo):

| Nombre | Módulo | Offset |
|---|---|---|
| `KeyValues_LoadFromBuffer` | engine.dll | 0 |
| `KeyValues_Initialize` | engine.dll | 0 |
| `KeyValues_FindKey` | client.dll | 0 |
| `KeyValues_SetInt` | client.dll | 0 |
| `CMatSystemSurface_StartDrawing` | vguimatsurface.dll | **-27** |
| `CMatSystemSurface_FinishDrawing` | vguimatsurface.dll | **-17** |
| `SetScissorRect` | vguimatsurface.dll | 0 |
| `INetChannel_SendNetMsg` | engine.dll | 0 |
| `WriteUsercmd` | client.dll | 0 |
| `CViewRender_RenderView` | client.dll | 0 |

Otros datos resueltos por firma (en `SDK.h` / headers de interfaz): `IDirect3DDevice9` en
`shaderapidx9.dll`, `RandomSeed` en `client.dll`, `CClientState`, `CClientModeShared`,
`CGlobalVarsBase`, `IInput`, `IUniformRandomStream`, `IViewRenderBeams`, `CBaseFileSystem`.

Direcciones estáticas conocidas:
- `gHUD` = `client.dll + 0x757740` (hookear el constructor de `CHudChat` disparaba demasiado tarde)
- `client.dll + 0x30CAD0` = `UpdateSpread`
- `client.dll + 0x1ACDB0` = `Random_Type` *(el valor `0x1ACE30` que circuló es incorrecto: 0x80
  bytes de más, crash instantáneo)*
- `client.dll + 0x2F5D20` = *mid-function de `FireBullet` (el entry real es `0x2F5C20`, 0x100 bytes antes — verificado en IDA 13/08)*
- `client.dll + 0x726C58` = puntero al jugador local

## 4.2 Índices de vtable

> **Regla R6: nunca cuentes un índice desde los headers de `src/SDK/`.** Ya hubo 3 defectos. Verificá
> contra el call site real del juego.

| Interfaz | Método | Índice | Estado |
|---|---|---|---|
| `IDirect3DDevice9` | `Reset` | 16 | verificado |
| `IDirect3DDevice9` | `EndScene` | 42 | verificado |
| `IBaseClientDLL` | `LevelShutdown` | 6 | verificado |
| `IBaseClientDLL` | `FrameStageNotify` | 34 | verificado |
| `IEngineVGui` | `Paint` | 14 | verificado |
| `IMatSystemSurface` | `LockCursor` | 59 | verificado |
| `IMatSystemSurface` | `OnScreenSizeChanged` | 108 | verificado |
| `IVModelRender` | `DrawModelEx` | 16 | verificado |
| `IVModelRender` | `DrawModelExecute` | 19 | verificado |
| `CClientModeShared` | `CreateMove` | **27** | verificado — **el header `iclientmode.h` está 6 slots corto y contaría 21** |
| `CClientModeShared` | `OverrideView` | **19** | verificado 13/08 — el header lo contaría en 16 (3 slots corto hasta acá, 6 hasta `CreateMove`) |
| `IInput` | `CAM_ToThirdPerson` | 32 | verificado |
| `IGameEvent` | `GetName` | 1 | verificado |
| `IGameEvent` | `GetBool` | 5 | verificado |
| `IGameEvent` | `GetInt` | 6 | verificado |
| `IGameEvent` | `GetString` | **9** | verificado — el header lo tenía en 8 (ver `06_TRAPS.md`) |
| `IGameEventListener2` | `GetEventDebugID` | 2 | **existe y el motor la llama**; faltaba en el header y crasheaba |
| `C_TerrorPlayer` | `GetHealth` | 116 (offset 464) | verificado |
| `C_TerrorPlayer` | `GetMaxHealth` | 118 (offset 472) | verificado |

> Esta tabla lista **solo índices que el proyecto usa explícitamente** con
> `Memory::GetVFunc(iface, N)`. Los métodos que se llaman como virtuales normales de C++
> (`I::MaterialSystem->FindMaterial(...)`, `I::EngineTrace->TraceRay(...)`, etc.) resuelven su
> índice desde el layout del header y **no hay que hardcodearlos**. Si alguna vez necesitás uno de
> esos por número, verificalo en IDA primero (R6) — el índice que devuelva contar los `virtual` del
> header puede no ser el real, porque varias de estas interfaces heredan de `IAppSystem` y algunos
> headers de este SDK están incompletos.

## 4.3 Offsets de estado de red (los del corazón de los exploits)

| Qué | Dónde | Nota |
|---|---|---|
| `client_state` real | `*(engine_base + 4352236)` — RVA `0x4268EC` | **`GetClientState()` (engine+0x82CA0) devuelve ese puntero YA +8.** |
| `choked_commands` | `client_state + 19024` = **`GetClientState() + 19016`** | Triple confirmación independiente. Escribir `+19024` sobre el resultado del accesor es el bug clásico. |
| "próximo tiempo de muestreo permitido" | `GetClientState() + 112` (double) | Ponerlo en 0 abre el segundo gate |
| `CNetChannel::m_fClearTime` | netchannel + **0xB8** | `CanPacket()` es literalmente `net_time > m_fClearTime` |
| `CNetChannel::m_nOutSequenceNrAck` | — | se spoofea a `-1` |
| `CNetChannel::m_nChokedPackets` | — | se spoofea a `255` |
| límite de agrupamiento | `min(campo + 1, 15)` | el motor lo aplica en el constructor de paquete |
| choke máximo | 14 | |
| `new_commands` | ≤ 15 | |
| `backup_commands` | ≤ 7, con `max(2, extra)` | **el 2 es un PISO, no una constante** |
| comandos por paquete | 22 | tope del protocolo |

`CNetChannel::SetChoked()` es solo `++m_nOutSequenceNr; ++m_nChokedPackets;` — **no** lee ack ni
choked. Ese fue el hallazgo que cerró meses de teorías equivocadas sobre los exploits.

## 4.4 Offsets de entidad / arma

| Qué | Offset | Fuente |
|---|---|---|
| `CBaseCombatWeapon::m_hOwner` | **+2392** | tabla verificada de netvars. **No es `m_hOwnerEntity`** |
| `m_flNextPrimaryAttack` | +2400 | delta 0 respecto del ancla de `WeaponState` |
| `m_bInReload` | +2493 | anclado sobre el anterior |
| spread del arma | +0xD0C (3340) | usado por `NoSpread.cpp` |
| `wep + 0xAA4` (2724) | se pone a `75.f` antes de la traza de shove | dos fuentes independientes |
| `last_shove_time` = `m_flNextShoveTime + 8` | 7344 | |
| `m_angEyeAngles` | Player + 0x1384 | silent aim / thirdperson |
| `m_vecPunchAngle` | Player + 0x1204 | NoRecoil |
| `m_nTickBase` | Player + 0x5324 | backtrack |
| `m_flSimulationTime` | Entity + 0x150 | compensación de lag |
| `m_nTickBase` **del lado servidor** | +0x2058 | solo relevante si se emula `AdjustPlayerTimeBase` |
| info de arma: daño | weaponinfo + 3144 y + 2512 | el cliente **no** calcula daño; hay que leerlo de acá |

> **Tabla completa:** `_reference/analisis_externo/l4d2_netvars_offsets.md` tiene **1616 netvars** +
> 23 offsets base / índices de vtable, extraídos de datos de loader de una implementación externa y
> **cruzados contra el mismo build de L4D2 que usa este proyecto**. Es el artefacto de referencia
> más valioso que hay. Consultalo antes de reversear un offset a mano.

## 4.5 Class IDs (`src/SDK/L4D2/tf_shareddefs.h`)

### Jugadores e infectados
```
Boomer = 0        Hunter = 263      Infected = 264    Jockey = 265
Smoker = 270      Spitter = 272     SurvivorBot = 275 Tank = 276
Witch = 277       Charger = 99      CTerrorPlayer = 232
CTerrorPlayerResource = 233
```

> `Boomer = 0` es real. Por eso `GetClassId()` **no sirve** para descartar "sin clase": funde ambos
> casos en 0. Hay que leer `GetClientClass()` directamente.

### Armas — solo las clases **hoja** llegan por red
```
CAssaultRifle=1    CAutoShotgun=2     CChainsaw=39      CGrenadeLauncher=96
CMagnumPistol=116  CPistol=131        CPumpShotgun=148
CRifle_AK47=152    CRifle_Desert=153  CRifle_M60=154    CRifle_SG552=155
CShotgun_Chrome=162 CShotgun_SPAS=163 CSMG_MP5=165      CSMG_Silenced=166
CSniper_AWP=169    CSniper_Military=170 CSniper_Scout=171 CSniperRifle=172
CTerrorGun=230     CTerrorMeleeWeapon=231
```

`CTerrorGun` y `CTerrorMeleeWeapon` **sí** son reales: los usan las armas sin subclase propia
(la uzi, todos los cuerpo a cuerpo).

`CTerrorWeapon = 235` y `CBaseCombatWeapon = 11` **NUNCA llegan como class id** — son clases base de
la jerarquía; el juego siempre manda la hoja concreta. Filtrar por ellas no matchea nada.

### Otros útiles
```
CBaseAnimating = 4     CBaseUpgradeItem = 29 (mira láser)   CDynamicProp = 52
CFuncPlayerGhostInfectedClip = 85    CFuncPlayerInfectedClip = 86
```

`CDynamicProp = 52` es genérico: un mapa lo usa para cualquier cosa. No lo clasifiques por id.

### Mapa de hitbox por ClassId
```
Charger(99)=9   Hunter(263)=10   Common(264)=15   Jockey(265)=4
Smoker(270)=10  Spitter(272)=4   Tank(276)=12     Witch(277)=10
```

## 4.6 Semillas de NoSpread (escopetas)

```
AutoShotgun (2):        command# = -2134739495, seed = 11144000
PumpShotgun (148):      command# = -2139542887, seed = 1246243990
SPAS/Chrome (162/163):  command# = -2139097805, seed = 494641349
otras armas:            seed = 0
```

Solo funciona con escopetas. Las semillas para el resto de las armas nunca se encontraron.

## 4.7 Verdad de terreno sobre el disparo (L4D2 `FireBullet`)

- Ángulo del disparo = `EyeAngles + spread + punch`.
- Loop de perdigones para escopetas.
- Acumulador de spread en `weapon + 0xD0C`.
- **El cliente NO calcula daño.** No tiene sentido hookear buscando daño: hay que leer
  `weaponinfo + 3144` / `+2512`.
- NoSpread/NoRecoil se reducen a **2 deltas**: restar el punch y limpiar la velocidad de punch.

## 4.8 Sistema de glow (L4D2)

- Layout de `CGlowProperty` reverseado y documentado.
- **Tipo 3 = incondicional** (se ve siempre).
- **Rango 0 = ilimitado.**
- Truco de `SetGlowType` documentado en la memoria del proyecto.

## 4.9 Peculiaridad de la vida en L4D2 (importante para ESP y daño)

- Mientras un superviviente está **incapacitado**, `m_iHealth` **no es vida**: es el contador
  regresivo de desangrado. Salta a ~294 y baja. La HUD nativa del juego lee **exactamente el mismo
  campo crudo** (confirmado en IDA: `CHudZombieHealth` llama al virtual `GetHealth()`, que es un
  `return this[N]` trivial), así que también se le desborda la barra. No es un bug nuestro.
- El escudo temporal vive en `m_healthBuffer` (puntos) + `m_healthBufferTime` (timestamp de
  decaimiento). El truco de incap+revive corrompe **el timestamp**, no el monto: lo deja decenas de
  miles de segundos en el futuro, así que el decaimiento nunca arranca y los ~30 puntos quedan
  efectivamente permanentes. El número crece cuanto más se sostiene el exploit.
- El ESP detecta ese estado (`buf > 0.5 && m_healthBufferTime - curtime > 120s`), etiqueta
  `[GOD MODE]` y muestra el timestamp como número.
