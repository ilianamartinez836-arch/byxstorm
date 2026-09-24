# 05 — Reglas de trabajo (con el porqué)

Las versiones cortas están en `AGENTS.md`. Acá está el contexto que las hace convincentes: cada una
salió de un problema real.

---

## R1 — No hay git. Hacé backup antes de cambios mecánicos grandes

El proyecto **no es un repositorio git**. No hay `git diff`, no hay `git checkout --`, no hay
historial. Un `sed -i` mal escrito sobre 300 archivos es irrecuperable.

**Cómo se hace acá:** copiar `src/` + `ByxStorm.vcxproj` + `ByxStorm.vcxproj.filters` a
`_backups/pre_<motivo>_<YYYYMMDD>/` antes de empezar. Ya hay 7 snapshots con ese formato:

```
_backups/pre_rapidfire_20260717_180036/
_backups/pre_exploits_misctools_split_20260722_172427/
_backups/pre_structure_reorg_20260721_204141/
_backups/pre_menu_restyle_20260804/
_backups/pre_feature_removals_20260806/
_backups/pre_aimbot_port_20260810/
_backups/pre_menu_polish_20260811/
```

Los snapshots también sirven para **diffs de símbolos**: comparar la lista de símbolos
`Features::`/`Config::` contra el backup previo es cómo se verificó que un restyle de 3000 líneas no
perdiera nada (227 símbolos, todos presentes).

> Si en algún momento se inicializa git, esta regla se puede relajar. Hasta entonces, no.

---

## R2 — Cero atribución externa dentro del proyecto

**Nunca** nombrar fuentes externas dentro de archivos del proyecto: ni en comentarios, ni en strings
de UI visibles en la app compilada, ni en los `.txt` de la raíz. Se usa lenguaje neutro:

- ✅ "una implementación de referencia externa"
- ✅ "un binario de referencia compilado"
- ✅ "un script de referencia"
- ❌ nombres propios de personas, cheats o herramientas de terceros
- ❌ nombres de archivo delatores (un `.lua` compartido, un `.dll` de terceros)

Se extiende a **cualquier artefacto identificable por asociación**: nombres de binario específicos,
nombres de función o variable copiados verbatim de una fuente identificable.

También: **nunca dejes referencias tipo `[[memoria]]` dentro del código**. Ya se encontraron y
quitaron varias incrustadas en `.cpp`/`.h`. Eso pertenece al archivo de memoria del agente, no al
proyecto.

**Motivo:** opsec. Si el código o los docs se filtran o los lee otra persona, no deberían revelar de
dónde salió cada feature.

**Alcance ya decidido:** `_backups/*` no se toca (son snapshots históricos, no estado vivo) y la
config local de permisos de herramientas tampoco (es configuración local, no contenido distribuible).

---

## R3 — Cada feature en su pestaña por categoría

| Pestaña | Qué va |
|---|---|
| **Exploits** | trucos de paquete/tick únicamente |
| **Movement** | posición / colisión |
| **Aimbot** | todo lo relacionado a puntería (integrado en `Features::Aimbot`, no un sistema paralelo) |
| **ESP / Visuals** | visual |
| **HUD** | overlays informativos |
| **Misc** | el resto. Los toggles de arma sin pestaña propia (NoRecoil, AutoPistol) van en **Misc > Combat** |

**Motivo:** el usuario organiza la UI por **lo que la feature hace desde la perspectiva del
jugador**, no por la plomería que comparte internamente. Varias features de Movement y Aimbot usan
el mismo mecanismo de `CL_Move` que los Exploits — eso no las convierte en exploits.

No crear pestañas nuevas sin preguntar.

**Corolario — features con el mismo nombre y distinto mecanismo:** si aparecen dos, **no borres
ninguna asumiendo que una es código muerto**. Etiquetalas para que no se confundan y preguntá. Ya
pasó con dos "SpeedHack" distintos.

---

## R4 — Un listen server no valida nada de red

Un servidor local (crear partida propia, host = tu máquina) **no es superficie de prueba válida**
para nada que dependa de entrega/validación real de paquetes: cliente y servidor corren en el mismo
proceso, sin ida y vuelta por socket.

**Consecuencia:** un hack de netchannel genuinamente roto — ack/choke mal manejado, paquetes que en
una conexión real se descartarían — se ve **perfecto** ahí.

**Confirmado el 16/07:** RapidFire disparaba y registraba daño correctamente en listen server todo
el tiempo, mientras fallaba en servidores oficiales. Si se hubiera tomado el listen server como
suficiente, el bug real (una regresión de timing en el restore de ack/choked) habría parecido
arreglado sin estarlo.

**Cómo se aplica:** antes de dar por verificado un fix de `CL_Move`, `CNetChannel`, o cualquier
exploit de red, **pedí confirmación explícita de que la prueba fue contra un servidor dedicado
real**. Un "a mí me anda" en listen server no es evidencia en ninguna dirección.

---

## R5 — Instrumentar antes que deducir

*"Optemos por hacer debug (solo si es más preciso) de ahora en adelante."*

