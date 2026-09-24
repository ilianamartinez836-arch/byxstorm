# Documentos históricos

Vivían en la raíz del proyecto. Se movieron acá el 2026-08-13 para dejar la raíz limpia.
**No se borró nada.** Pero no todos siguen siendo válidos — leé el estado de cada uno antes de
usarlo como referencia.

| Archivo | Fecha | Estado |
|---|---|---|
| `BYX_STORM_PARTE2.txt` | jul 2026 | **Parcialmente vigente.** La tabla de offsets, los class IDs y el mapa de hitbox por ClassId siguen siendo correctos (re-verificados en `docs/04_OFFSETS.md`). Su sección "LO QUE QUEDÓ PENDIENTE" está vencida |
| `FEATURES_LIST.txt` | jul 2026 | **Desactualizado.** Su lista de "pendientes" incluye cosas ya implementadas (Chams, Glow, Thirdperson, NoFallDamage, Fakelag, Lua, HitSounds, ChatSpam…) y su lista de hooks activos está incompleta. Valor histórico únicamente |
| `ANALISIS_SESION_2026-07-14.txt` | 14/07/2026 | Registro de una sesión de trabajo. Histórico |
| `ANALISIS_SESION_2026-07-15.txt` | 15/07/2026 | Registro de una sesión de trabajo. Histórico |
| `build_warnings_snippet.txt` | ~15/07/2026 | Se llamaba `.txt` (sin nombre) en la raíz. Es un fragmento de salida de MSBuild con los warnings conocidos de `json.hpp` e `ISurface.h`. Sin valor más allá de documentar cómo se ven esos warnings |

## Dónde está la referencia vigente

- Offsets, firmas, vtables y class IDs → `docs/04_OFFSETS.md`
- Inventario de features actual → `docs/03_FEATURES.md`
- Estado y pendientes → `docs/08_ESTADO.md`
