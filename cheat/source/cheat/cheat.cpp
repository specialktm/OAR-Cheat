#include <pch/pch.h>
#include "cheat.h"
#include "base/Initialize.hpp"
#include <random>
#include <SDK/ShopItem_Map_FIA_classes.hpp>
#include <SDK/ShopItem_Mask_base_classes.hpp>
#include <SDK/PlayerCharacter_classes.hpp>
#include <SDK/ShopBuyUI_classes.hpp>
#include <SDK/ShopUI_classes.hpp>
#include <SDK/Money_base_classes.hpp>

#include <SDK/RobberTruck_classes.hpp>
#include <SDK/Item_Keycard_classes.hpp>
#include <SDK/GuardPhone_classes.hpp>
#include <SDK/CameraBP_classes.hpp>
#include <SDK/Tool_Camerahack_classes.hpp>
#include <SDK/DoorBP_classes.hpp>
#include <SDK/GunBase_classes.hpp>
#include <SDK/Shop_SkillBase_classes.hpp>

using namespace SDK;
bool esp = false;
void cheat::Initialize()
{

}
/*
*
* #include <SDK/ShopItem_Map_CarMansion_classes.hpp>
#include <SDK/ShopItem_Map_Museum_classes.hpp>
#include <SDK/ShopItem_Map_OrbitalBank_classes.hpp>
#include <SDK/ShopItem_Map_Sciencelab_classes.hpp>
#include <SDK/ShopItem_Map_Small_Bank_classes.hpp>
#include <SDK/Setup_map_Jewellery1_classes.hpp>
#include <SDK/ShopItem_Map_WineShop_classes.hpp>
#include <SDK/ShopItem_Map_EscoBar_classes.hpp>
#include <SDK/ShopItem_Map_BlackDiamondCasino_classes.hpp>

std::unordered_map<const char*, SDK::UClass*> maps
{
	{"Esco Bar", AShopItem_Map_EscoBar_C::StaticClass()},
	{"Black Diamond Casino", AShopItem_Map_BlackDiamondCasino_C::StaticClass()},
	{"FIA Office", AShopItem_Map_FIA_C::StaticClass()},
	{"Mansion", AShopItem_Map_CarMansion_C::StaticClass()},
	{"Museum", AShopItem_Map_Museum_C::StaticClass()},
	{"Orbital Bank", AShopItem_Map_OrbitalBank_C::StaticClass()},
	{"Science Lab", AShopItem_Map_Sciencelab_C::StaticClass()},
	{"Small Bank", AShopItem_Map_Small_Bank_C::StaticClass()},
	{"Wine Store", AShopItem_Map_WineShop_C::StaticClass()},
	{"Misc Map", ASetup_map_Jewellery1_C::StaticClass()},
};
*/

void printFvector(const FVector& l)
{
	g_logger->send(levels::debug, "X {} Y {} Z {}", l.X, l.Y, l.Z);
}

void cheat::tick()
{
	if (GetAsyncKeyState(VK_F8) & 1)
	{
		for (auto player : g_util.FindActors<ARobberController_C>())
		{
			if (player == nullptr) {
				g_logger->send(levels::error, "Robber is null, skipping.");
				continue;
			}

			for (const auto& map : g_util.FindActors<AShopItem_Map_base_C>())
			{
				if (map == nullptr) {
					g_logger->send(levels::error, "Map item is null, skipping.");
					continue;
				}
				player->AddMap(map->Class);
				g_logger->send(levels::debug, "Unlocked: [{}]", map->GetName());
			}
		}
		g_logger->send(levels::success, "Unlocked All Maps!");

		for (const auto& StoreItem : g_util.FindActors<AShopItem_Base_C>())
		{
			StoreItem->UnlockLevel = 0;
			StoreItem->CashCost = 1;
			StoreItem->CoinCost = 1;
			StoreItem->Rarity = ERarityNum::RarityNum_MAX;
		}

		std::this_thread::sleep_for(400ms);
	}


	if (GetAsyncKeyState(VK_F9) & 1)
	{
		for (const auto& item : g_util.FindActors<AMoney_base_C>())
		{
			for (const auto& player : g_util.FindActors<APlayerCharacter_C>())
			{

				if (item != nullptr && player != nullptr)
				{

					item->K2_SetActorLocation(player->K2_GetActorLocation(), false, nullptr, true);
					
					if (item->Value >= 0 && item->Value <= 100'000'000)
					{
						item->Value = 10;
					}

					item->Value += item->Value * 10;
					item->Exp += item->Exp * 10.f;
			

					player->Health = 100'000'000;
					player->MaxHealth = 100'000'000;
					player->AddedArmorHP_ = 100'000'000;

					g_logger->send(levels::debug, "Item: {} ${} EXP {}", item->GetName(), item->Value, item->Exp);
				}

			}

			std::this_thread::sleep_for(30ms);
		}
		g_logger->send(levels::debug, "Item Count: {}", g_util.FindActors<AMoney_base_C>().size());
	}
	if (GetAsyncKeyState(VK_F10) & 1)
	{
		for (const auto& item2 : g_util.FindActors<AGuardPhone_C>())
		{
			if (item2 != nullptr)
			{
				item2->Active_ = false;
			}
			std::this_thread::sleep_for(30ms);
		}
	}

	if (GetAsyncKeyState(VK_NUMPAD0) & 1)
	{
		for (const auto& cam : g_util.FindActors<ACameraBP_C>())
		{
			if (cam != nullptr)
			{
				if (!cam->Ignored_)
				{
					cam->IgnoreCamera();
					g_logger->send(levels::debug, "Camera: {} disabled.", cam->GetName());
				}
			}
			std::this_thread::sleep_for(30ms);
		}
	}

	if (GetAsyncKeyState(VK_NUMPAD1) & 1)
	{
		for (const auto& door : g_util.FindActors<ADoorBP_C>())
		{
			if (door != nullptr)
			{
				if (door->Locked_ || door->PowerLocked_)
				{
					door->UnlockDoor();
					door->PowerLocked_ = false;
					g_logger->send(levels::debug, "Door: {} unlocked.", door->GetName());
				}
			}
			std::this_thread::sleep_for(30ms);
		}
	}
	if (GetAsyncKeyState(VK_NUMPAD2) & 1)
	{
		for (const auto& weapon : g_util.FindActors<AGunBase_C>())
		{
			if (weapon != nullptr)
			{
				weapon->Damage = 1000.f;
				weapon->MagSize = 1000;
				weapon->BulletsLeft += 10 * weapon->BulletsLeft;
				weapon->RecoilStrength = 0.f;
				weapon->GunKickMultiplier = 0.f;
				weapon->Reload_time = 0.f;
				weapon->Auto_ = true;
				weapon->CanShoot_ = true;

				weapon->CalculatedReloadTime = 0.f;
				weapon->Accuracy = 100.f;
			}
			std::this_thread::sleep_for(30ms);
		}
	}	
	if (GetAsyncKeyState(VK_NUMPAD3) & 1)
	{
		for (const auto& skill : g_util.FindActors<AShop_SkillBase_C>())
		{
			if (skill != nullptr)
			{
				skill->Rarity = ERarityNum::NewEnumerator0;
				skill->CashCost = 1;
				skill->RequiredProgress = 1.f;
			}
			std::this_thread::sleep_for(30ms);
		}
	}
}



void cheat::Uninitialize()
{

}