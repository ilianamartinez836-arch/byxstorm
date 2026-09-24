#pragma once
#include <unordered_map>
#include <unordered_set>
#include <wtypes.h>
#include "../../SDK/SDK.h"
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

inline wchar_t Write_Character;
inline float Update_Animation_Time;
inline __int8 Consistent_Time;
inline __int8 Update_Animation_Type;
inline __int32 Extra_Commands;
inline __int32 Chainsaw_Cycles;
inline float Perform_Trace_Damage;
inline void* Perform_Trace_Target;
inline std::unordered_map<wchar_t, unsigned __int32[4]> Characters_Bounds;

struct Target_Structure
{
	__int32 Identifier;

	void* Self;

	__int8 Priority;

	float Distance;

	__int32 Tick_Number;
};

inline std::vector<Target_Structure> Sorted_Target_List;

inline void* Get_Studio_Header(void* Entity)
{
	using Get_Studio_Header_Type = void* (__thiscall*)(void* Entity);
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));

	return Get_Studio_Header_Type((unsigned __int32)Client_Module + 8512)(Entity);
}

inline void* Get_Hitbox_Set(Target_Structure* Target, float(*Bones)[3][4], float Time)
{
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));

	using Setup_Bones_Type = __int8(__thiscall*)(void* Entity, void* Bones, __int32 Maximum_Bones, __int32 Mask, float Time);

	if (Setup_Bones_Type((unsigned __int32)Client_Module + 246656)((void*)((unsigned __int32)Target->Self + 4), Bones, 128, 524032, Time) == 1)
	{
		void* Studio_Header = *(void**)Get_Studio_Header(Target->Self);

		return (void*)((unsigned __int32)Studio_Header + *(__int32*)((unsigned __int32)Studio_Header + 176));
	}

	return nullptr;
}
inline char* Get_Sequence_Name(void* Entity)
{
	using Get_Sequence_Name_Type = char* (__thiscall*)(void* Entity, __int32 Sequence);
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));

	return Get_Sequence_Name_Type((unsigned __int32)Client_Module + 203392)(Entity, *(__int32*)((unsigned __int32)Entity + 2212));
}

inline __int32 Get_Identifier(void* Entity, __int8 Raw, __int8 Equipment)
{
	using Get_Identifier_Type = void* (__cdecl**)();
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));

	__int32 Identifier = *(__int32*)((unsigned __int32)(*Get_Identifier_Type(*(unsigned __int32*)((unsigned __int32)Entity + 8) + 4))() + 20);

	if (Raw == 1)
	{
		return Identifier;
	}

	if (*(__int8*)((unsigned __int32)Entity + 221) == 0)
	{
		static std::unordered_set<__int32> Targets = { 0, 13, 99, 232, 263, 264, 265, 270, 272, 275, 276, 277 };

		if (Targets.contains(Identifier) == 1)
		{
			__int8 Valid = 0;

			if (Identifier == 13)
			{
				*(__int32*)((unsigned __int32)Entity + 228) = 1;

				Valid = *(__int8*)((unsigned __int32)Entity + 324) == 5;
			}
			else
			{
				if ((*(__int32*)((unsigned __int32)Entity + 572) - 131088 & 255) == 0)
				{
					if (__builtin_strstr(Get_Sequence_Name(Entity), "eath") == nullptr)
					{
						Valid = Identifier == 264 ? *(__int8*)((unsigned __int32)Entity + 4493) ^ 1 : 1;
					}
				}
			}

			if (Valid == 1)
			{
				if (Identifier * (*(__int32*)((unsigned __int32)Entity + 228) == 3) == 232)
				{
					static std::unordered_map<__int32, __int32> Translators =
					{
						{ 1, 270 },

						{ 2, 0 },

						{ 3, 263 },

						{ 4, 272 },

						{ 5, 265 },

						{ 6, 99 },

						{ 8, 276 }
					};

					Identifier = Translators[*(__int32*)((unsigned __int32)Entity + 7312)];
				}

				return (Identifier - 232) % 43 ? Identifier : 232;
			}
		}
		else
		{
			if (Equipment == 1)
			{
				if ((*(__int32*)((unsigned __int32)Entity + 224) & 32) == 0)
				{
					static std::unordered_set<__int32> Equipment_List = { 73, 105, 109, 121, 256, 260 };

					if (Equipment_List.contains(Identifier) == 1)
					{
						if (Identifier == 260)
						{
							static std::unordered_map<__int32, __int32> Translators =
							{
								{ 12, 73 },

								{ 15, 121 },

								{ 23, 105 },

								{ 24, 109 }
							};

							Identifier = Translators[*(__int32*)((unsigned __int32)Entity + 2392)];
						}

						if (Identifier != 0)
						{
							if (*(void**)((unsigned __int32)Entity + 312) == INVALID_HANDLE_VALUE)
							{
								return -Identifier;
							}
						}
					}
				}
			}
		}
	}

	return -1;
}
struct Global_Variables_Structure
{
	__int8 Additional_Bytes_1[12];

	float Time;

	float Frame_Time;

	__int8 Additional_Bytes_2[8];

	float Interval_Per_Tick;
};

