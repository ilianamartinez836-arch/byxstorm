#pragma once
#include "../../../../SDK/SDK.h"

// Movement Recorder - graba los comandos de movimiento (viewangles, wishmove, botones, posicion)
// tick a tick y los reproduce despues, con checkpoints periodicos, deteccion de desvio y reanudar
// desde el checkpoint mas cercano. Dibuja el camino grabado y guarda/carga a archivo por mapa.
//
// Portado de un script externo, reescrito en C++ nativo (el original guardaba cada frame en
// "shared vars" del runtime Lua; aca es un vector en memoria del proceso, mucho mas simple). Se
// respetan el esquema de campos por frame y la logica de checkpoints/desvio/reanudar del original.
//
// Hotkeys (por defecto): F5 grabar on/off, F6 reproducir on/off, F7 cargar, F8 guardar,
// R (mantener) reanudar desde checkpoint cercano cuando se activo el control manual por desvio.
namespace Features::MiscTools::MovementRecorder
{
	inline bool  bEnabled = false;

	inline int   nCheckpointInterval = 14;    // frames entre checkpoints
	inline float flResumeDistance = 100.f;    // radio para reanudar desde un checkpoint
	inline float flDeviationThreshold = 150.f;// desvio que activa el control manual
	inline bool  bShowPath = true;
	inline bool  bShowCheckpoints = true;
	inline bool  bShowStatus = true;

	// Estado, para el menu.
	bool IsRecording();
	bool IsPlaying();
	int  FrameCount();
	int  CheckpointCount();

	// Acciones (botones del menu / hotkeys).
	void StartRecording();
	void StopRecording();
	void StartPlayback();
	void StopPlayback();
	void ClearAll();
	void SaveToFile();
	void LoadFromFile();

	// Se llama una vez por comando desde Copy_Command. Maneja hotkeys, graba o reproduce.
	void Run(CUserCmd *pCmd, C_TerrorPlayer *pLocal);

	// Dibuja camino / checkpoints / estado. Se llama desde el hook de Paint.
	void Paint();
}
