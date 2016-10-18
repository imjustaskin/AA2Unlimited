#include "HairMeshes.h"

#include <Windows.h>
#include <vector>

#include "Functions\Shared\Globals.h"
#include "Functions\AAPlay\Globals.h"
#include "Files\Config.h"
#include "MemMods\Hook.h"
#include "General\ModuleInfo.h"
#include "External\ExternalClasses\CharacterStruct.h"
#include "Functions\Shared\Overrides.h"

namespace SharedInjections {
namespace HairMeshes {



/*
 * Fired after the hair load function was executed.
 * Return true to repeat function, return false to end.s
 */
bool __stdcall HairLoadAAEdit(BYTE kind, ExtClass::CharacterStruct* character) {
	/*
	 * The general idea is this: this function loads the hair of kind kind (0-3) for the given character. If the
	 * character allready has a xxfile of this hair loaded, its deleted first.
	 * This means we can repeat this function to both load additional hairs, as well as delete old hairs.
	 * Therefor, we will repeat this function as many times as we need hair OR have to delete old hair
	 */
	//static vars, preserved other the calls
	static int currIndex = 0;	//index of the hair that has to be loaded next.
	static int maxIndex;		//max index, max(overridelist.size(), currXXFiles.size())
	static BYTE savedHair;
	static BYTE savedFlip;
	static BYTE savedAdjustment;
	static ExtClass::XXFile* savedPtr; //the original hair pointer
	static std::vector<std::pair<AAUCardData::HairPart,ExtClass::XXFile*>> savedPointers; //hairs we generated
	
	const auto& list = Shared::g_currentChar->m_cardData.GetHairs(kind);
	auto& data = character->m_charData->m_hair;

	if(currIndex == 0) {
		//the first, original hair
		maxIndex = max(list.size(),Shared::g_currentChar->m_hairs[kind].size());
		if (maxIndex == 0) return false; //if no overriden hairs, no need to do anything
		//save properties of first hair
		savedPtr = character->m_xxHairs[kind];
		savedHair = data.hairs[kind];
		savedFlip = data.hairFlips[kind];
		savedAdjustment = data.hairAdjustment[kind];
	}
	else {
		//if this was a generated hair, save the pointer
		if(currIndex-1 < list.size()) { //note that currIndex-1 is the last loaded index
			AAUCardData::HairPart hair{ kind, data.hairs[kind], data.hairAdjustment[kind], data.hairFlips[kind] };
			savedPointers.emplace_back(std::move(hair),character->m_xxHairs[kind]);
		}
		//if done
		if (currIndex >= maxIndex) {
			//we're done
			//save the hairs we generated
			Shared::g_currentChar->m_hairs[kind] = std::move(savedPointers);
			savedPointers.clear();
			//restore original state and gtfo
			data.hairs[kind] = savedHair;
			data.hairAdjustment[kind] = savedAdjustment;
			data.hairFlips[kind] = savedFlip;
			character->m_xxHairs[kind] = savedPtr;
			currIndex = 0;
			return false;
		}
	}
	//the hair in this one will be deleted, so we fill our old hairs in if applicable, or NULL else
	if(currIndex < Shared::g_currentChar->m_hairs[kind].size())  {
		character->m_xxHairs[kind] = Shared::g_currentChar->m_hairs[kind][currIndex].second;
	}
	else {
		character->m_xxHairs[kind] = NULL;
	}
	//if we have to generate another hair, put it in here. if no hair is left and we only have to delete old hairs,
	//put slot 0, flip 1 in here, cause this hair never exists
	if(currIndex < list.size()) {
		data.hairs[kind] = list[currIndex].slot;
		data.hairFlips[kind] = list[currIndex].flip;
		data.hairAdjustment[kind] = list[currIndex].adjustment;
	}
	else {
		data.hairs[kind] = 0;
		data.hairFlips[kind] = 1;
	}
	currIndex++;
	return true;

}

/*
* return false to end, return true to repeat. Currently only fires in AAPlay
*/
bool __stdcall XXCleanupEvent(ExtClass::CharacterStruct* character) {
	static int currIndex = 0;

	bool repeat = false; //if we need to repeat the function to load more hairs
						 //for every hair kind
	for (int kind = 0; kind < 4; kind++) {
		auto& list = AAPlay::GetInstFromStruct(character)->m_hairs[kind];
		if(list.size() > 0) {
			if (currIndex < list.size()) {
				//still have to do something
				repeat = true;
				character->m_xxHairs[kind] = list[currIndex].second;
			}
			else {
				//we're done, clear the list
				list.clear();
			}
		}
	}

	if (repeat) {
		currIndex++;
		return true;
	}
	else {
		
		currIndex = 0;
		return false;
	}

	return false;
}

//CURRENTLY NOT USED, BUT LOGIC MIGHT BE NICE TO HAVE FOR LATER
/*
* Fired after the hair load function was executed.
* Return true to repeat function, return false to end.
*/
bool __stdcall HairLoadAAPlay(ExtClass::CharacterStruct* character) {
	/*
	 * This version does not have a kind. it loads everything. we will have to deal with that.
	 * We also have no cleanup here.
	 */
	static int currIndex = 0;
	static BYTE savedHair[4];
	static BYTE savedFlip[4];
	static BYTE savedAdjustment[4];
	static ExtClass::XXFile* savedPtr[4]; //the original hair pointer
	static std::vector<std::pair<AAUCardData::HairPart,ExtClass::XXFile*>> savedPointers[4]; //hairs we generated
	static bool done[4]; //what kinds we are done with

	if(currIndex == 0) {
		for (int i = 0; i < 4; i++) done[i] = Shared::g_currentChar->m_cardData.GetHairs(i).empty();
	}

	bool repeat = false; //if we need to repeat the function to load more hairs
	//for every hair kind
	for(int kind = 0; kind < 4; kind++) {
		if (done[kind]) continue; //done with this one

		const auto& list = Shared::g_currentChar->m_cardData.GetHairs(kind);
		auto& data = character->m_charData->m_hair;

		if(currIndex == 0) {
			//the first, original hair
			savedPtr[kind] = character->m_xxHairs[kind];
			savedHair[kind] = data.hairs[kind];
			savedFlip[kind] = data.hairFlips[kind];
			savedAdjustment[kind] = data.hairAdjustment[kind];
		}
		else {
			//save last hair
			AAUCardData::HairPart hair{ kind, data.hairs[kind], data.hairAdjustment[kind], data.hairFlips[kind] };
			savedPointers[kind].emplace_back(std::move(hair),character->m_xxHairs[kind]);

			//if this was the last hair, mark us as done, restore state and continue
			if(currIndex >= list.size()) {
				done[kind] = true;
				data.hairs[kind] = savedHair[kind];
				data.hairAdjustment[kind] = savedAdjustment[kind];
				data.hairFlips[kind] = savedFlip[kind];
				character->m_xxHairs[kind] = savedPtr[kind];
				continue;
			}
		}

		data.hairs[kind] = list[currIndex].slot;
		data.hairFlips[kind] = list[currIndex].flip;
		data.hairAdjustment[kind] = list[currIndex].adjustment;
		repeat = true;
	}

	if(repeat) {
		currIndex++;
		return true;
	}
	else {
		currIndex = 0;
		return false;
	}
}

DWORD HairLoadFuncStart;
void __declspec(naked) HairLoadRedirectAAEdit() {
	__asm {
		add esp,0x1018

		push eax

		mov eax,[esp+8] //that pointer
		mov eax,[eax+0x24]		//character struct
		movsx edx, byte ptr [esp+0x10] //hair slot
		push eax
		push edx
		call HairLoadAAEdit

		test al,al
		pop eax
		jz HairLoadRedirect_end

		jmp [HairLoadFuncStart]
	HairLoadRedirect_end:
		ret 0x10
	}
}

void __declspec(naked) HairLoadRedirectAAPlay() {
	__asm {
		add esp, 0x4f4

		push eax

		mov eax,[esp+8] //that pointer
		mov eax,[eax+0x24]		//character struct
		push eax
		call HairLoadAAPlay

		test al,al
		pop eax
		jz HairLoadRedirect_end

		jmp[HairLoadFuncStart]
	HairLoadRedirect_end:
		ret 0x0C
	}
}


void HairLoadInject() {
	//these are different functions. The AAEdit version is loading a certain hair slot only,
	//while the AAPlay version loads all of them. Therefor, the AAPlay version needs different handling
	if (General::IsAAEdit) {
		//stack: retVal, [classStruct+2C], someBool, hairSlot, someBool, whereas [[classStruct+2C]+24] == classStruct
		/*AA2Edit.exe+119A10 - 6A FF                 - push -01 { 255 }
		AA2Edit.exe+119A12 - 68 260F6500           - push AA2Edit.exe+2C0F26 { [139] }
		*/
		//...
		/*AA2Edit.exe+11A8EB - 81 C4 18100000        - add esp,00001018 { 4120 }
		AA2Edit.exe+11A8F1 - C2 1000               - ret 0010 { 16 }
		*/
		DWORD address = General::GameBase + 0x11A8EB;
		DWORD redirectAddress = (DWORD)(&HairLoadRedirectAAEdit);
		Hook((BYTE*)address,
			{ 0x81, 0xC4, 0x18, 0x10, 0x00, 0x00 },						//expected values
			{ 0xE9, HookControl::RELATIVE_DWORD, redirectAddress, 0x90 },	//redirect to our function
			NULL);
		HairLoadFuncStart = General::GameBase + 0x119A10;
	}
	else if(General::IsAAPlay) {
		//retVal, [classStruct+2C], someBool, someBool
		/*AA2Play v12 FP v1.4.0a.exe+128CD0 - 6A FF                 - push -01 { 255 }
		AA2Play v12 FP v1.4.0a.exe+128CD2 - 68 EA054300           - push "AA2Play v12 FP v1.4.0a.exe"+2E05EA { [139] }
		*/
		//...
		/*AA2Play v12 FP v1.4.0a.exe+1296B0 - 81 C4 F4040000        - add esp,000004F4 { 1268 }
		AA2Play v12 FP v1.4.0a.exe+1296B6 - C2 0C00               - ret 000C { 12 }
		*/
		/*DWORD address = General::GameBase + 0x1296B0;
		DWORD redirectAddress = (DWORD)(&HairLoadRedirectAAPlay);
		Hook((BYTE*)address,
		{ 0x81, 0xC4, 0xF4, 0x04, 0x00, 0x00 },						//expected values
		{ 0xE9, HookControl::RELATIVE_DWORD, redirectAddress, 0x90 },	//redirect to our function
			NULL);
		HairLoadFuncStart = General::GameBase + 0x128CD0;*/

		/*AA2Play v12 FP v1.4.0a.exe+12B500 - 6A FF                 - push -01 { 255 }
		AA2Play v12 FP v1.4.0a.exe+12B502 - 68 56026900           - push "AA2Play v12 FP v1.4.0a.exe"+2E0256 { [139] }
		*/
		//...
		/*AA2Play v12 FP v1.4.0a.exe+12C3F3 - E8 61D61500           - call "AA2Play v12 FP v1.4.0a.exe"+289A59 { ->AA2Play v12 FP v1.4.0a.exe+289A59 }
		AA2Play v12 FP v1.4.0a.exe+12C3F8 - 81 C4 18100000        - add esp,00001018 { 4120 }
		AA2Play v12 FP v1.4.0a.exe+12C3FE - C2 1000               - ret 0010 { 16 }
		*/
		DWORD address = General::GameBase + 0x12C3F8;
		DWORD redirectAddress = (DWORD)(&HairLoadRedirectAAEdit);
		Hook((BYTE*)address,
			{ 0x81, 0xC4, 0x18, 0x10, 0x00, 0x00 },						//expected values
			{ 0xE9, HookControl::RELATIVE_DWORD, redirectAddress, 0x90 },	//redirect to our function
			NULL);
		HairLoadFuncStart = General::GameBase + 0x12B500;
	}
}

DWORD XXCleanupFuncStart;
void __declspec(naked) XXCleanupRedirect() {
	__asm {
		pop ebx
		add esp, 0x40

		push eax //save return value

		mov edx, [esp+4+8-0x40]
		sub edx, 0x48 //face offest
		push edx //save for the repeat as well
		push edx
		call XXCleanupEvent
		test al,al
		pop ecx
		pop eax
		jz XXCleanupRedirect_end

		jmp[XXCleanupFuncStart]
	XXCleanupRedirect_end:

		ret
	}
}

void XXCleanupInjection() {

	if(General::IsAAPlay) {
		//ecx this call characterStruct, deletes all xx files.
		//at the end (before the esp), no trace of the this pointer is left, but esp+8 should contain an array
		//of pointers to xx files for easier deletion (so it should always be there), starting with the face xx.
		//we can get back the pointer from it
		/*AA2Play v12 FP v1.4.0a.exe+114810 - 83 EC 40              - sub esp,40 { 64 }
		AA2Play v12 FP v1.4.0a.exe+114813 - 53                    - push ebx
		AA2Play v12 FP v1.4.0a.exe+114814 - 55                    - push ebp
		AA2Play v12 FP v1.4.0a.exe+114815 - 56                    - push esi
		AA2Play v12 FP v1.4.0a.exe+114816 - 57                    - push edi
		*/
		//...
		/*AA2Play v12 FP v1.4.0a.exe+114C3D - B0 01                 - mov al,01 { 1 }
		AA2Play v12 FP v1.4.0a.exe+114C3F - 5B                    - pop ebx
		AA2Play v12 FP v1.4.0a.exe+114C40 - 83 C4 40              - add esp,40 { 64 }
		AA2Play v12 FP v1.4.0a.exe+114C43 - C3                    - ret
		*/
		DWORD address = General::GameBase + 0x114C3F;
		DWORD redirectAddress = (DWORD)(&XXCleanupRedirect);
		Hook((BYTE*)address,
		{ 0x5B, 0x83, 0xC4, 0x40, 0xC3 },						//expected values
		{ 0xE9, HookControl::RELATIVE_DWORD, redirectAddress },	//redirect to our function
			NULL);
		XXCleanupFuncStart = General::GameBase + 0x114810;
	}
	
}

void __declspec(naked) HairRefreshRedirect() {
	__asm {

		ret 4
	}
}

void HairRefreshInject() {
	if(General::IsAAEdit) {
		//stack param, esi is this
		/*AA2Edit.exe+FA470 - 55                    - push ebp{ redraws some meshes.flags filter which ones are
		drawn. disabling some will cause animations to stop
		Flags: skelleton, face, tounge, glasses, front hair,
		back hair, side hair, hair extension
		}
		AA2Edit.exe+FA471 - 8B EC                 - mov ebp,esp
		AA2Edit.exe+FA473 - 83 E4 F8              - and esp,-08 { 248 }
		AA2Edit.exe+FA476 - 51                    - push ecx
		AA2Edit.exe+FA477 - 53                    - push ebx
		AA2Edit.exe+FA478 - 8A 5D 08              - mov bl,[ebp+08]*/
		//...
		/*AA2Edit.exe+FA65E - B0 01                 - mov al,01 { 1 }
		AA2Edit.exe+FA660 - 5B                    - pop ebx
		AA2Edit.exe+FA661 - 8B E5                 - mov esp,ebp
		AA2Edit.exe+FA663 - 5D                    - pop ebp
		AA2Edit.exe+FA664 - C2 0400               - ret 0004 { 4 }
		*/
		/*DWORD address = General::GameBase + 0x1F89F0;
		DWORD redirectAddress = (DWORD)(&HairRefreshRedirect);
		Hook((BYTE*)address,
			{ 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 },						//expected values
			{ 0xE9, HookControl::RELATIVE_DWORD, redirectAddress, 0x90 },	//redirect to our function
		NULL);*/
	}
	else if(General::IsAAPlay) {

	}
	
}



}
}