El calificador es del usuario: no todo necesita una captura, y reversear el binario sigue siendo la
mejor herramienta cuando la respuesta es **estática** (offsets, índices de vtable, contenido de VMT).
Pero cuando un log sería más preciso que el razonamiento, poné el log.

**De dónde salió:** una investigación en la que se enviaron cuatro rondas de fixes confiados
construidos sobre evidencia mal leída. Cada conclusión era firme y venía de un log que no estaba lo
bastante afinado.

**Qué es buena instrumentación acá:**

- **Imprimí un VEREDICTO, no columnas para adivinar.** `-> Resolve=ACEPTA/RECHAZA` le gana a una
  columna de flags.
- **La clave de deduplicación tiene que incluir todo lo que varía**, o esconde justo el caso que
  estás cazando (un dedup que omitía los flags ocultó el segundo dibujo de cada entidad).
- **Logueá el nombre del modelo junto al class id.** "classid=4, entidx=-1" es ambiguo; el modelo no.
- **Verificá que la captura contenga el sujeto.** Dos capturas seguidas de "commons" no tenían ni un
  common adentro (el usuario estaba en un mapa de prueba). Eso no es evidencia de nada.

**Ejemplo de que funciona:** el toggle `bLogUnknown` de ItemESP se pagó solo de inmediato — dos
líneas de log expusieron que `strstr` sobre rutas de modelo fallaba por mayúsculas y que faltaba una
clase.

---

## R6 — No confíes en un índice de vtable de los headers del SDK

`src/SDK/L4D2/` es una adaptación de headers de Source. **Ya aparecieron tres defectos distintos:**

1. `IGameEventListener2` no declaraba `GetEventDebugID` → **crash real al entrar a una partida**.
2. `IGameEvent::GetString` estaba en el slot 8; el real es **9** → toda lectura de campo string
   devolvía basura.
3. `iclientmode.h` está **6 slots corto**: contaría `CreateMove` en 21 cuando el real es **27**.

**Cómo verificar:** desensamblá en IDA una función del **módulo que es dueño de la interfaz** (no el
que la consume) y leé los offsets de sus llamadas indirectas. `[eax+36]` = índice 9, y listo.

Corolario para clases derivadas: si vas a **implementar** una interfaz del SDK que nunca se
instanció antes en este proyecto, contá los métodos que el motor realmente despacha. Un virtual
faltante en una interfaz declarada es invisible hasta que algo la implementa **y** el motor la
llama.

---

## R7 — Nunca escanees por firma una función que además hookeás

MinHook escribe un `jmp` en el prólogo de la función hookeada. Un escaneo posterior de la misma
firma **falla siempre** (el `55 8B EC ...` inicial ya no está), y la guarda de fallback de
`ResolveFn` — correctamente — rechaza la RVA vieja porque el primer byte ya no coincide.

**Cómo se rompió:** `CBaseHudChat::ChatPrintf` se resolvía en dos lugares. El hook resolvía bien al
arrancar y parcheaba; el segundo resolvedor (un `static` perezoso dentro de `EnemyChat`) corría
recién en el primer mensaje de chat, ya con el prólogo parcheado, y fallaba. Enemy Chat y Kill
Notifier armaban mensajes que nunca se imprimían. El mensaje de error culpaba a "¿se movió la
función?" — mal atribuido, pero la guarda hizo su trabajo.

**Solución:** un resolvedor único para todo el proyecto
(`Features::MiscTools::EnemyChat::ResolveChatPrintfOnce()`, un `static DWORD` local a la función).
La dirección del `MAKE_HOOK` lo llama (se evalúa como argumento de `Hook.Create`, o sea **antes**
del parche) y el otro consumidor devuelve el valor cacheado.

**Auditalo** cada vez que agregues un hook cuya dirección se necesite en otro lado.

---

## R8 — Definición de "terminado"

Una feature nueva no está lista hasta que:

1. Compila con 0 errores en `Release|x86`.
2. Sus archivos están **en `ByxStorm.vcxproj` Y en `ByxStorm.vcxproj.filters`** (no hay globbing;
   un `.cpp` sin listar no se compila y no da error).
3. Tiene sus líneas `CFG("clave", var)` en `src/App/Features/ConfigRegistry.cpp` — si no, nada
   persiste y el usuario pierde sus ajustes en cada reinyección.
4. Está en la pestaña correcta (R3).
5. Si toca la red, quedó marcada como **no verificada** hasta que se pruebe en dedicado (R4).

---

## Preferencias del usuario ya establecidas

- **No re-trabajar la arquitectura de `CRapidFire`** sin evidencia nueva de un problema. Fue probada
  in-game y aprobada explícitamente. Si hay que investigar algo ahí, investigá el síntoma
  específico, no re-derives el mecanismo entero.
- **Un `.cpp` + un `.h` por feature, en su propia subcarpeta.** Nada de archivos "puente" que junten
  la config y los toggles de varias features. Esa arquitectura se eliminó a pedido explícito.
- El usuario **prefiere los números crudos más grandes** en el tradeoff conocido de Bleed/Choke
  (más paquetes/pps aunque parte del tráfico sean reenvíos duplicados) por sobre la versión
  conservadora de solo-ticks-reales. Ya se decidió, ya se revirtió una vez y se restauró. No lo
  cambies sin preguntar.