inline float* Get_Center(void* Entity)
{
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));
	using Get_Center_Type = float* (__thiscall*)(void* Entity);

	return Get_Center_Type((unsigned __int32)Client_Module + 114400)(Entity);
}
inline float Vector_Normalize(float* Vector)
{
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));
	using Vector_Normalize_Type = float(__thiscall*)(float* Vector);

	return Vector_Normalize_Type((unsigned __int32)Client_Module + 3536192)(Vector);
};

inline void Angle_Vectors(float* Angles, float* Forward, float* Right, float* Up)
{
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));
	using Angle_Vectors_Type = void(__cdecl*)(float* Angles, float* Forward, float* Right, float* Up);

	Angle_Vectors_Type((unsigned __int32)Client_Module + 3539392)(Angles, Forward, Right, Up);
};
struct Prediction_Copy_Structure
{
	__int8 Additionals_Bytes_1[8];

	void* Destination;

	void* Source;

	__int8 Additional_Bytes_2[48];

	void Construct(void* Destination, void* Source, void* Handler)
	{
		static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
		static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));
		using Construct_Type = void(__fastcall*)(void* Prediction_Copy, void* Unknown_Parameter, __int32 Type, void* Destination, __int8 Destination_Packed, void* Source, __int8 Source_Packed, __int32 Operation_Type, void* Handler);

		Construct_Type((unsigned __int32)Client_Module + 1564512)(this, nullptr, 2, Destination, 1, Source, 0, 3, Handler);
	}
};
inline Prediction_Copy_Structure Predicton_Copy;
struct Prediction_Field_Structure
{
	__int32 Type;

	char* Name;

	__int32 Offset;

	unsigned __int16 Size;

	__int8 Additionals_Bytes_1[18];

	__int32 Bytes;

	__int8 Additionals_Bytes_2[12];

	__int32 Flat_Offset[2];

	__int8 Additionals_Bytes_3[2];
};
inline void Predicton_Copy_Compare(void* Unknown_Parameter_1, void* Unknown_Parameter_2, void* Unknown_Parameter_3, void* Unknown_Parameter_4, void* Unknown_Parameter_5, void* Unknown_Parameter_6, __int8 Within_Tolerance, void* Unknown_Parameter_7)
{
	Prediction_Field_Structure* Field = *(Prediction_Field_Structure**)((unsigned __int32)__builtin_frame_address(0) + 60);

	if (Field->Flat_Offset[0] * Consistent_Time == 5324)
	{
		goto Copy_Label;
	}

	if (Within_Tolerance == 1)
	{
		if ((256 - Field->Flat_Offset[0] ^ Field->Flat_Offset[0] - 244) != 12)
		{
		Copy_Label:
			{
				Byte_Manager::Copy_Bytes(1, (void*)((unsigned __int32)Predicton_Copy.Destination + Field->Flat_Offset[0]), Field->Bytes, (void*)((unsigned __int32)Predicton_Copy.Source + Field->Flat_Offset[1]));
			}
		}
	}
}
struct Prediction_Descriptor_Structure
{
	Prediction_Field_Structure* Fields;

	__int32 Size;

	__int8 Additional_Bytes_1[4];

	Prediction_Descriptor_Structure* Parent;

	__int8 Additional_Bytes_2[8];
};


inline void Redirected_Update_Animations()
{
	static auto Client_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"client.dll"));
	static auto Engine_Module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"engine.dll"));
	Global_Variables_Structure* Global_Variables = *(Global_Variables_Structure**)((unsigned __int32)Client_Module + 7096744);

	float Previous_Time = Global_Variables->Time;

	Global_Variables->Time = Update_Animation_Time;

	float Previous_Frame_Time = Global_Variables->Frame_Time;

	Global_Variables->Frame_Time = Global_Variables->Interval_Per_Tick * Update_Animation_Type;

	__int32 Entity_Number = 0;

Traverse_Animation_List_Label:
	{
		if (Entity_Number != *(__int32*)((unsigned __int32)Client_Module + 7479624))
		{
			void* Animation_List = *(void**)((unsigned __int32)Client_Module + 7479612);

			if ((*(__int8*)((unsigned __int32)Animation_List + 8 * Entity_Number + 4) & 1) == 1)
			{
				using Update_Animation_Type = void(__thiscall**)(void* Entity);

				void* Entity = *(void**)((unsigned __int32)Animation_List + 8 * Entity_Number);

				*(float*)((unsigned __int32)Entity + 328) = Update_Animation_Time - Global_Variables->Frame_Time;

				(*Update_Animation_Type(*(unsigned __int32*)Entity + 808))(Entity);
			}

			Entity_Number += 1;

			goto Traverse_Animation_List_Label;
		}
	}

	Global_Variables->Frame_Time = Previous_Frame_Time;

	Global_Variables->Time = Previous_Time;
}

struct Command_Structure
{
	__int8 Additional_Bytes_1[4];

	__int32 Command_Number;

	__int32 Tick_Number;

	float Angles[3];

	float Move[3];

	__int32 Buttons;

	__int8 Additional_Bytes_2[9];

	__int32 Random_Seed;
};

struct Extended_Command_Structure
{
	__int32 Extra_Commands;

	__int32 Sequence_Shift;
};

inline Extended_Command_Structure Extended_Commands[150];

