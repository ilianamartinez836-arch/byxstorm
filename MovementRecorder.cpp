#include "MovementRecorder.h"
#include "../../Misc/Binds/Binds.h"
#include "../../../../Utils/DebugLog.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace Features::MiscTools::MovementRecorder
{
	struct Frame
	{
		float viewX = 0.f, viewY = 0.f;
		float forward = 0.f, side = 0.f, up = 0.f;
		int   buttons = 0;
		int   impulse = 0;
		int   mousedx = 0, mousedy = 0;
		Vec3  pos = {};
		bool  bCheckpoint = false;
		int   checkpointId = -1;
	};

	struct Checkpoint
	{
		int  frameIndex = 0;
		Vec3 pos = {};
		int  id = 0;
	};

	static std::vector<Frame>      s_frames;
	static std::vector<Checkpoint> s_checkpoints;

	static bool s_bRecording = false;
	static bool s_bPlaying = false;
	static bool s_bManual = false;
	static int  s_nCurrentFrame = 0;             // indice 0-based del proximo frame a reproducir
	static int  s_nFramesSinceCheckpoint = 0;
	static int  s_nNextCheckpointId = 0;

	// ── Estado publico ────────────────────────────────────────────────────────
	bool IsRecording() { return s_bRecording; }
	bool IsPlaying()   { return s_bPlaying; }
	int  FrameCount()  { return static_cast<int>(s_frames.size()); }
	int  CheckpointCount() { return static_cast<int>(s_checkpoints.size()); }

	// ── Utilidades ──────────────────────────────────────────────────────────────
	static std::string MapName()
	{
		if (I::EngineClient && I::EngineClient->IsInGame())
		{
			const char *p = I::EngineClient->GetLevelNameShort();
			if (p && p[0])
				return p;
		}
		return "unknown";
	}

	static std::filesystem::path FilePath()
	{
		return std::filesystem::path(std::filesystem::current_path())
			/ "ByxStorm" / "recordings" / (MapName() + ".dat");
	}

	static float Dist(const Vec3 &a, const Vec3 &b)
	{
		const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	// ── Acciones ──────────────────────────────────────────────────────────────
	void ClearAll()
	{
		s_frames.clear();
		s_checkpoints.clear();
		s_nCurrentFrame = 0;
		s_nFramesSinceCheckpoint = 0;
		s_nNextCheckpointId = 0;
	}

	void StartRecording()
	{
		if (s_bRecording || s_bPlaying)
			return;
		ClearAll();
		s_bRecording = true;
	}

	void StopRecording()
	{
		if (!s_bRecording)
			return;
		s_bRecording = false;
		SaveToFile();
	}

	void StartPlayback()
	{
		if (s_bRecording || s_bPlaying || s_frames.empty())
			return;
		s_bPlaying = true;
		s_bManual = false;
		s_nCurrentFrame = 0;
	}

	void StopPlayback()
	{
		if (!s_bPlaying)
			return;
		s_bPlaying = false;
		s_bManual = false;
		s_nCurrentFrame = 0;
	}

	// ── Guardado / carga ──────────────────────────────────────────────────────
	void SaveToFile()
	{
		if (s_frames.empty())
			return;

		const auto path = FilePath();
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f)
		{
			Debug::Log("[Byx][Recorder] no se pudo abrir para guardar: %s\n", path.string().c_str());
			return;
		}

		f << "frames " << s_frames.size() << "\n";
		for (const auto &fr : s_frames)
		{
			f << fr.viewX << ' ' << fr.viewY << ' '
			  << fr.forward << ' ' << fr.side << ' ' << fr.up << ' '
			  << fr.buttons << ' ' << fr.impulse << ' '
			  << fr.mousedx << ' ' << fr.mousedy << ' '
			  << fr.pos.x << ' ' << fr.pos.y << ' ' << fr.pos.z << ' '
			  << (fr.bCheckpoint ? 1 : 0) << ' ' << fr.checkpointId << "\n";
		}
		f << "checkpoints " << s_checkpoints.size() << "\n";
		for (const auto &cp : s_checkpoints)
			f << cp.frameIndex << ' ' << cp.pos.x << ' ' << cp.pos.y << ' ' << cp.pos.z << ' ' << cp.id << "\n";

		Debug::Log("[Byx][Recorder] guardado %zu frames / %zu checkpoints en %s\n",
			s_frames.size(), s_checkpoints.size(), path.string().c_str());
	}

	void LoadFromFile()
	{
		if (s_bRecording || s_bPlaying)
			return;

		const auto path = FilePath();
		std::ifstream f(path, std::ios::binary);
		if (!f)
		{
			Debug::Log("[Byx][Recorder] no hay grabacion para este mapa: %s\n", path.string().c_str());
			return;
		}

		ClearAll();

		std::string tag;
		size_t n = 0;

		if (!(f >> tag >> n) || tag != "frames")
			return;
		s_frames.reserve(n);
		for (size_t i = 0; i < n; i++)
		{
			Frame fr;
			int cp = 0;
			if (!(f >> fr.viewX >> fr.viewY >> fr.forward >> fr.side >> fr.up
			        >> fr.buttons >> fr.impulse >> fr.mousedx >> fr.mousedy
			        >> fr.pos.x >> fr.pos.y >> fr.pos.z >> cp >> fr.checkpointId))
				break;
			fr.bCheckpoint = cp != 0;
			s_frames.push_back(fr);
		}

		if (f >> tag >> n && tag == "checkpoints")
		{
			s_checkpoints.reserve(n);
			for (size_t i = 0; i < n; i++)
			{
				Checkpoint c;
				if (!(f >> c.frameIndex >> c.pos.x >> c.pos.y >> c.pos.z >> c.id))
					break;
				s_checkpoints.push_back(c);
				if (c.id >= s_nNextCheckpointId)
					s_nNextCheckpointId = c.id + 1;
			}
		}

		Debug::Log("[Byx][Recorder] cargado %zu frames / %zu checkpoints\n",
			s_frames.size(), s_checkpoints.size());
	}

	// ── Grabacion ────────────────────────────────────────────────────────────
	static void RecordFrame(CUserCmd *pCmd, C_TerrorPlayer *pLocal)
	{
		Frame fr;
		fr.viewX = pCmd->viewangles.x;
		fr.viewY = pCmd->viewangles.y;
		fr.forward = pCmd->forwardmove;
		fr.side = pCmd->sidemove;
		fr.up = pCmd->upmove;
		fr.buttons = pCmd->buttons;
		fr.impulse = pCmd->impulse;
		fr.mousedx = pCmd->mousedx;
		fr.mousedy = pCmd->mousedy;
		fr.pos = pLocal->m_vecOrigin();

		s_nFramesSinceCheckpoint++;
		if (s_nFramesSinceCheckpoint >= nCheckpointInterval)
		{
			fr.bCheckpoint = true;
			fr.checkpointId = s_nNextCheckpointId++;
			Checkpoint cp;
			cp.frameIndex = static_cast<int>(s_frames.size()); // indice del frame que estamos por push
			cp.pos = fr.pos;
			cp.id = fr.checkpointId;
			s_checkpoints.push_back(cp);
			s_nFramesSinceCheckpoint = 0;
		}

		s_frames.push_back(fr);
	}

	// ── Reproduccion ──────────────────────────────────────────────────────────
	static void ReplayFrame(CUserCmd *pCmd, const Frame &fr)
	{
		pCmd->viewangles.x = fr.viewX;
		pCmd->viewangles.y = fr.viewY;
		pCmd->viewangles.z = 0.f;
		pCmd->forwardmove = fr.forward;
		pCmd->sidemove = fr.side;
		pCmd->upmove = fr.up;
		pCmd->buttons = fr.buttons;
		pCmd->impulse = static_cast<unsigned char>(fr.impulse);
		pCmd->mousedx = static_cast<short>(fr.mousedx);
		pCmd->mousedy = static_cast<short>(fr.mousedy);
	}

	static const Checkpoint *NearestCheckpoint(const Vec3 &pos, float maxDist)
	{
		const Checkpoint *best = nullptr;
		float bestDist = maxDist;
		for (const auto &cp : s_checkpoints)
		{
			const float d = Dist(pos, cp.pos);
			if (d < bestDist)
			{
				bestDist = d;
				best = &cp;
			}
		}
		return best;
	}

	// ── Hotkeys ────────────────────────────────────────────────────────────────
	// Pasan por Features::Binds::Down y no por GetAsyncKeyState crudo. Era la unica feature
	// con teclas que se lo salteaba sin decir por que (el Teclado Virtual tambien lo hace,
	// pero ahi es a proposito y esta documentado): F5-F8 disparaban con el menu del cheat
	// abierto, escribiendo en el chat, o con el juego en segundo plano - exactamente lo que
	// el gate se agrego para evitar.
	static bool KeyPressed(int vk)
	{
		static bool s_prev[256] = {};
		if (vk < 0 || vk > 255)
			return false;
		const bool down = Features::Binds::Down(vk);
		const bool pressed = down && !s_prev[vk];
		s_prev[vk] = down;
		return pressed;
	}

	// ── Bucle principal ──────────────────────────────────────────────────────
	void Run(CUserCmd *pCmd, C_TerrorPlayer *pLocal)
	{
		if (!bEnabled || !pCmd || !pLocal)
			return;

		if (KeyPressed(VK_F5)) { s_bRecording ? StopRecording() : StartRecording(); }
		if (KeyPressed(VK_F6)) { s_bPlaying   ? StopPlayback()  : StartPlayback();  }
		if (KeyPressed(VK_F7)) LoadFromFile();
		if (KeyPressed(VK_F8)) SaveToFile();

		if (pLocal->m_lifeState() != 0)
			return;

		if (s_bRecording)
		{
			RecordFrame(pCmd, pLocal);
			return;
		}

		if (!s_bPlaying || s_frames.empty())
			return;

		// Reanudar desde checkpoint mientras se mantiene R en control manual.
		if (s_bManual && Features::Binds::Down('R'))
		{
			if (const Checkpoint *cp = NearestCheckpoint(pLocal->m_vecOrigin(), flResumeDistance))
			{
				s_nCurrentFrame = cp->frameIndex;
				s_bManual = false;
			}
		}

		if (s_bManual)
			return;

		if (s_nCurrentFrame < 0 || s_nCurrentFrame >= (int)s_frames.size())
			s_nCurrentFrame = 0;

		const Frame &fr = s_frames[s_nCurrentFrame];

		// Deteccion de desvio: si nos alejamos mucho del frame que toca, control manual.
		if (Dist(pLocal->m_vecOrigin(), fr.pos) > flDeviationThreshold)
		{
			s_bManual = true;
			return;
		}

		ReplayFrame(pCmd, fr);

		s_nCurrentFrame++;
		if (s_nCurrentFrame >= (int)s_frames.size())
			s_nCurrentFrame = 0; // loop
	}

	// ── Dibujo ───────────────────────────────────────────────────────────────
	void Paint()
	{
		if (!bEnabled)
			return;

		if (bShowPath && s_frames.size() > 1)
		{
			const int step = 4;
			Vec3 prevScreen;
			bool bHavePrev = false;
			for (size_t i = 0; i < s_frames.size(); i += step)
			{
				Vec3 screen;
				if (H::Draw->W2S(s_frames[i].pos, screen))
				{
					if (bHavePrev)
						H::Draw->Line((int)prevScreen.x, (int)prevScreen.y,
							(int)screen.x, (int)screen.y, Color_t(255, 105, 180, 90));
					prevScreen = screen;
					bHavePrev = true;
				}
				else
				{
					bHavePrev = false;
				}
			}
		}

		if (bShowCheckpoints)
		{
			for (const auto &cp : s_checkpoints)
			{
				Vec3 screen;
				if (H::Draw->W2S(cp.pos, screen))
				{
					const bool bPassed = s_bPlaying && cp.frameIndex < s_nCurrentFrame;
					const Color_t c = bPassed ? Color_t(130, 130, 130, 110)
						: Color_t(255, 105, 180, 180);
					H::Draw->FilledCircle((int)screen.x, (int)screen.y, 3, 12, c);
					H::Draw->OutlinedCircle((int)screen.x, (int)screen.y, 3, 12, Color_t(255, 255, 255, 130));
				}
			}
		}

		// Marcador del frame actual en reproduccion.
		if (s_bPlaying && s_nCurrentFrame >= 0 && s_nCurrentFrame < (int)s_frames.size())
		{
			Vec3 screen;
			if (H::Draw->W2S(s_frames[s_nCurrentFrame].pos, screen))
			{
				H::Draw->FilledCircle((int)screen.x, (int)screen.y, 4, 16, Color_t(0, 255, 255, 200));
				H::Draw->OutlinedCircle((int)screen.x, (int)screen.y, 4, 16, Color_t(255, 255, 255, 255));
			}
		}

		if (!bShowStatus || (!s_bRecording && !s_bPlaying))
			return;

		const int x = H::Draw->GetScreenW() - 300;
		int y = 24;
		H::Draw->Rect(x, y, 285, 66, Color_t(20, 20, 20, 235));
		H::Draw->OutlinedRect(x, y, 285, 66, Color_t(100, 100, 100, 255));
		H::Draw->String(H::Fonts->Get(EFonts::ESP), x + 8, y + 6, Color_t(255, 255, 255, 255),
			POS_LEFT, "Movement Recorder");

		if (s_bRecording)
			H::Draw->String(H::Fonts->Get(EFonts::ESP), x + 8, y + 24, Color_t(255, 60, 60, 255),
				POS_LEFT, "[GRABANDO]  frames: %d", (int)s_frames.size());
		else if (s_bManual)
			H::Draw->String(H::Fonts->Get(EFonts::ESP), x + 8, y + 24, Color_t(255, 230, 60, 255),
				POS_LEFT, "[MANUAL]  mantene R cerca de un checkpoint");
		else
			H::Draw->String(H::Fonts->Get(EFonts::ESP), x + 8, y + 24, Color_t(60, 255, 90, 255),
				POS_LEFT, "[REPRODUCIENDO]  %d / %d", s_nCurrentFrame, (int)s_frames.size());

		H::Draw->String(H::Fonts->Get(EFonts::ESP), x + 8, y + 44, Color_t(200, 200, 200, 255),
			POS_LEFT, "checkpoints: %d  (cada %d frames)", (int)s_checkpoints.size(), nCheckpointInterval);
	}
}
