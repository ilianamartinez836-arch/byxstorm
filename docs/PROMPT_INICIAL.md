# Prompt de arranque para un agente nuevo

Copiá y pegá el bloque de abajo como **primer mensaje** en Cursor (o la herramienta que uses), con
la carpeta `C:\Byx_storm-L4D2` abierta como proyecto.

Es de un solo uso: sirve para que el agente se oriente, verifique que puede compilar y confirme que
entendió las reglas. Las sesiones siguientes no lo necesitan, porque `AGENTS.md` se carga solo.

---

## Prompt

```
Vas a trabajar en ByxStorm: un DLL de C++20 x86 que se inyecta en Left 4 Dead 2 (motor Source).
Está en C:\Byx_storm-L4D2 y NO es un repositorio git.

Antes de tocar nada, leé en este orden y confirmame que lo hiciste:

1. C:\Byx_storm-L4D2\AGENTS.md
   Es tu archivo de instrucciones. Tiene las 9 reglas del proyecto (R1-R9), el comando de
   compilación, las 5 macros con las que está escrito todo, y el índice del resto.

2. C:\Byx_storm-L4D2\docs\06_TRAPS.md
   ~30 trampas conocidas con su síntoma y su causa. Cada una costó entre horas y días.
   Es el documento con más valor por token del proyecto.

3. C:\Byx_storm-L4D2\docs\08_ESTADO.md
   Dónde quedó todo y el backlog priorizado (sección 8.5, "Cómo continuar").

El resto de docs/ se lee BAJO DEMANDA, no de entrada. El índice está al final de AGENTS.md:
  docs/00_OVERVIEW.md      qué es, ciclo de vida del DLL, mapa de carpetas
  docs/01_BUILD.md         toolchain, compilar, agregar archivos al .vcxproj
  docs/02_ARCHITECTURE.md  macros, los 18 hooks, pipelines, config, Lua
  docs/03_FEATURES.md      inventario de features por pestaña del menú
  docs/04_OFFSETS.md       offsets, firmas, vtables y class IDs verificados
  docs/05_RULES.md         las reglas con el contexto de por qué existen
  docs/07_EXTERNALS.md     dependencias y herramientas
  docs/09_MIGRACION.md     cómo está armado este paquete
  docs/_historico/         .txt viejos, con su estado de validez

Material de referencia que NO se compila:
  _reference/analisis_externo/l4d2_netvars_offsets.md
      1616 netvars + 23 offsets base verificados contra este mismo build de L4D2.
      CONSULTALO ANTES de reversear un offset a mano.
  _reference/analisis_externo/01_nucleo_l4d2.md y 02_visuales_framework_loader.md
      análisis de una implementación de referencia externa: técnicas, comparación, qué falta.
  _reference/lua/API.md
      API de Lua expuesta a los scripts, con 7 ejemplos en _reference/lua/examples/.

Para compilar usá PowerShell (Git Bash mangla los switches de MSBuild y falla con MSB1008):

  & "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\Byx_storm-L4D2\ByxStorm.sln" /t:Build /p:Configuration=Release /p:Platform=x86 /m /v:minimal

Éxito = exit code 0 Y la línea final "-> C:\Byx_storm-L4D2\Release\ByxStorm.dll".
No greppees "error" a secas: la palabra está dentro de "terror" (c_terror_player.h, CTerrorPlayer)
y da falsos positivos en cada build. Usá "error :" y "warning :", con espacio y dos puntos.
El total de warnings (~248) NO es señal de regresión: es ruido de headers de terceros, una vez por
unidad de traducción. No vuelques el log al contexto, son ~150 KB.

Para reversear el juego hay un servidor MCP de IDA Pro 9.3 en http://127.0.0.1:13337/mcp
(requiere IDA abierto con client.dll o engine.dll). Casi todo lo que este proyecto sabe salió de
ahí. Alternativa sin IDA: Python con pefile + capstone sobre el DLL en disco.

LO MÁS IMPORTANTE — R9: no tenés memoria entre sesiones, así que docs/ ES tu memoria.
Todo hallazgo que valga la pena lo escribís ahí ANTES de dar una tarea por terminada:
offsets nuevos -> 04_OFFSETS.md · bugs de causa no obvia -> 06_TRAPS.md ·
features -> 03_FEATURES.md · decisiones mías -> 05_RULES.md · estado y pendientes -> 08_ESTADO.md.
Registrá el SÍNTOMA y por qué la respuesta obvia era incorrecta, no solo la conclusión.
La sección 7 de AGENTS.md tiene el protocolo completo.

Tres reglas que si las rompés me cuestan tiempo real:
- NO hay git: hacé backup en _backups/pre_<motivo>_<AAAAMMDD>/ antes de cambios mecánicos grandes.
- Un archivo nuevo va en ByxStorm.vcxproj Y en ByxStorm.vcxproj.filters. No hay globbing:
  un .cpp sin listar no se compila y NO da error.
- Nunca nombres fuentes externas (personas, cheats de terceros, archivos compartidos) dentro del
  proyecto, ni en comentarios ni en strings de UI. Lenguaje neutro: "una implementación de
  referencia externa".

Cuando termines de leer, respondeme con:
 a) las 9 reglas en una línea cada una, con tus palabras;
 b) qué son los TRES gates de CL_Move y qué síntoma distinto produce cada uno si falta;
 c) el resultado de correr el comando de compilación tal cual está arriba;
 d) qué proponés hacer primero según docs/08_ESTADO.md sección 8.5.

No edites nada hasta que yo apruebe lo que propongas.
```

---

## Por qué el prompt pide esas cuatro cosas al final

No es ceremonia: cada una verifica que algo de la cadena funciona.

| Pide | Verifica |
|---|---|
| (a) las 9 reglas | que realmente leyó `AGENTS.md` y no lo resumió de memoria |
| (b) los 3 gates de `CL_Move` | que entiende el subsistema más difícil del proyecto. Está en `docs/06_TRAPS.md §C2`. Si inventa la respuesta, ya sabés que alucina en vez de leer |
| (c) el build | que su shell no le mangla los switches y que el toolchain está bien |
| (d) qué hacer primero | que sabe leer el backlog en vez de improvisar |

Si falla (b) o (c), arreglá eso antes de darle trabajo real.

## Para las sesiones siguientes

Ninguna necesita este prompt. `AGENTS.md` (o `.cursorrules` / `.cursor/rules/byxstorm.mdc`) se
carga solo. Alcanza con pedir la tarea. Si el agente empieza a ignorar las reglas —típico cuando la
conversación se hace larga y el contexto se llena— recordáselo con una línea:

```
Releé AGENTS.md antes de seguir, sobre todo R9 (registrar en docs/).
```
