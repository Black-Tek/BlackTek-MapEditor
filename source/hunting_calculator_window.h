//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Hunting Calculator Window - Analyzes hunting potential for selected areas
//////////////////////////////////////////////////////////////////////

#ifndef RME_HUNTING_CALCULATOR_WINDOW_H_
#define RME_HUNTING_CALCULATOR_WINDOW_H_

#include "ext/pugixml.hpp"
#include "outfit.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <wx/filepicker.h>
#include <wx/gauge.h>
#include <wx/spinctrl.h>
#include <wx/vlbox.h>
#include <wx/wx.h>

// Forward declarations
class Editor;
class HuntingCalculatorWindow;

// Monster data structure for hunting calculations
struct HuntingMonsterData {
  std::string name;
  Outfit outfit; // Full outfit for proper sprite rendering with colors
  uint64_t experience = 0;
  int32_t health = 100;
  int32_t armor = 0;
  int32_t defense = 0;
  int count = 0; // Count in selected area

  // Calculated values (filled during calculation)
  double respawnTime = 600.0;
  double killsPerHour = 0.0;
  double expPerHour = 0.0;

  // Loot data
  struct LootItem {
    std::string name;
    uint16_t id = 0;
    uint32_t chance = 0; // Out of 100000
    uint32_t countmax = 1;
  };
  std::vector<LootItem> loot;
};

// Aggregated loot result
struct AggregatedLoot {
  std::string name;
  uint16_t id = 0;
  double expectedCount = 0.0;
  double dropRate = 0.0; // Percentage
};

// Config.lua multipliers
struct ServerConfig {
  double rateExp = 1.0;
  double rateLoot = 1.0;
  double rateSpawn = 1.0;
  double rateSkill = 1.0;
  double rateMagic = 1.0;
  bool loaded = false;
};

// Custom list box for monsters with sprites
class MonsterListBox : public wxVListBox {
public:
  MonsterListBox(wxWindow *parent, wxWindowID id);
  void SetMonsters(const std::vector<HuntingMonsterData> *monsters);

protected:
  void OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const override;
  wxCoord OnMeasureItem(size_t n) const override;

private:
  const std::vector<HuntingMonsterData> *m_monsters;
};

// Custom list box for loot with sprites and right-click context menu
class LootListBox : public wxVListBox {
public:
  LootListBox(wxWindow *parent, wxWindowID id,
              HuntingCalculatorWindow *calculator);
  void SetLoot(const std::vector<AggregatedLoot> *loot);

protected:
  void OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const override;
  wxCoord OnMeasureItem(size_t n) const override;
  void OnRightClick(wxMouseEvent &event);
  void OnShowExpectedTime(wxCommandEvent &event);

private:
  const std::vector<AggregatedLoot> *m_loot;
  HuntingCalculatorWindow *m_calculator;
  int m_rightClickedItem;

  DECLARE_EVENT_TABLE()
};

class HuntingCalculatorWindow : public wxDialog {
public:
  HuntingCalculatorWindow(wxWindow *parent, Editor &editor);
  virtual ~HuntingCalculatorWindow();

  // Set the area to analyze (coordinates)
  void SetArea(int startX, int startY, int startZ, int endX, int endY,
               int endZ);

  // Set selection mode (true = use selected tiles, false = use coordinates)
  void SetUseSelection(bool useSelection);

  // Load monsters from the selected area
  void LoadMonstersFromArea();

  // Public helper functions (needed by LootListBox)
  std::string FormatTime(double minutes);
  double CalculateExpectedTimeForItem(const AggregatedLoot &item) const;
  double CalculateTimeForProbability(const AggregatedLoot &item,
                                     double probability) const;

private:
  // Event handlers
  void OnCalculate(wxCommandEvent &event);
  void OnClose(wxCommandEvent &event);
  void OnMonsterDirChanged(wxFileDirPickerEvent &event);
  void OnConfigFileChanged(wxFileDirPickerEvent &event);
  void OnApplyMultipliersChanged(wxCommandEvent &event);
  void OnSaveAnalysis(wxCommandEvent &event);
  void OnLoadAnalysis(wxCommandEvent &event);
  void OnKillModeChanged(wxCommandEvent &event);

  // Helper functions
  void CreateControls();
  void LoadMonsterDatabase();
  void LoadMonstersFromDirectory(const std::string &dirPath);
  bool LoadMonsterFromXML(const std::string &filepath,
                          HuntingMonsterData &data);
  void ParseLootXML(pugi::xml_node lootNode,
                    std::vector<HuntingMonsterData::LootItem> &lootList);
  bool LoadMonsterFromLua(const std::string &filepath,
                          HuntingMonsterData &data);
  void ParseLootLua(const std::string &content,
                    std::vector<HuntingMonsterData::LootItem> &lootList);
  bool LoadConfigLua(const std::string &filepath);
  void CalculateResults();
  void UpdateMonsterList();
  void UpdateLootList();
  void UpdateMultiplierLabels();
  void UpdateKillModeUI();
  double CalculateTimePerKill();
  uint64_t GetCoinValue(uint16_t itemId);
  std::string FormatNumber(double value);
  std::string FormatGold(uint64_t gold);

  // Get total kills per hour (for expected time calculations)
  double GetTotalKillsPerHour() const;

  // Config persistence (per-map settings)
  void LoadMapConfig();
  void SaveMapConfig();
  std::string GetMapConfigPath();

  // Analysis save/load system
  void SaveAnalysis(const std::string &name);
  void LoadAnalysis(const std::string &name);
  std::vector<std::string> GetSavedAnalyses();
  std::string GetAnalysisFolder();
  void RefreshSavedAnalysesList();

