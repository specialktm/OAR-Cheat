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
void cheat::tick()
{
	if (GetAsyncKeyState(VK_F8) & 1)
	{

	

		for (auto robber : g_util.FindActors<ARobberController_C>())
		{
			if (robber == nullptr) {
				g_logger->send(levels::error, "Robber is null, skipping.");
				continue;
			}

			for (const auto& rr : g_util.FindActors<AShopItem_Map_base_C>())
			{
				if (rr == nullptr) {
					g_logger->send(levels::error, "Map item is null, skipping.");
					continue;
				}
				robber->AddMap(rr->Class);
				g_logger->send(levels::debug, "Unlocked: [{}]", rr->GetName());
			}	
		}
		g_logger->send(levels::success, "Unlocked All Maps!");

		for (const auto& mask : g_util.FindActors<AShopItem_Base_C>())
		{
			mask->UnlockLevel = 0;
			mask->CashCost = 1;
			mask->CoinCost = 0;	
	
		}

		std::this_thread::sleep_for(400ms);
	}
	if (GetAsyncKeyState(VK_F9) & 1)
	{
		for (const auto& item : g_util.FindActors<APickupItem_base_C>())
		{
			for (const auto& player : g_util.FindActors<APlayerCharacter_C>())
			{
				item->K2_SetActorLocation(FVector{ player->K2_GetActorLocation().X,player->K2_GetActorLocation().Y, player->K2_GetActorLocation().Z - 3.f }, false, nullptr, true);
			}
			std::this_thread::sleep_for(30ms);
		}
	}
}



void cheat::Uninitialize()
{

}