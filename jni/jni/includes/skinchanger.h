#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include "imgui/imgui/imgui.h"

struct SkinItem {
    int id;
    std::string name;
    int rarity;  // 1-7 редкость
    uint64_t ptr;
    
    // Модификации скина
    std::map<int, int> stickers;  // позиция (1-4) -> ID наклейки
    int charm_id = 0;             // ID брелка (0 = нет брелка)
};

// Структура для хранения доступных наклеек и брелков
struct StickerItem {
    int id;
    std::string name;
    int rarity;
};

struct CharmItem {
    int id;
    std::string name;
    int rarity;
};

struct SkinChangerSettings {
    bool enabled = false;
};

inline SkinChangerSettings skinChangerSettings;

class SkinChanger {
private:
    uint64_t libunity_base_;
    std::map<int, std::pair<std::string, int>> all_skins; // id -> {name, rarity}
    std::vector<SkinItem> inventory_skins;
    std::map<int, int> saved_skin_replacements; // index -> new_id
    
    // Наклейки и брелки
    std::vector<StickerItem> all_stickers;
    std::vector<CharmItem> all_charms;
    
    // UI state
    int skin_tab = 0;
    int selected_inv_skin = -1;
    int selected_all_skin_idx = 0;
    std::string search_string;
    std::vector<std::pair<int, std::pair<std::string, int>>> sorted_all_skins;
    std::vector<std::pair<int, std::pair<std::string, int>>> filtered_skins;
    
    // UI state для модификаций
    int selected_sticker_slots[4] = {0, 0, 0, 0}; // Выбранные наклейки для каждого слота
    int selected_charm = 0;                        // Выбранный брелок
    bool sticker_slot_enabled[4] = {false, false, false, false}; // Включены ли слоты
    bool charm_enabled = false;                    // Включен ли брелок
    
    // UI state для поиска в слотах наклеек
    std::string sticker_search_strings[4];         // Поиск для каждого слота наклейки
    std::string charm_search_string;               // Поиск для брелка
    std::vector<StickerItem> filtered_stickers[4]; // Отфильтрованные наклейки для каждого слота
    std::vector<CharmItem> filtered_charms;        // Отфильтрованные брелки
    
    ImU32 GetRarityColor(int rarity);
    void UpdateFilteredSkins();
    
    // Новые функции для работы с модификациями
    void UpdateStickersAndCharms();
    void SetWeaponStickers(uint64_t weapon_data_ptr, const std::map<int, int>& stickers);
    void SetWeaponStickersAlternative(uint64_t weapon_data_ptr, const std::map<int, int>& stickers);
    void SetWeaponStickersDirectly(uint64_t inventory_item_ptr, const std::map<int, int>& stickers);
    void SetWeaponCharm(uint64_t weapon_data_ptr, int charm_id);
    std::map<int, int> GetWeaponStickers(uint64_t weapon_data_ptr);
    int GetWeaponCharm(uint64_t weapon_data_ptr);
    void RenderSkinModifications(float scale, float contentWidth, float padding);
    
    // Функции для рендеринга UI слотов наклеек и брелков
    void RenderStickerSlotUI(int slot_index, float scale, float contentWidth, float padding, float listHeight, float btnHeight);
    void RenderCharmSlotUI(float scale, float contentWidth, float padding, float listHeight, float btnHeight);
    void UpdateStickerFilter(int slot_index);
    void UpdateCharmFilter();
    
    // Статическая функция для чтения Unity строк (нужна для UnityStringDictionary)
    static std::string ReadUnityString(uint64_t str_ptr);

public:
    SkinChanger();
    void Init(uint64_t libunity_base);
    
    void UpdateAllSkins();
    void UpdateInventorySkins();
    void ReplaceSkin(SkinItem& to_swap, int new_id, const std::map<int, int>& stickers = {}, int charm_id = 0);
    void RenderUI();
    
    // Config
    bool SaveConfig(const std::string& name);
    bool LoadConfig(const std::string& name);
    bool DeleteConfig(const std::string& name);
    std::vector<std::string> GetConfigList();
    void ApplySavedSkins();
    
    const std::vector<SkinItem>& GetInventorySkins() const { return inventory_skins; }
    const std::map<int, std::pair<std::string, int>>& GetAllSkins() const { return all_skins; }
};

inline SkinChanger skinChanger;

// Advanced config system functions
void UpdateSkinConfigList();
std::string GetSelectedSkinConfigPath();
void CreateNewSkinConfig(SkinChanger* skinchanger);
void DeleteSelectedSkinConfig();