  // Cache management
  void CacheSelectionTiles();
  void InvalidateCache();
  bool IsCacheValid() const { return m_cacheValid; }

  // Progress bar helpers
  void ShowProgress(const wxString &message, int total);
  void UpdateProgress(int current);
  void HideProgress();

  // UI Controls - ALL initialized to nullptr to prevent crashes in Release mode
  wxSpinCtrlDouble *m_huntingDuration = nullptr;
  wxSpinCtrlDouble *m_timePerKill = nullptr;
  wxSpinCtrlDouble *m_playerDPS = nullptr;
  wxCheckBox *m_useDPSMode = nullptr;
  wxStaticText *m_timePerKillLabel = nullptr;
  wxStaticText *m_dpsLabel = nullptr;
  wxDirPickerCtrl *m_monsterDirPicker = nullptr;
  wxFilePickerCtrl *m_configFilePicker = nullptr;
  wxCheckBox *m_applyMultipliers = nullptr;

  // Multiplier display labels
  wxStaticText *m_expMultLabel = nullptr;
  wxStaticText *m_lootMultLabel = nullptr;
  wxStaticText *m_spawnMultLabel = nullptr;

  // Coordinate inputs
  wxSpinCtrl *m_startX = nullptr;
  wxSpinCtrl *m_startY = nullptr;
  wxSpinCtrl *m_startZ = nullptr;
  wxSpinCtrl *m_endX = nullptr;
  wxSpinCtrl *m_endY = nullptr;
  wxSpinCtrl *m_endZ = nullptr;

  // Result labels
  wxStaticText *m_expPerHourLabel = nullptr;
  wxStaticText *m_totalExpLabel = nullptr;
  wxStaticText *m_totalKillsLabel = nullptr;
  wxStaticText *m_goldPerHourLabel = nullptr;

  // Lists with sprites
  MonsterListBox *m_monsterList = nullptr;
  LootListBox *m_lootList = nullptr;

  // Buttons
  wxButton *m_calculateButton = nullptr;
  wxButton *m_closeButton = nullptr;
  wxButton *m_saveAnalysisButton = nullptr;

  // Analysis save controls
  wxTextCtrl *m_analysisName = nullptr;
  wxChoice *m_savedAnalysesList = nullptr;

  // Data
  Editor &m_editor;
  std::string m_monsterDirectory;
  std::string m_configFilePath;
  ServerConfig m_serverConfig;
  std::unordered_map<std::string, HuntingMonsterData> m_monsterDatabase;
  std::vector<HuntingMonsterData> m_monstersInArea;
  std::vector<AggregatedLoot> m_aggregatedLoot;

  // Area coordinates
  int m_areaStartX = 0, m_areaStartY = 0, m_areaStartZ = 0;
  int m_areaEndX = 0, m_areaEndY = 0, m_areaEndZ = 0;

  // Selection mode (true = use selected tiles directly, false = use
  // coordinates)
  bool m_useSelection = false;

  // Coordinate box sizer (to hide when using selection)
  wxStaticBoxSizer *m_coordBox = nullptr;

  // Selection info (shown when using lasso selection)
  wxStaticBoxSizer *m_selectionInfoBox = nullptr;
  wxStaticText *m_selectionInfoLabel = nullptr;

  // Cached selection data for recalculation - ONLY tiles with monsters
  struct CachedMonsterData {
    std::string creatureName;
    Outfit outfit;
  };
  std::vector<CachedMonsterData>
      m_cachedMonsters; // Only monsters, not all tiles
  bool m_cacheValid = false;
  int m_cachedCurrentFloor = 7; // Floor when selection was made
  size_t m_cachedTileCount = 0; // Number of tiles in selection

  // Progress tracking
  wxGauge *m_progressBar = nullptr;
  wxStaticText *m_progressLabel = nullptr;

  // Calculation results
  double m_totalExpPerHour = 0;
  double m_totalExp = 0;
  int m_totalKills = 0;
  uint64_t m_totalGoldPerHour = 0;

  // Constants
  static const int DEFAULT_RESPAWN_TIME = 600; // 10 minutes in seconds

  DECLARE_EVENT_TABLE()
};

// Event IDs
enum HuntingCalculatorIDs {
  ID_HUNTING_CALC_CALCULATE = wxID_HIGHEST + 5000,
  ID_HUNTING_CALC_CLOSE,
  ID_HUNTING_CALC_MONSTER_DIR,
  ID_HUNTING_CALC_CONFIG_FILE,
  ID_HUNTING_CALC_APPLY_MULTIPLIERS,
  ID_HUNTING_CALC_PLAYERS,
  ID_HUNTING_CALC_DURATION,
  ID_HUNTING_CALC_TIME_PER_KILL,
  ID_HUNTING_CALC_START_X,
  ID_HUNTING_CALC_START_Y,
  ID_HUNTING_CALC_START_Z,
  ID_HUNTING_CALC_END_X,
  ID_HUNTING_CALC_END_Y,
  ID_HUNTING_CALC_END_Z,
  ID_HUNTING_CALC_MONSTER_LIST,
  ID_HUNTING_CALC_LOOT_LIST,
  ID_HUNTING_CALC_SAVE_ANALYSIS,
  ID_HUNTING_CALC_LOAD_ANALYSIS,
  ID_HUNTING_CALC_ANALYSIS_NAME,
  ID_HUNTING_CALC_USE_DPS_MODE,
  ID_HUNTING_CALC_PLAYER_DPS,
  ID_HUNTING_CALC_LOOT_EXPECTED_TIME
};

#endif // RME_HUNTING_CALCULATOR_WINDOW_H_
