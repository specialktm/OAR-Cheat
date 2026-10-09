#include "pch/pch.h"
#include "ui/tabs.hpp"
#include "ui/widgets.hpp"
#include "ui/menu.hpp"
#include "config/config.hpp"
#include "world/actor_iter.hpp"
#include "features/manager.hpp"
#include "features/shop.hpp"
#include "features/modelview.hpp"
#include "SDK/SteamInventoryLibrary_classes.hpp"

#include "SDK/Basic.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/PlayerCharacter_classes.hpp"
#include "SDK/RobberController_classes.hpp"
#include "SDK/ShopItem_Base_classes.hpp"

namespace {

struct PStats { int hp = 0, maxhp = 100, cash = 0, coins = 0, level = 0; bool ok = false; };
static bool seh_read_stats(SDK::APlayerCharacter_C* p, SDK::ARobberController_C* rc, PStats& o) {
    __try {
        if (p) { o.hp = p->Health; o.maxhp = p->MaxHealth; }
        if (rc) { o.cash = rc->Cash; o.coins = rc->Coins; o.level = rc->Level; }
        o.ok = true; return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_write_stats(SDK::APlayerCharacter_C* p, SDK::ARobberController_C* rc,
        int hp, int maxhp, int cash, int coins, int level) {
    __try {
        if (p) { p->Health = hp; p->MaxHealth = maxhp; p->AddedArmorHP_ = (float)maxhp; }
        if (rc) { rc->Cash = cash; rc->Coins = coins; rc->Level = level; }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static bool seh_save_stats(SDK::ARobberController_C* rc) {
    __try { if (rc) { rc->SaveCash(); rc->SaveLevel(); } return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static SDK::APlayerCharacter_C* seh_pawn() {
    __try { return reinterpret_cast<SDK::APlayerCharacter_C*>(actors::local_pawn()); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static int seh_coin_price(SDK::UClass* cls) {
    __try {
        auto* world = static_cast<SDK::UObject*>(actors::game_world());
        if (!IS_VALID_PTR(world)) return -1;
        int price = 0;
        SDK::USteamInventoryLibrary_C::GetDefaultObj()->Get_Steam_Item_Coin_Price(cls, world, &price);
        return price;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static SDK::ARobberController_C* seh_rc() {
    __try { return reinterpret_cast<SDK::ARobberController_C*>(actors::local_player_controller()); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static std::vector<std::string> g_inv;
static uint64_t g_invTs = 0;
static char g_filter[64] = { 0 };
static int g_hp = 100, g_maxhp = 100, g_cash = 0, g_coins = 0, g_level = 1;
static bool g_statsLoaded = false;

static std::string short_inv(const std::string& n) {
    std::string d = n;
    const char* pre = "Default__";
    if (d.rfind(pre, 0) == 0) d = d.substr(8);
    if (d.size() > 2 && d.substr(d.size() - 2) == "_C")
        d = d.substr(0, d.size() - 2);
    while (!d.empty() && d.front() == '_') d.erase(d.begin());
    if (d.empty()) d = n;
    return d;
}

void draw_player() {
    widgets::header_card("Players", "Stats and inventory");
    auto* pawn = seh_pawn();
    auto* rc = seh_rc();
    bool inGame = IS_VALID_PTR(pawn) || IS_VALID_PTR(rc);
    if (!inGame) {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.30f, 1.0f), "No local player - join a lobby first.");
        return;
    }

    widgets::os_section("Stats", ImVec2(400, 500));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        if (!g_statsLoaded) {
            PStats s{};
            if (seh_read_stats(pawn, rc, s) && s.ok) {
                g_hp = s.hp; g_maxhp = s.maxhp; g_cash = s.cash; g_coins = s.coins; g_level = s.level;
                g_statsLoaded = true;
            }
        }
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt("Health", &g_hp, 1000, 100000);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt("Max HP", &g_maxhp, 1000, 100000);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt("Cash", &g_cash, 1000, 1000000);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt("Coins", &g_coins, 10, 1000);
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputInt("Level", &g_level, 1, 10);
        if (g_maxhp <= 0) g_maxhp = 100;
        if (ImGui::Button("APPLY STATS", ImVec2(-1, 30))) {
            int hp = g_hp, mx = g_maxhp, ca = g_cash, co = g_coins, lv = g_level;
            feat::queue_keyed("apply_stats", [hp, mx, ca, co, lv] {
                auto* p = seh_pawn();
                auto* r = seh_rc();
                seh_write_stats(p, r, hp, mx, ca, co, lv);
                seh_save_stats(r);
            });
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("write + save");
        widgets::check_bridge("Godmode", g_cfg.godMode);
        if (ImGui::Button("REFETCH", ImVec2(-1, 30)))
            g_statsLoaded = false;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("read from game");
    }
    widgets::os_section_end();
    ImGui::SetCursorPos(ImVec2(420, 120));
    widgets::os_section("Selected", ImVec2(400, 500));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        const auto& cat2 = shop::catalog(false);
        int sel2 = shop::selected();
        if (sel2 >= 0 && sel2 < (int)cat2.size()) {
            const auto& it = cat2[sel2];
            ImGui::TextWrapped("%s", short_inv(it.name).c_str());
            ImGui::TextDisabled("cash $%d  coins %d", it.cash, it.coin);
            ImGui::TextDisabled("unlock L%d  def %d", it.level, it.defid);
            static int priceSel = -2;
            static int price = -1;
            if (priceSel != sel2) { priceSel = sel2; price = -1; }
            if (price < 0 && IS_VALID_PTR(it.cls))
                price = seh_coin_price(static_cast<SDK::UClass*>(it.cls));
            if (price >= 0)
                ImGui::TextDisabled("steam price: %d coins", price);
            if (ImGui::Button("GRANT", ImVec2(-1, 30))) {
                void* cls = it.cls;
                feat::queue_keyed("grant_class", [cls] { shop::grant_class(cls); });
            }
            ImVec2 o = ImGui::GetCursorScreenPos();
            ImVec2 sz(ImGui::GetContentRegionAvail().x, 150);
            modelview::draw_live_model(o, sz);
            ImGui::Dummy(ImVec2(sz.x, sz.y));
        } else {
            ImGui::TextDisabled("pick an item below");
        }
    }
    widgets::os_section_end();

    ImGui::SetCursorPos(ImVec2(0, 640));
    widgets::os_section("Inventory", ImVec2(800, 420));
    {
        ImGui::SetWindowFontScale(dpi_scale);
        static int seenGrants = -1;
        int gc = shop::grant_count();
        if (seenGrants >= 0 && gc != seenGrants) shop::catalog(true);
        seenGrants = gc;
        const std::vector<shop::ItemInfo>& cat = shop::catalog(false);
        uint64_t now = GetTickCount64();
        if (g_inv.empty() || now - g_invTs > 4000) {
            g_inv = shop::inventory_names();
            g_invTs = now;
        }
        int sel = shop::selected();
        ImGui::InputText("Filter", g_filter, sizeof(g_filter));
        if (ImGui::Button("REFRESH", ImVec2(-1, 26)))
            feat::queue_fn([] { shop::catalog(true); });
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("reload catalog");
        std::string flt = g_filter;
        for (auto& ch : flt) ch = (char)tolower(ch);

        ImGui::BeginChild("All Items", ImVec2(280, 232), true);
        for (int i = 0; i < (int)cat.size(); ++i) {
            const auto& it = cat[i];
            if (!flt.empty()) {
                std::string n = it.name;
                for (auto& ch : n) ch = (char)tolower(ch);
                if (n.find(flt) == std::string::npos) continue;
            }
            std::string disp = short_inv(it.name);
            ImGui::PushID(i);
            if (ImGui::Selectable(disp.c_str(), sel == i, ImGuiSelectableFlags_AllowDoubleClick))
                shop::set_selected(i);
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip("%s", disp.c_str());
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                void* cls = it.cls;
                ImGui::SetDragDropPayload("OARCLS", &cls, sizeof(void*));
                ImGui::Text("Grant %s", disp.c_str());
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::SameLine(0, 8);
        ImGui::BeginChild("Current", ImVec2(0, 232), true);
        char chead[64];
        snprintf(chead, sizeof(chead), "CURRENT (%d) - drop here", (int)g_inv.size());
        ImGui::TextDisabled("%s", chead);
        ImGui::Separator();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("OARCLS")) {
                void* cls = nullptr;
                memcpy(&cls, pl->Data, sizeof(void*));
                feat::queue_fn([cls] { shop::grant_class(cls); });
            }
            ImGui::EndDragDropTarget();
        }
        for (auto& n : g_inv) {
            std::string d = short_inv(n);
            ImGui::BulletText("%s", d.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", d.c_str());
        }
        ImGui::EndChild();
    }
    widgets::os_section_end();
}
struct Reg { Reg() { tabs::add({ "player", "Players", draw_player }); } };
static Reg g_reg;
}
