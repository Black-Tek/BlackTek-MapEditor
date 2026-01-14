//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Hunting Calculator Window Implementation
//////////////////////////////////////////////////////////////////////

#include "main.h"
#include "hunting_calculator_window.h"
#include "editor.h"
#include "map.h"
#include "tile.h"
#include "creature.h"
#include "items.h"
#include "graphics.h"
#include "gui.h"

#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/statline.h>
#include "ext/pugixml.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <cctype>
#include <fstream>
#include <cmath>

// Coin IDs and values
static const uint16_t ITEM_GOLD_COIN = 2148;
static const uint16_t ITEM_PLATINUM_COIN = 2152;
static const uint16_t ITEM_CRYSTAL_COIN = 2160;
static const uint32_t GOLD_COIN_VALUE = 1;
static const uint32_t PLATINUM_COIN_VALUE = 100;
static const uint32_t CRYSTAL_COIN_VALUE = 10000;

// ============================================================================
// MonsterListBox Implementation
// ============================================================================

MonsterListBox::MonsterListBox(wxWindow* parent, wxWindowID id)
    : wxVListBox(parent, id, wxDefaultPosition, wxSize(480, 200))
    , m_monsters(nullptr)
{
    SetBackgroundColour(wxColour(45, 45, 48));
}

void MonsterListBox::SetMonsters(const std::vector<HuntingMonsterData>* monsters)
{
    m_monsters = monsters;
    SetItemCount(monsters ? monsters->size() : 0);
    Refresh();
}

void MonsterListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
    if (!m_monsters || n >= m_monsters->size()) return;
    
    const HuntingMonsterData& monster = (*m_monsters)[n];
    
    // Background
    if (IsSelected(n)) {
        dc.SetBrush(wxBrush(wxColour(62, 62, 66)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect);
    }
    
    // Draw white background for sprite
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.DrawRectangle(rect.GetX() + 2, rect.GetY() + 2, 32, 32);
    
    // Draw creature sprite with outfit colors
    if (monster.outfit.lookType > 0) {
        try {
            GameSprite* sprite = g_gui.gfx.getCreatureSprite(monster.outfit.lookType);
            if (sprite) {
                wxRect spriteRect(rect.GetX() + 2, rect.GetY() + 2, 32, 32);
                sprite->DrawTo(&dc, spriteRect, monster.outfit);
            }
        } catch (...) {
            // If sprite loading fails, just skip drawing the sprite
        }
    }
    
    // Text color
    dc.SetTextForeground(IsSelected(n) ? wxColour(255, 255, 255) : wxColour(220, 220, 220));
    
    // Draw monster info columns
    int x = rect.GetX() + 38;
    int y = rect.GetY() + 10;
    
    // Name (wider column)
    dc.DrawText(wxString(monster.name).Left(18), x, y);
    x += 130;
    
    // Count
    dc.DrawText(wxString::Format("%d", monster.count), x, y);
    x += 50;
    
    // Exp
    dc.DrawText(wxString::Format("%llu", (unsigned long long)monster.experience), x, y);
    x += 70;
    
    // Regen time
    dc.DrawText(wxString::Format("%.0fs", monster.respawnTime), x, y);
    x += 60;
    
    // Kills/h
    dc.DrawText(wxString::Format("%.0f", monster.killsPerHour), x, y);
    x += 55;
    
    // Exp/h
    dc.SetTextForeground(wxColour(68, 173, 37));  // Green for exp
    dc.DrawText(wxString::Format("%.0f", monster.expPerHour), x, y);
}

wxCoord MonsterListBox::OnMeasureItem(size_t n) const
{
    return 36;
}

// ============================================================================
// LootListBox Implementation
// ============================================================================

BEGIN_EVENT_TABLE(LootListBox, wxVListBox)
    EVT_RIGHT_DOWN(LootListBox::OnRightClick)
    EVT_MENU(ID_HUNTING_CALC_LOOT_EXPECTED_TIME, LootListBox::OnShowExpectedTime)
END_EVENT_TABLE()

LootListBox::LootListBox(wxWindow* parent, wxWindowID id, HuntingCalculatorWindow* calculator)
    : wxVListBox(parent, id, wxDefaultPosition, wxSize(380, 200))
    , m_loot(nullptr)
    , m_calculator(calculator)
    , m_rightClickedItem(-1)
{
    SetBackgroundColour(wxColour(45, 45, 48));
}

void LootListBox::SetLoot(const std::vector<AggregatedLoot>* loot)
{
    m_loot = loot;
    SetItemCount(loot ? loot->size() : 0);
    Refresh();
}

void LootListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
    if (!m_loot || n >= m_loot->size()) return;
    
    const AggregatedLoot& loot = (*m_loot)[n];
    
    // Background
    if (IsSelected(n)) {
        dc.SetBrush(wxBrush(wxColour(62, 62, 66)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect);
    }
    
    // Draw white background for sprite
    dc.SetBrush(wxBrush(wxColour(255, 255, 255)));
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.DrawRectangle(rect.GetX() + 2, rect.GetY() + 2, 32, 32);
    
    // Draw item sprite
    if (loot.id > 0) {
        // Get the item type to find the correct sprite
        try {
            if (g_items.isValidID(loot.id)) {
                const ItemType& itemType = g_items.getItemType(loot.id);
                if (itemType.sprite) {
                    itemType.sprite->DrawTo(&dc, SPRITE_SIZE_32x32, rect.GetX() + 2, rect.GetY() + 2, 32, 32);
                }
            }
        } catch (...) {
            // If item sprite loading fails, just skip drawing the sprite
        }
    }
    
    // Text color
    dc.SetTextForeground(IsSelected(n) ? wxColour(255, 255, 255) : wxColour(220, 220, 220));
    
    int x = rect.GetX() + 38;
    int y = rect.GetY() + 10;
    
    // Item name
    dc.DrawText(wxString(loot.name).Left(20), x, y);
    x += 150;
    
    // Expected count
    dc.SetTextForeground(wxColour(255, 215, 0));  // Gold color
    dc.DrawText(wxString::Format("%.0f", loot.expectedCount), x, y);
    x += 80;
    
    // Drop rate
    dc.SetTextForeground(wxColour(150, 150, 150));
    dc.DrawText(wxString::Format("%.2f%%", loot.dropRate), x, y);
}

wxCoord LootListBox::OnMeasureItem(size_t n) const
{
    return 36;
}

void LootListBox::OnRightClick(wxMouseEvent& event)
{
    if (!m_loot || m_loot->empty()) return;
    
    // Find which item was clicked
    int item = VirtualHitTest(event.GetY());
    if (item == wxNOT_FOUND || item < 0 || static_cast<size_t>(item) >= m_loot->size()) return;
    
    m_rightClickedItem = item;
    
    // Select the item
    SetSelection(item);
    
    // Create context menu
    wxMenu menu;
    menu.Append(ID_HUNTING_CALC_LOOT_EXPECTED_TIME, "Expected Time to Drop");
    
    PopupMenu(&menu, event.GetPosition());
}

void LootListBox::OnShowExpectedTime(wxCommandEvent& event)
{
    if (!m_loot || m_rightClickedItem < 0 || static_cast<size_t>(m_rightClickedItem) >= m_loot->size()) return;
    if (!m_calculator) return;
    
    const AggregatedLoot& item = (*m_loot)[m_rightClickedItem];
    
    // Calculate expected times
    double expectedTime = m_calculator->CalculateExpectedTimeForItem(item);
    double time50 = m_calculator->CalculateTimeForProbability(item, 0.50);
    double time90 = m_calculator->CalculateTimeForProbability(item, 0.90);
    double time95 = m_calculator->CalculateTimeForProbability(item, 0.95);
    
    // Format the message
    wxString message;
    message += wxString::Format("Item: %s\n", item.name);
    message += wxString::Format("Drop Rate: %.4f%%\n\n", item.dropRate);
    message += "--- Expected Time ---\n";
    message += wxString::Format("Average (E[T]): %s\n\n", m_calculator->FormatTime(expectedTime));
    message += "--- Probability Thresholds ---\n";
    message += wxString::Format("50%% chance: %s\n", m_calculator->FormatTime(time50));
    message += wxString::Format("90%% chance: %s\n", m_calculator->FormatTime(time90));
    message += wxString::Format("95%% chance: %s\n", m_calculator->FormatTime(time95));
    
    wxMessageBox(message, "Expected Time to Drop: " + item.name, wxOK | wxICON_INFORMATION);
}

// ============================================================================
// HuntingCalculatorWindow Implementation
// ============================================================================

BEGIN_EVENT_TABLE(HuntingCalculatorWindow, wxDialog)
    EVT_BUTTON(ID_HUNTING_CALC_CALCULATE, HuntingCalculatorWindow::OnCalculate)
    EVT_BUTTON(ID_HUNTING_CALC_CLOSE, HuntingCalculatorWindow::OnClose)
    EVT_BUTTON(ID_HUNTING_CALC_SAVE_ANALYSIS, HuntingCalculatorWindow::OnSaveAnalysis)
    EVT_CHOICE(ID_HUNTING_CALC_LOAD_ANALYSIS, HuntingCalculatorWindow::OnLoadAnalysis)
    EVT_DIRPICKER_CHANGED(ID_HUNTING_CALC_MONSTER_DIR, HuntingCalculatorWindow::OnMonsterDirChanged)
    EVT_FILEPICKER_CHANGED(ID_HUNTING_CALC_CONFIG_FILE, HuntingCalculatorWindow::OnConfigFileChanged)
    EVT_CHECKBOX(ID_HUNTING_CALC_APPLY_MULTIPLIERS, HuntingCalculatorWindow::OnApplyMultipliersChanged)
    EVT_CHECKBOX(ID_HUNTING_CALC_USE_DPS_MODE, HuntingCalculatorWindow::OnKillModeChanged)
END_EVENT_TABLE()

HuntingCalculatorWindow::HuntingCalculatorWindow(wxWindow* parent, Editor& editor)
    : wxDialog(parent, wxID_ANY, "Hunting Calculator", wxDefaultPosition, wxSize(950, 750),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_editor(editor)
    , m_cacheValid(false)
    , m_cachedCurrentFloor(7)
    , m_cachedTileCount(0)
{
    SetBackgroundColour(wxColour(37, 37, 38));
    
    try {
        CreateControls();
    } catch (...) {
        // If CreateControls fails, we still have a valid dialog
    }
    
    try {
        LoadMapConfig();  // Load saved config for this map
    } catch (...) {
        // If LoadMapConfig fails, continue with defaults
    }
    
    Centre();
}

HuntingCalculatorWindow::~HuntingCalculatorWindow()
{
    // Clean up cached data to free memory
    InvalidateCache();
    m_monstersInArea.clear();
    m_monstersInArea.shrink_to_fit();
    m_aggregatedLoot.clear();
    m_aggregatedLoot.shrink_to_fit();
    m_monsterDatabase.clear();
}

void HuntingCalculatorWindow::CreateControls()
{
    wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);
    
    // Helper lambda for creating labels
    auto createLabel = [this](const wxString& text) {
        wxStaticText* label = newd wxStaticText(this, wxID_ANY, text);
        label->SetForegroundColour(wxColour(180, 180, 180));
        return label;
    };
    
    // ========================================================================
    // Area Coordinates (shown when NOT using lasso selection)
    // ========================================================================
    m_coordBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Area Coordinates");
    m_coordBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    wxFlexGridSizer* coordGrid = newd wxFlexGridSizer(2, 7, 5, 5);
    
    coordGrid->Add(createLabel("Start:"), 0, wxALIGN_CENTER_VERTICAL);
    coordGrid->Add(createLabel("X:"), 0, wxALIGN_CENTER_VERTICAL);
    m_startX = newd wxSpinCtrl(this, ID_HUNTING_CALC_START_X, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    coordGrid->Add(m_startX, 0);
    coordGrid->Add(createLabel("Y:"), 0, wxALIGN_CENTER_VERTICAL);
    m_startY = newd wxSpinCtrl(this, ID_HUNTING_CALC_START_Y, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    coordGrid->Add(m_startY, 0);
    coordGrid->Add(createLabel("Z:"), 0, wxALIGN_CENTER_VERTICAL);
    m_startZ = newd wxSpinCtrl(this, ID_HUNTING_CALC_START_Z, "7", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 15);
    coordGrid->Add(m_startZ, 0);
    
    coordGrid->Add(createLabel("End:"), 0, wxALIGN_CENTER_VERTICAL);
    coordGrid->Add(createLabel("X:"), 0, wxALIGN_CENTER_VERTICAL);
    m_endX = newd wxSpinCtrl(this, ID_HUNTING_CALC_END_X, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    coordGrid->Add(m_endX, 0);
    coordGrid->Add(createLabel("Y:"), 0, wxALIGN_CENTER_VERTICAL);
    m_endY = newd wxSpinCtrl(this, ID_HUNTING_CALC_END_Y, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    coordGrid->Add(m_endY, 0);
    coordGrid->Add(createLabel("Z:"), 0, wxALIGN_CENTER_VERTICAL);
    m_endZ = newd wxSpinCtrl(this, ID_HUNTING_CALC_END_Z, "7", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 15);
    coordGrid->Add(m_endZ, 0);
    
    m_coordBox->Add(coordGrid, 0, wxALL, 5);
    mainSizer->Add(m_coordBox, 0, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Selection Info (shown when using lasso selection)
    // ========================================================================
    m_selectionInfoBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Selection Info");
    m_selectionInfoBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    m_selectionInfoLabel = newd wxStaticText(this, wxID_ANY, "No selection");
    m_selectionInfoLabel->SetForegroundColour(wxColour(100, 200, 100));
    m_selectionInfoLabel->SetFont(m_selectionInfoLabel->GetFont().Bold());
    m_selectionInfoBox->Add(m_selectionInfoLabel, 0, wxALL | wxALIGN_CENTER, 10);
    
    // Initially hidden (shown only when using lasso selection)
    m_selectionInfoBox->GetStaticBox()->Hide();
    m_selectionInfoBox->ShowItems(false);
    
    mainSizer->Add(m_selectionInfoBox, 0, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Calculation Parameters with Multipliers
    // ========================================================================
    wxStaticBoxSizer* paramBox = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Calculation Parameters");
    paramBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    // Left side - basic params
    wxFlexGridSizer* paramGrid = newd wxFlexGridSizer(4, 2, 5, 10);
    
    paramGrid->Add(createLabel("Hunting Duration (min):"), 0, wxALIGN_CENTER_VERTICAL);
    m_huntingDuration = newd wxSpinCtrlDouble(this, ID_HUNTING_CALC_DURATION, "60", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1.0, 1440.0, 60.0, 5.0);
    paramGrid->Add(m_huntingDuration, 0);
    
    // Kill time mode checkbox
    m_useDPSMode = newd wxCheckBox(this, ID_HUNTING_CALC_USE_DPS_MODE, "Use DPS mode");
    m_useDPSMode->SetForegroundColour(wxColour(200, 200, 200));
    m_useDPSMode->SetValue(false);
    paramGrid->Add(m_useDPSMode, 0, wxALIGN_CENTER_VERTICAL);
    paramGrid->AddSpacer(0);
    
    // Time per Kill (shown when NOT using DPS mode)
    m_timePerKillLabel = createLabel("Time per Kill (s):");
    paramGrid->Add(m_timePerKillLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_timePerKill = newd wxSpinCtrlDouble(this, ID_HUNTING_CALC_TIME_PER_KILL, "10.0", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1.0, 300.0, 10.0, 1.0);
    paramGrid->Add(m_timePerKill, 0);
    
    // Player DPS (shown when using DPS mode)
    m_dpsLabel = createLabel("Your DPS:");
    m_dpsLabel->Hide();
    paramGrid->Add(m_dpsLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_playerDPS = newd wxSpinCtrlDouble(this, ID_HUNTING_CALC_PLAYER_DPS, "1000", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 100, 100000, 1000, 100);
    m_playerDPS->Hide();
    paramGrid->Add(m_playerDPS, 0);
    
    paramBox->Add(paramGrid, 0, wxALL, 5);
    
    // Separator
    paramBox->Add(newd wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL), 0, wxEXPAND | wxALL, 10);
    
    // Right side - multipliers
    wxBoxSizer* multSizer = newd wxBoxSizer(wxVERTICAL);
    
    m_applyMultipliers = newd wxCheckBox(this, ID_HUNTING_CALC_APPLY_MULTIPLIERS, "Apply config.lua multipliers");
    m_applyMultipliers->SetForegroundColour(wxColour(200, 200, 200));
    m_applyMultipliers->SetValue(false);
    m_applyMultipliers->Enable(false);
    multSizer->Add(m_applyMultipliers, 0, wxBOTTOM, 5);
    
    wxFlexGridSizer* multGrid = newd wxFlexGridSizer(3, 2, 3, 10);
    
    multGrid->Add(createLabel("Exp Rate:"), 0, wxALIGN_CENTER_VERTICAL);
    m_expMultLabel = newd wxStaticText(this, wxID_ANY, "1.0x");
    m_expMultLabel->SetForegroundColour(wxColour(68, 173, 37));
    multGrid->Add(m_expMultLabel, 0, wxALIGN_CENTER_VERTICAL);
    
    multGrid->Add(createLabel("Loot Rate:"), 0, wxALIGN_CENTER_VERTICAL);
    m_lootMultLabel = newd wxStaticText(this, wxID_ANY, "1.0x");
    m_lootMultLabel->SetForegroundColour(wxColour(255, 215, 0));
    multGrid->Add(m_lootMultLabel, 0, wxALIGN_CENTER_VERTICAL);
    
    multGrid->Add(createLabel("Spawn Rate:"), 0, wxALIGN_CENTER_VERTICAL);
    m_spawnMultLabel = newd wxStaticText(this, wxID_ANY, "1.0x");
    m_spawnMultLabel->SetForegroundColour(wxColour(100, 149, 237));
    multGrid->Add(m_spawnMultLabel, 0, wxALIGN_CENTER_VERTICAL);
    
    multSizer->Add(multGrid, 0);
    paramBox->Add(multSizer, 0, wxALL, 5);
    
    mainSizer->Add(paramBox, 0, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Data Sources Section
    // ========================================================================
    wxStaticBoxSizer* dataBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Data Sources");
    dataBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    wxFlexGridSizer* dataGrid = newd wxFlexGridSizer(2, 2, 5, 10);
    dataGrid->AddGrowableCol(1, 1);
    
    // Monster directory
    wxStaticText* monsterDirLabel = newd wxStaticText(this, wxID_ANY, "Monsters Directory:");
    monsterDirLabel->SetForegroundColour(wxColour(255, 255, 255));
    dataGrid->Add(monsterDirLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_monsterDirPicker = newd wxDirPickerCtrl(this, ID_HUNTING_CALC_MONSTER_DIR,
        wxEmptyString, "Select monster data directory",
        wxDefaultPosition, wxDefaultSize, wxDIRP_USE_TEXTCTRL | wxDIRP_DIR_MUST_EXIST);
    m_monsterDirPicker->GetTextCtrl()->SetForegroundColour(wxColour(255, 255, 255));
    m_monsterDirPicker->GetTextCtrl()->SetBackgroundColour(wxColour(45, 45, 48));
    dataGrid->Add(m_monsterDirPicker, 1, wxEXPAND);
    
    // Config.lua file
    wxStaticText* configLabel = newd wxStaticText(this, wxID_ANY, "Config Lua:");
    configLabel->SetForegroundColour(wxColour(255, 255, 255));
    dataGrid->Add(configLabel, 0, wxALIGN_CENTER_VERTICAL);
    m_configFilePicker = newd wxFilePickerCtrl(this, ID_HUNTING_CALC_CONFIG_FILE,
        wxEmptyString, "Select config.lua file", "Lua files (*.lua)|*.lua",
        wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
    m_configFilePicker->GetTextCtrl()->SetForegroundColour(wxColour(255, 255, 255));
    m_configFilePicker->GetTextCtrl()->SetBackgroundColour(wxColour(45, 45, 48));
    dataGrid->Add(m_configFilePicker, 1, wxEXPAND);
    
    dataBox->Add(dataGrid, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(dataBox, 0, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Calculate Button
    // ========================================================================
    m_calculateButton = newd wxButton(this, ID_HUNTING_CALC_CALCULATE, "Calculate");
    m_calculateButton->SetBackgroundColour(wxColour(76, 175, 80));
    m_calculateButton->SetForegroundColour(*wxWHITE);
    m_calculateButton->SetMinSize(wxSize(150, 35));
    mainSizer->Add(m_calculateButton, 0, wxALIGN_CENTER | wxALL, 8);
    
    // ========================================================================
    // Progress Bar (hidden by default)
    // ========================================================================
    wxBoxSizer* progressSizer = newd wxBoxSizer(wxHORIZONTAL);
    m_progressLabel = newd wxStaticText(this, wxID_ANY, "");
    m_progressLabel->SetForegroundColour(wxColour(200, 200, 200));
    progressSizer->Add(m_progressLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    
    m_progressBar = newd wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(300, 20));
    progressSizer->Add(m_progressBar, 1, wxALIGN_CENTER_VERTICAL);
    
    mainSizer->Add(progressSizer, 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
    m_progressLabel->Hide();
    m_progressBar->Hide();
    
    // ========================================================================
    // Experience Results
    // ========================================================================
    wxStaticBoxSizer* resultBox = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Results");
    resultBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    m_expPerHourLabel = newd wxStaticText(this, wxID_ANY, "Exp/Hour: 0");
    m_expPerHourLabel->SetFont(m_expPerHourLabel->GetFont().Bold());
    m_expPerHourLabel->SetForegroundColour(wxColour(68, 173, 37));
    resultBox->Add(m_expPerHourLabel, 1, wxALL | wxALIGN_CENTER_VERTICAL, 8);
    
    m_totalExpLabel = newd wxStaticText(this, wxID_ANY, "Total Exp: 0");
    m_totalExpLabel->SetFont(m_totalExpLabel->GetFont().Bold());
    m_totalExpLabel->SetForegroundColour(wxColour(100, 200, 100));
    resultBox->Add(m_totalExpLabel, 1, wxALL | wxALIGN_CENTER_VERTICAL, 8);
    
    m_totalKillsLabel = newd wxStaticText(this, wxID_ANY, "Kills: 0");
    m_totalKillsLabel->SetFont(m_totalKillsLabel->GetFont().Bold());
    m_totalKillsLabel->SetForegroundColour(wxColour(200, 200, 200));
    resultBox->Add(m_totalKillsLabel, 1, wxALL | wxALIGN_CENTER_VERTICAL, 8);
    
    m_goldPerHourLabel = newd wxStaticText(this, wxID_ANY, "Gold/Hour: 0");
    m_goldPerHourLabel->SetFont(m_goldPerHourLabel->GetFont().Bold());
    m_goldPerHourLabel->SetForegroundColour(wxColour(255, 215, 0));  // Gold color
    resultBox->Add(m_goldPerHourLabel, 1, wxALL | wxALIGN_CENTER_VERTICAL, 8);
    
    mainSizer->Add(resultBox, 0, wxEXPAND | wxALL, 5);

    // ========================================================================
    // Monster and Loot Lists (side by side) with headers
    // ========================================================================
    wxBoxSizer* listsSizer = newd wxBoxSizer(wxHORIZONTAL);
    
    // Monster List with header
    wxStaticBoxSizer* monsterBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Monsters in Selection");
    monsterBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    // Column headers for monsters
    wxBoxSizer* monsterHeaderSizer = newd wxBoxSizer(wxHORIZONTAL);
    monsterHeaderSizer->AddSpacer(40);  // Sprite space
    
    auto addHeader = [this](wxBoxSizer* sizer, const wxString& text, int width) {
        wxStaticText* header = newd wxStaticText(this, wxID_ANY, text);
        header->SetForegroundColour(wxColour(150, 150, 150));
        header->SetFont(header->GetFont().Bold());
        header->SetMinSize(wxSize(width, -1));
        sizer->Add(header, 0);
    };
    
    addHeader(monsterHeaderSizer, "Name", 130);
    addHeader(monsterHeaderSizer, "Count", 50);
    addHeader(monsterHeaderSizer, "Exp", 70);
    addHeader(monsterHeaderSizer, "Regen", 60);
    addHeader(monsterHeaderSizer, "Kills/h", 55);
    addHeader(monsterHeaderSizer, "Exp/h", 60);
    
    monsterBox->Add(monsterHeaderSizer, 0, wxLEFT | wxRIGHT | wxTOP, 5);
    
    m_monsterList = newd MonsterListBox(this, ID_HUNTING_CALC_MONSTER_LIST);
    monsterBox->Add(m_monsterList, 1, wxEXPAND | wxALL, 5);
    listsSizer->Add(monsterBox, 1, wxEXPAND | wxRIGHT, 5);
    
    // Loot List with header
    wxStaticBoxSizer* lootBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Expected Loot");
    lootBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    // Column headers for loot
    wxBoxSizer* lootHeaderSizer = newd wxBoxSizer(wxHORIZONTAL);
    lootHeaderSizer->AddSpacer(40);  // Sprite space
    addHeader(lootHeaderSizer, "Item", 150);
    addHeader(lootHeaderSizer, "Expected", 80);
    addHeader(lootHeaderSizer, "Drop %", 60);
    
    lootBox->Add(lootHeaderSizer, 0, wxLEFT | wxRIGHT | wxTOP, 5);
    
    m_lootList = newd LootListBox(this, ID_HUNTING_CALC_LOOT_LIST, this);
    lootBox->Add(m_lootList, 1, wxEXPAND | wxALL, 5);
    listsSizer->Add(lootBox, 1, wxEXPAND | wxLEFT, 5);
    
    mainSizer->Add(listsSizer, 1, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Save Analysis Section
    // ========================================================================
    wxStaticBoxSizer* saveBox = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Save Analysis");
    saveBox->GetStaticBox()->SetForegroundColour(wxColour(200, 200, 200));
    
    wxStaticText* nameLabel = newd wxStaticText(this, wxID_ANY, "Analysis Name:");
    nameLabel->SetForegroundColour(wxColour(255, 255, 255));
    saveBox->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    m_analysisName = newd wxTextCtrl(this, ID_HUNTING_CALC_ANALYSIS_NAME, "", wxDefaultPosition, wxSize(200, -1));
    m_analysisName->SetForegroundColour(wxColour(255, 255, 255));
    m_analysisName->SetBackgroundColour(wxColour(45, 45, 48));
    saveBox->Add(m_analysisName, 1, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    m_saveAnalysisButton = newd wxButton(this, ID_HUNTING_CALC_SAVE_ANALYSIS, "Save");
    m_saveAnalysisButton->SetMinSize(wxSize(80, 28));
    saveBox->Add(m_saveAnalysisButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    wxStaticText* loadLabel = newd wxStaticText(this, wxID_ANY, "Load:");
    loadLabel->SetForegroundColour(wxColour(255, 255, 255));
    saveBox->Add(loadLabel, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    m_savedAnalysesList = newd wxChoice(this, ID_HUNTING_CALC_LOAD_ANALYSIS);
    m_savedAnalysesList->SetMinSize(wxSize(150, -1));
    saveBox->Add(m_savedAnalysesList, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    
    // Populate saved analyses list
    RefreshSavedAnalysesList();
    
    mainSizer->Add(saveBox, 0, wxEXPAND | wxALL, 5);
    
    // ========================================================================
    // Close Button
    // ========================================================================
    m_closeButton = newd wxButton(this, ID_HUNTING_CALC_CLOSE, "Close");
    m_closeButton->SetMinSize(wxSize(100, 30));
    mainSizer->Add(m_closeButton, 0, wxALIGN_CENTER | wxALL, 10);
    
    SetSizer(mainSizer);
}

void HuntingCalculatorWindow::SetArea(int startX, int startY, int startZ, int endX, int endY, int endZ)
{
    // Ensure start <= end
    m_areaStartX = std::min(startX, endX);
    m_areaStartY = std::min(startY, endY);
    m_areaStartZ = std::min(startZ, endZ);
    m_areaEndX = std::max(startX, endX);
    m_areaEndY = std::max(startY, endY);
    m_areaEndZ = std::max(startZ, endZ);
    
    // Update UI
    m_startX->SetValue(m_areaStartX);
    m_startY->SetValue(m_areaStartY);
    m_startZ->SetValue(m_areaStartZ);
    m_endX->SetValue(m_areaEndX);
    m_endY->SetValue(m_areaEndY);
    m_endZ->SetValue(m_areaEndZ);
}

void HuntingCalculatorWindow::SetUseSelection(bool useSelection)
{
    m_useSelection = useSelection;
    
    // Hide/show coordinate box based on selection mode
    if (m_coordBox) {
        m_coordBox->GetStaticBox()->Show(!useSelection);
        m_coordBox->Show(!useSelection);
    }
    
    // Show/hide selection info based on selection mode
    if (m_selectionInfoBox) {
        m_selectionInfoBox->GetStaticBox()->Show(useSelection);
        m_selectionInfoBox->ShowItems(useSelection);
    }
    
    // If using selection, cache the tiles and update info
    if (useSelection) {
        // Safely get current floor
        try {
            m_cachedCurrentFloor = g_gui.GetCurrentFloor();
        } catch (...) {
            m_cachedCurrentFloor = 7;  // Default to ground floor
        }
        
        m_cacheValid = false;
        
        // Cache selection tiles now (with error handling inside)
        CacheSelectionTiles();
        
        // Update selection info label
        if (m_selectionInfoLabel) {
            wxString info = wxString::Format("Floor %d  |  %zu tiles  |  %zu monsters",
                m_cachedCurrentFloor, m_cachedTileCount, m_cachedMonsters.size());
            m_selectionInfoLabel->SetLabel(info);
        }
    }
    
    Layout();
    Refresh();
}

void HuntingCalculatorWindow::LoadMonstersFromArea()
{
    m_monstersInArea.clear();
    
    std::map<std::string, int> monsterCounts;
    std::map<std::string, std::string> monsterOriginalNames;
    std::map<std::string, Outfit> monsterOutfits;
    
    if (m_useSelection) {
        // Use cached monsters if available
        if (m_cacheValid && !m_cachedMonsters.empty()) {
            // Process cached monsters
            for (const auto& cachedMonster : m_cachedMonsters) {
                std::string lowerName = cachedMonster.creatureName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                
                monsterCounts[lowerName]++;
                monsterOriginalNames[lowerName] = cachedMonster.creatureName;
                
                if (cachedMonster.outfit.lookType > 0) {
                    monsterOutfits[lowerName] = cachedMonster.outfit;
                }
            }
        } else {
            // No cache - try to get from current selection
            if (!m_editor.hasSelection()) {
                wxMessageBox("No selection found.\nPlease make a selection with the lasso tool first.",
                             "No Selection", wxOK | wxICON_INFORMATION);
                return;
            }
            
            const Selection& selection = m_editor.getSelection();
            if (selection.empty()) {
                wxMessageBox("Selection is empty.", "No Selection", wxOK | wxICON_INFORMATION);
                return;
            }
            
            // Use the cached floor (which was detected from the selection in CacheSelectionTiles)
            int currentFloor = m_cachedCurrentFloor;
            
            // Copy tiles to local vector first to avoid issues with selection modification
            std::vector<Tile*> tilesToProcess;
            try {
                const TileSet& tiles = selection.getTiles();
                tilesToProcess.reserve(tiles.size());
                for (Tile* tile : tiles) {
                    if (tile != nullptr && tile->location != nullptr) {
                        tilesToProcess.push_back(tile);
                    }
                }
            } catch (...) {
                wxMessageBox("Error accessing selection tiles.", "Error", wxOK | wxICON_ERROR);
                return;
            }
            
            // If cache is not valid, detect floor from tiles
            if (!m_cacheValid && !tilesToProcess.empty()) {
                std::map<int, int> floorCounts;
                for (Tile* tile : tilesToProcess) {
                    if (tile != nullptr && tile->location != nullptr) {
                        try {
                            int tileZ = tile->getZ();
                            floorCounts[tileZ]++;
                        } catch (...) {
                            continue;
                        }
                    }
                }
                
                // Find the floor with the most tiles
                int maxCount = 0;
                for (const auto& pair : floorCounts) {
                    if (pair.second > maxCount) {
                        maxCount = pair.second;
                        currentFloor = pair.first;
                    }
                }
                m_cachedCurrentFloor = currentFloor;
            }
            
            // Iterate over copied tiles
            for (Tile* tile : tilesToProcess) {
                if (tile == nullptr || tile->location == nullptr) continue;
                
                int tileZ = 0;
                try {
                    tileZ = tile->getZ();
                } catch (...) {
                    continue;
                }
                
                if (tileZ != currentFloor) continue;
                
                Creature* creature = tile->creature;
                if (creature != nullptr && !creature->isNpc()) {
                    try {
                        std::string name = creature->getName();
                        std::string lowerName = name;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                        
                        monsterCounts[lowerName]++;
                        monsterOriginalNames[lowerName] = name;
                        
                        const Outfit& outfit = creature->getLookType();
                        if (outfit.lookType > 0) {
                            monsterOutfits[lowerName] = outfit;
                        }
                    } catch (...) {
                        continue;
                    }
                }
            }
        }
        
        if (monsterCounts.empty()) {
            wxMessageBox("No monsters found on floor " + std::to_string(m_cachedCurrentFloor) + ".",
                         "No Monsters", wxOK | wxICON_INFORMATION);
            return;
        }
    } else {
        // Use coordinate-based area scan
        Map& map = m_editor.getMap();
        
        int startX = m_startX->GetValue();
        int startY = m_startY->GetValue();
        int startZ = m_startZ->GetValue();
        int endX = m_endX->GetValue();
        int endY = m_endY->GetValue();
        int endZ = m_endZ->GetValue();
        
        // Ensure proper order
        if (startX > endX) std::swap(startX, endX);
        if (startY > endY) std::swap(startY, endY);
        if (startZ > endZ) std::swap(startZ, endZ);
        
        // Scan the area for creatures (no progress bar to avoid wxYield issues)
        for (int z = startZ; z <= endZ; ++z) {
            for (int y = startY; y <= endY; ++y) {
                for (int x = startX; x <= endX; ++x) {
                    Tile* tile = map.getTile(x, y, z);
                    if (tile && tile->creature && !tile->creature->isNpc()) {
                        std::string name = tile->creature->getName();
                        std::string lowerName = name;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                        monsterCounts[lowerName]++;
                        monsterOriginalNames[lowerName] = name;
                        
                        const Outfit& outfit = tile->creature->getLookType();
                        if (outfit.lookType > 0) {
                            monsterOutfits[lowerName] = outfit;
                        }
                    }
                }
            }
        }
    }
    
    // Convert to monster data
    for (const auto& pair : monsterCounts) {
        HuntingMonsterData data;
        data.name = monsterOriginalNames[pair.first];
        data.count = pair.second;
        
        auto outfitIt = monsterOutfits.find(pair.first);
        if (outfitIt != monsterOutfits.end()) {
            data.outfit = outfitIt->second;
        }
        
        m_monstersInArea.push_back(data);
    }
    
    // Sort by count descending
    std::sort(m_monstersInArea.begin(), m_monstersInArea.end(),
              [](const HuntingMonsterData& a, const HuntingMonsterData& b) {
                  return a.count > b.count;
              });
}

bool HuntingCalculatorWindow::LoadConfigLua(const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Parse rates from config.lua
    std::regex rateExpRegex(R"(rateExp\s*=\s*(\d+(?:\.\d+)?))");
    std::regex rateLootRegex(R"(rateLoot\s*=\s*(\d+(?:\.\d+)?))");
    std::regex rateSpawnRegex(R"(rateSpawn\s*=\s*(\d+(?:\.\d+)?))");
    std::regex rateSkillRegex(R"(rateSkill\s*=\s*(\d+(?:\.\d+)?))");
    std::regex rateMagicRegex(R"(rateMagic\s*=\s*(\d+(?:\.\d+)?))");
    
    std::smatch match;
    
    if (std::regex_search(content, match, rateExpRegex)) {
        m_serverConfig.rateExp = std::stod(match[1].str());
    }
    
    if (std::regex_search(content, match, rateLootRegex)) {
        m_serverConfig.rateLoot = std::stod(match[1].str());
    }
    
    if (std::regex_search(content, match, rateSpawnRegex)) {
        m_serverConfig.rateSpawn = std::stod(match[1].str());
    }
    
    if (std::regex_search(content, match, rateSkillRegex)) {
        m_serverConfig.rateSkill = std::stod(match[1].str());
    }
    
    if (std::regex_search(content, match, rateMagicRegex)) {
        m_serverConfig.rateMagic = std::stod(match[1].str());
    }
    
    m_serverConfig.loaded = true;
    return true;
}

void HuntingCalculatorWindow::UpdateMultiplierLabels()
{
    if (m_serverConfig.loaded) {
        m_expMultLabel->SetLabel(wxString::Format("%.1fx", m_serverConfig.rateExp));
        m_lootMultLabel->SetLabel(wxString::Format("%.1fx", m_serverConfig.rateLoot));
        m_spawnMultLabel->SetLabel(wxString::Format("%.1fx", m_serverConfig.rateSpawn));
    } else {
        m_expMultLabel->SetLabel("1.0x");
        m_lootMultLabel->SetLabel("1.0x");
        m_spawnMultLabel->SetLabel("1.0x");
    }
}

void HuntingCalculatorWindow::LoadMonsterDatabase()
{
    m_monsterDatabase.clear();
    
    if (m_monsterDirectory.empty()) {
        return;
    }
    
    // Load from main directory and subdirectories
    LoadMonstersFromDirectory(m_monsterDirectory);
    
    // Also check for 'lua' subdirectory
    wxString luaDir = m_monsterDirectory + wxFileName::GetPathSeparator() + "lua";
    if (wxDir::Exists(luaDir)) {
        LoadMonstersFromDirectory(luaDir.ToStdString());
    }
}

void HuntingCalculatorWindow::LoadMonstersFromDirectory(const std::string& dirPath)
{
    if (dirPath.empty()) {
        return;
    }
    
    wxDir dir(dirPath);
    if (!dir.IsOpened()) {
        return;
    }
    
    wxString filename;
    
    // Load XML files
    bool cont = dir.GetFirst(&filename, "*.xml", wxDIR_FILES);
    while (cont) {
        // Skip monsters.xml index file
        if (filename.Lower() != "monsters.xml") {
            wxString fullPath = dirPath + wxFileName::GetPathSeparator() + filename;
            try {
                HuntingMonsterData data;
                if (LoadMonsterFromXML(fullPath.ToStdString(), data) && !data.name.empty()) {
                    std::string key = data.name;
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                    m_monsterDatabase[key] = data;
                }
            } catch (...) {
                // Skip files that fail to load
            }
        }
        cont = dir.GetNext(&filename);
    }
    
    // Load Lua files
    cont = dir.GetFirst(&filename, "*.lua", wxDIR_FILES);
    while (cont) {
        // Skip files starting with # (examples/templates)
        if (!filename.StartsWith("#")) {
            wxString fullPath = dirPath + wxFileName::GetPathSeparator() + filename;
            try {
                HuntingMonsterData data;
                if (LoadMonsterFromLua(fullPath.ToStdString(), data) && !data.name.empty()) {
                    std::string key = data.name;
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                    m_monsterDatabase[key] = data;
                }
            } catch (...) {
                // Skip files that fail to load
            }
        }
        cont = dir.GetNext(&filename);
    }
    
    // Recursively load from subdirectories (limit depth to avoid infinite loops)
    static int recursionDepth = 0;
    if (recursionDepth < 5) {  // Max 5 levels deep
        cont = dir.GetFirst(&filename, wxEmptyString, wxDIR_DIRS);
        while (cont) {
            if (filename != "." && filename != "..") {
                wxString subDir = dirPath + wxFileName::GetPathSeparator() + filename;
                recursionDepth++;
                LoadMonstersFromDirectory(subDir.ToStdString());
                recursionDepth--;
            }
            cont = dir.GetNext(&filename);
        }
    }
}


bool HuntingCalculatorWindow::LoadMonsterFromXML(const std::string& filepath, HuntingMonsterData& data)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());
    
    if (!result) {
        return false;
    }
    
    pugi::xml_node monsterNode = doc.child("monster");
    if (!monsterNode) {
        return false;
    }
    
    // Basic attributes
    data.name = monsterNode.attribute("name").as_string();
    data.experience = monsterNode.attribute("experience").as_uint();
    
    // Health
    pugi::xml_node healthNode = monsterNode.child("health");
    if (healthNode) {
        data.health = healthNode.attribute("max").as_int(100);
    }
    
    // Look type and colors for sprite
    pugi::xml_node lookNode = monsterNode.child("look");
    if (lookNode) {
        data.outfit.lookType = lookNode.attribute("type").as_uint(0);
        data.outfit.lookHead = lookNode.attribute("head").as_uint(0);
        data.outfit.lookBody = lookNode.attribute("body").as_uint(0);
        data.outfit.lookLegs = lookNode.attribute("legs").as_uint(0);
        data.outfit.lookFeet = lookNode.attribute("feet").as_uint(0);
        data.outfit.lookAddon = lookNode.attribute("addons").as_uint(0);
    }
    
    // Defenses
    pugi::xml_node defensesNode = monsterNode.child("defenses");
    if (defensesNode) {
        data.armor = defensesNode.attribute("armor").as_int(0);
        data.defense = defensesNode.attribute("defense").as_int(0);
    }
    
    // Loot - parse recursively to handle nested containers
    pugi::xml_node lootNode = monsterNode.child("loot");
    if (lootNode) {
        ParseLootXML(lootNode, data.loot);
    }
    
    return !data.name.empty();
}

void HuntingCalculatorWindow::ParseLootXML(pugi::xml_node lootNode, std::vector<HuntingMonsterData::LootItem>& lootList)
{
    for (pugi::xml_node itemNode : lootNode.children("item")) {
        HuntingMonsterData::LootItem lootItem;
        
        // Get item id first
        if (itemNode.attribute("id")) {
            lootItem.id = itemNode.attribute("id").as_uint();
            
            // Try to get name from RME's item database
            if (g_items.isValidID(lootItem.id)) {
                const ItemType& itemType = g_items.getItemType(lootItem.id);
                lootItem.name = itemType.name;
            } else {
                lootItem.name = "Item #" + std::to_string(lootItem.id);
            }
        }
        
        // Override with name attribute if present
        if (itemNode.attribute("name")) {
            lootItem.name = itemNode.attribute("name").as_string();
        }
        
        lootItem.chance = itemNode.attribute("chance").as_uint(0);
        lootItem.countmax = itemNode.attribute("countmax").as_uint(1);
        
        if (!lootItem.name.empty() && lootItem.chance > 0) {
            lootList.push_back(lootItem);
        }
        
        // Handle nested loot (containers)
        if (itemNode.child("item")) {
            ParseLootXML(itemNode, lootList);
        }
    }
}

bool HuntingCalculatorWindow::LoadMonsterFromLua(const std::string& filepath, HuntingMonsterData& data)
{
    wxTextFile file;
    if (!file.Open(filepath)) {
        return false;
    }
    
    std::string content;
    for (wxString line = file.GetFirstLine(); !file.Eof(); line = file.GetNextLine()) {
        content += line.ToStdString() + "\n";
    }
    file.Close();
    
    // Parse Lua monster file using regex
    std::regex nameRegex(R"((?:monster\.)?name\s*=\s*[\"']([^\"']+)[\"'])");
    std::regex expRegex(R"((?:monster\.)?experience\s*=\s*(\d+))");
    std::regex healthRegex(R"((?:monster\.)?(?:health|maxHealth)\s*=\s*(\d+))");
    std::regex lookTypeRegex(R"(lookType\s*=\s*(\d+))");
    std::regex lookHeadRegex(R"(lookHead\s*=\s*(\d+))");
    std::regex lookBodyRegex(R"(lookBody\s*=\s*(\d+))");
    std::regex lookLegsRegex(R"(lookLegs\s*=\s*(\d+))");
    std::regex lookFeetRegex(R"(lookFeet\s*=\s*(\d+))");
    std::regex lookAddonsRegex(R"(lookAddons\s*=\s*(\d+))");
    std::regex armorRegex(R"(armor\s*=\s*(\d+))");
    std::regex defenseRegex(R"(defense\s*=\s*(\d+))");
    
    std::smatch match;
    
    if (std::regex_search(content, match, nameRegex)) {
        data.name = match[1].str();
    }
    
    if (std::regex_search(content, match, expRegex)) {
        data.experience = std::stoull(match[1].str());
    }
    
    if (std::regex_search(content, match, healthRegex)) {
        data.health = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookTypeRegex)) {
        data.outfit.lookType = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookHeadRegex)) {
        data.outfit.lookHead = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookBodyRegex)) {
        data.outfit.lookBody = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookLegsRegex)) {
        data.outfit.lookLegs = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookFeetRegex)) {
        data.outfit.lookFeet = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, lookAddonsRegex)) {
        data.outfit.lookAddon = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, armorRegex)) {
        data.armor = std::stoi(match[1].str());
    }
    
    if (std::regex_search(content, match, defenseRegex)) {
        data.defense = std::stoi(match[1].str());
    }
    
    // Parse loot table
    ParseLootLua(content, data.loot);
    
    return !data.name.empty();
}

void HuntingCalculatorWindow::ParseLootLua(const std::string& content, std::vector<HuntingMonsterData::LootItem>& lootList)
{
    // Find the loot table
    std::regex lootTableRegex(R"((?:monster\.)?loot\s*=\s*\{)");
    std::smatch lootMatch;
    
    if (!std::regex_search(content, lootMatch, lootTableRegex)) {
        return;
    }
    
    size_t lootStart = lootMatch.position() + lootMatch.length();
    std::string lootSection = content.substr(lootStart);
    
    // Patterns
    std::regex lootStringRegex(R"(\{\s*id\s*=\s*[\"']([^\"']+)[\"'])");
    std::regex lootNumericRegex(R"(\{\s*id\s*=\s*(\d+)\s*,)");
    std::regex chanceRegex(R"(chance\s*=\s*(\d+))");
    std::regex maxCountRegex(R"(maxCount\s*=\s*(\d+))");
    
    // Find all loot blocks
    size_t pos = 0;
    while (pos < lootSection.size()) {
        size_t braceStart = lootSection.find('{', pos);
        if (braceStart == std::string::npos) break;
        
        int braceCount = 1;
        size_t braceEnd = braceStart + 1;
        while (braceEnd < lootSection.size() && braceCount > 0) {
            if (lootSection[braceEnd] == '{') braceCount++;
            else if (lootSection[braceEnd] == '}') braceCount--;
            braceEnd++;
        }
        
        if (braceCount != 0) break;
        
        std::string block = lootSection.substr(braceStart, braceEnd - braceStart);
        
        std::smatch idMatch;
        HuntingMonsterData::LootItem lootItem;
        bool hasId = false;
        
        if (std::regex_search(block, idMatch, lootStringRegex)) {
            lootItem.name = idMatch[1].str();
            // Try to resolve item ID from name using RME's item database
            // Search through items to find matching name
            for (uint16_t itemId = 100; itemId < 50000; ++itemId) {
                if (g_items.isValidID(itemId)) {
                    const ItemType& itemType = g_items.getItemType(itemId);
                    std::string itemName = itemType.name;
                    std::string searchName = lootItem.name;
                    // Case-insensitive comparison
                    std::transform(itemName.begin(), itemName.end(), itemName.begin(), ::tolower);
                    std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);
                    if (itemName == searchName) {
                        lootItem.id = itemId;
                        break;
                    }
                }
            }
            hasId = true;
        }
        else if (std::regex_search(block, idMatch, lootNumericRegex)) {
            lootItem.id = std::stoul(idMatch[1].str());
            
            if (g_items.isValidID(lootItem.id)) {
                const ItemType& itemType = g_items.getItemType(lootItem.id);
                lootItem.name = itemType.name;
            } else {
                lootItem.name = "Item #" + std::to_string(lootItem.id);
            }
            hasId = true;
        }
        
        if (hasId) {
            std::smatch chanceMatch;
            if (std::regex_search(block, chanceMatch, chanceRegex)) {
                lootItem.chance = std::stoul(chanceMatch[1].str());
            }
            
            std::smatch maxCountMatch;
            if (std::regex_search(block, maxCountMatch, maxCountRegex)) {
                lootItem.countmax = std::stoul(maxCountMatch[1].str());
            } else {
                lootItem.countmax = 1;
            }
            
            if (lootItem.chance > 0 && !lootItem.name.empty()) {
                lootList.push_back(lootItem);
            }
        }
        
        pos = braceStart + 1;
    }
}

void HuntingCalculatorWindow::CalculateResults()
{
    double huntingDurationMinutes = m_huntingDuration->GetValue();
    double huntingDurationHours = huntingDurationMinutes / 60.0;  // Convert to hours for calculations
    double timePerKill = CalculateTimePerKill();
    
    // Get multipliers
    double expMult = 1.0;
    double lootMult = 1.0;
    double spawnMult = 1.0;
    
    if (m_applyMultipliers->IsChecked() && m_serverConfig.loaded) {
        expMult = m_serverConfig.rateExp;
        lootMult = m_serverConfig.rateLoot;
        spawnMult = m_serverConfig.rateSpawn;
    }
    
    // Respawn formula: only uses spawn rate from config.lua
    // Protect against division by zero
    double respawnMultiplier = (spawnMult > 0.0) ? spawnMult : 1.0;
    
    m_totalExpPerHour = 0;
    m_totalExp = 0;
    m_totalKills = 0;
    m_totalGoldPerHour = 0;
    m_aggregatedLoot.clear();
    
    std::map<uint32_t, AggregatedLoot> lootMapById;  // Use ID as key to avoid duplicates
    std::map<std::string, AggregatedLoot> lootMapByName;  // Fallback for items without ID
    
    for (auto& monster : m_monstersInArea) {
        // Calculate respawn time (protect against division by zero)
        monster.respawnTime = DEFAULT_RESPAWN_TIME / respawnMultiplier;
        if (monster.respawnTime <= 0.0) monster.respawnTime = 1.0;  // Minimum 1 second
        
        // In DPS mode, calculate time per kill based on monster health
        double effectiveTimePerKill = timePerKill;
        if (m_useDPSMode->IsChecked() && monster.health > 0) {
            double playerDPS = m_playerDPS->GetValue();
            if (playerDPS > 0.0) {
                effectiveTimePerKill = static_cast<double>(monster.health) / playerDPS;
            }
        }
        // Ensure minimum time per kill to avoid division by zero
        if (effectiveTimePerKill < 1.0) effectiveTimePerKill = 1.0;
        
        // Kills per hour (safe from division by zero now)
        double maxKillsPerHour = 3600.0 / effectiveTimePerKill;
        double respawnKillsPerHour = (3600.0 / monster.respawnTime) * monster.count;
        monster.killsPerHour = std::min(maxKillsPerHour, respawnKillsPerHour);
        
        // Experience per hour (with multiplier)
        monster.expPerHour = monster.killsPerHour * monster.experience * expMult;
        m_totalExpPerHour += monster.expPerHour;
        
        // Total kills and exp
        int totalKillsForMonster = static_cast<int>(monster.killsPerHour * huntingDurationHours);
        m_totalKills += totalKillsForMonster;
        m_totalExp += totalKillsForMonster * monster.experience * expMult;
        
        // Calculate loot (with multiplier)
        for (const auto& lootItem : monster.loot) {
            double dropRate = (lootItem.chance / 100000.0) * lootMult;
            if (dropRate > 1.0) dropRate = 1.0;  // Cap at 100%
            double expectedCount = totalKillsForMonster * dropRate * lootItem.countmax;
            
            // Calculate gold value for coins
            uint64_t coinValue = GetCoinValue(lootItem.id);
            if (coinValue > 0) {
                // This is a coin - calculate gold per hour
                double coinsPerHour = monster.killsPerHour * dropRate * lootItem.countmax;
                m_totalGoldPerHour += static_cast<uint64_t>(coinsPerHour * coinValue);
            }
            
            // Use ID as key if available, otherwise use name
            if (lootItem.id > 0) {
                if (lootMapById.find(lootItem.id) == lootMapById.end()) {
                    AggregatedLoot agg;
                    agg.name = lootItem.name;
                    agg.id = lootItem.id;
                    agg.expectedCount = expectedCount;
                    agg.dropRate = dropRate * 100.0;
                    lootMapById[lootItem.id] = agg;
                } else {
                    lootMapById[lootItem.id].expectedCount += expectedCount;
                }
            } else if (!lootItem.name.empty()) {
                std::string lowerName = lootItem.name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                
                // Check if this is a coin by name
                if (lowerName == "gold coin") {
                    double coinsPerHour = monster.killsPerHour * dropRate * lootItem.countmax;
                    m_totalGoldPerHour += static_cast<uint64_t>(coinsPerHour * GOLD_COIN_VALUE);
                } else if (lowerName == "platinum coin") {
                    double coinsPerHour = monster.killsPerHour * dropRate * lootItem.countmax;
                    m_totalGoldPerHour += static_cast<uint64_t>(coinsPerHour * PLATINUM_COIN_VALUE);
                } else if (lowerName == "crystal coin") {
                    double coinsPerHour = monster.killsPerHour * dropRate * lootItem.countmax;
                    m_totalGoldPerHour += static_cast<uint64_t>(coinsPerHour * CRYSTAL_COIN_VALUE);
                }
                
                if (lootMapByName.find(lowerName) == lootMapByName.end()) {
                    AggregatedLoot agg;
                    agg.name = lootItem.name;
                    agg.id = 0;
                    agg.expectedCount = expectedCount;
                    agg.dropRate = dropRate * 100.0;
                    lootMapByName[lowerName] = agg;
                } else {
                    lootMapByName[lowerName].expectedCount += expectedCount;
                }
            }
        }
    }
    
    // Convert loot maps to vector and sort
    for (const auto& pair : lootMapById) {
        m_aggregatedLoot.push_back(pair.second);
    }
    for (const auto& pair : lootMapByName) {
        m_aggregatedLoot.push_back(pair.second);
    }
    
    std::sort(m_aggregatedLoot.begin(), m_aggregatedLoot.end(),
              [](const AggregatedLoot& a, const AggregatedLoot& b) {
                  return a.expectedCount > b.expectedCount;
              });
}

double HuntingCalculatorWindow::CalculateTimePerKill()
{
    if (m_useDPSMode->IsChecked()) {
        // In DPS mode, we calculate per-monster, return a default
        return 10.0;
    }
    double timePerKill = m_timePerKill->GetValue();
    // Ensure minimum time per kill to avoid division by zero
    return (timePerKill >= 1.0) ? timePerKill : 1.0;
}

uint64_t HuntingCalculatorWindow::GetCoinValue(uint16_t itemId)
{
    switch (itemId) {
        case ITEM_GOLD_COIN:
            return GOLD_COIN_VALUE;
        case ITEM_PLATINUM_COIN:
            return PLATINUM_COIN_VALUE;
        case ITEM_CRYSTAL_COIN:
            return CRYSTAL_COIN_VALUE;
        default:
            return 0;
    }
}

std::string HuntingCalculatorWindow::FormatGold(uint64_t gold)
{
    std::ostringstream oss;
    
    if (gold >= 1000000000) {
        oss << std::fixed << std::setprecision(2) << (gold / 1000000000.0) << "kkk";
    } else if (gold >= 1000000) {
        oss << std::fixed << std::setprecision(2) << (gold / 1000000.0) << "kk";
    } else if (gold >= 1000) {
        oss << std::fixed << std::setprecision(1) << (gold / 1000.0) << "k";
    } else {
        oss << gold;
    }
    
    return oss.str();
}

std::string HuntingCalculatorWindow::FormatTime(double minutes)
{
    if (std::isnan(minutes) || std::isinf(minutes) || minutes <= 0) {
        return "N/A";
    }
    
    std::ostringstream oss;
    
    if (minutes < 1.0) {
        // Less than 1 minute - show seconds
        oss << std::fixed << std::setprecision(0) << (minutes * 60.0) << "s";
    } else if (minutes < 60.0) {
        // Less than 1 hour - show minutes
        oss << std::fixed << std::setprecision(1) << minutes << " min";
    } else if (minutes < 1440.0) {
        // Less than 1 day - show hours and minutes
        int hours = static_cast<int>(minutes / 60.0);
        int mins = static_cast<int>(minutes) % 60;
        oss << hours << "h " << mins << "m";
    } else {
        // 1 day or more - show days and hours
        int days = static_cast<int>(minutes / 1440.0);
        int hours = static_cast<int>((minutes - days * 1440.0) / 60.0);
        oss << days << "d " << hours << "h";
    }
    
    return oss.str();
}

double HuntingCalculatorWindow::GetTotalKillsPerHour() const
{
    double total = 0.0;
    for (const auto& monster : m_monstersInArea) {
        total += monster.killsPerHour;
    }
    return total;
}

double HuntingCalculatorWindow::CalculateExpectedTimeForItem(const AggregatedLoot& item) const
{
    // Expected time formula: E[T] = 1 / (p * r)
    // where p = drop probability per kill, r = kills per hour
    // Result is in hours, we convert to minutes
    
    if (item.dropRate <= 0.0) return std::numeric_limits<double>::infinity();
    
    double dropProbability = item.dropRate / 100.0;  // Convert from percentage
    double killsPerHour = GetTotalKillsPerHour();
    
    if (killsPerHour <= 0.0) return std::numeric_limits<double>::infinity();
    
    // E[T] in hours = 1 / (p * r)
    double expectedTimeHours = 1.0 / (dropProbability * killsPerHour);
    
    // Convert to minutes
    return expectedTimeHours * 60.0;
}

double HuntingCalculatorWindow::CalculateTimeForProbability(const AggregatedLoot& item, double probability) const
{
    // Time for X% probability of getting at least 1 item
    // P(≥1 drop in time t) = 1 - (1-p)^(r*t)
    // Solving for t: t = ln(1-P) / (r * ln(1-p))
    // Result is in hours, we convert to minutes
    
    if (item.dropRate <= 0.0 || probability <= 0.0 || probability >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }
    
    double dropProbability = item.dropRate / 100.0;  // Convert from percentage
    double killsPerHour = GetTotalKillsPerHour();
    
    if (killsPerHour <= 0.0 || dropProbability >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }
    
    // t = ln(1-P) / (r * ln(1-p))
    double numerator = std::log(1.0 - probability);
    double denominator = killsPerHour * std::log(1.0 - dropProbability);
    
    if (denominator >= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    
    double timeHours = numerator / denominator;
    
    // Convert to minutes
    return timeHours * 60.0;
}

void HuntingCalculatorWindow::UpdateKillModeUI()
{
    bool useDPS = m_useDPSMode->IsChecked();
    
    m_timePerKillLabel->Show(!useDPS);
    m_timePerKill->Show(!useDPS);
    m_dpsLabel->Show(useDPS);
    m_playerDPS->Show(useDPS);
    
    Layout();
    Refresh();
}

void HuntingCalculatorWindow::UpdateMonsterList()
{
    m_monsterList->SetMonsters(&m_monstersInArea);
}

void HuntingCalculatorWindow::UpdateLootList()
{
    m_lootList->SetLoot(&m_aggregatedLoot);
}

std::string HuntingCalculatorWindow::FormatNumber(double value)
{
    // Handle special cases
    if (std::isnan(value)) return "0";
    if (std::isinf(value)) return "∞";
    if (value < 0) value = 0;  // No negative values
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(0);
    
    if (value >= 1000000) {
        oss << std::setprecision(1) << (value / 1000000.0) << "M";
    } else if (value >= 1000) {
        oss << value;
        std::string str = oss.str();
        int insertPosition = str.length() - 3;
        while (insertPosition > 0) {
            str.insert(insertPosition, ",");
            insertPosition -= 3;
        }
        return str;
    } else {
        oss << value;
    }
    
    return oss.str();
}

void HuntingCalculatorWindow::OnCalculate(wxCommandEvent& event)
{
    // Safety check for UI controls
    if (!m_expPerHourLabel || !m_totalExpLabel || !m_totalKillsLabel || 
        !m_goldPerHourLabel || !m_monsterList || !m_lootList) {
        return;
    }
    
    // Load monsters from the specified area (simple and direct)
    LoadMonstersFromArea();
    
    if (m_monstersInArea.empty()) {
        return;  // Message already shown in LoadMonstersFromArea
    }
    
    // Load monster database only if directory is set and database is empty
    // Do this AFTER we know we have monsters to avoid unnecessary loading
    if (m_monsterDatabase.empty() && !m_monsterDirectory.empty()) {
        LoadMonsterDatabase();
    }
    
    // Match monsters with database info
    for (auto& monster : m_monstersInArea) {
        std::string lowerName = monster.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        auto it = m_monsterDatabase.find(lowerName);
        if (it != m_monsterDatabase.end()) {
            monster.experience = it->second.experience;
            monster.health = it->second.health;
            monster.loot = it->second.loot;
            if (monster.outfit.lookType == 0) {
                monster.outfit = it->second.outfit;
            }
        }
    }
    
    // Calculate results
    CalculateResults();
    
    // Update UI
    m_expPerHourLabel->SetLabel("Exp/Hour: " + FormatNumber(m_totalExpPerHour));
    m_totalExpLabel->SetLabel("Total Exp: " + FormatNumber(m_totalExp));
    m_totalKillsLabel->SetLabel("Kills: " + FormatNumber(m_totalKills));
    m_goldPerHourLabel->SetLabel("Gold/Hour: " + FormatGold(m_totalGoldPerHour));
    
    UpdateMonsterList();
    UpdateLootList();
}

void HuntingCalculatorWindow::OnClose(wxCommandEvent& event)
{
    EndModal(wxID_CANCEL);
}

void HuntingCalculatorWindow::OnMonsterDirChanged(wxFileDirPickerEvent& event)
{
    m_monsterDirectory = event.GetPath().ToStdString();
    m_monsterDatabase.clear();
    // Don't invalidate tile cache - only monster database changed
    SaveMapConfig();  // Save config when path changes
}

void HuntingCalculatorWindow::OnConfigFileChanged(wxFileDirPickerEvent& event)
{
    m_configFilePath = event.GetPath().ToStdString();
    if (LoadConfigLua(m_configFilePath)) {
        m_applyMultipliers->Enable(true);
        UpdateMultiplierLabels();
    } else {
        m_applyMultipliers->Enable(false);
        m_serverConfig.loaded = false;
        UpdateMultiplierLabels();
    }
    SaveMapConfig();  // Save config when path changes
}

void HuntingCalculatorWindow::OnApplyMultipliersChanged(wxCommandEvent& event)
{
    // Recalculate if we have data
    if (!m_monstersInArea.empty()) {
        CalculateResults();
        
        m_expPerHourLabel->SetLabel("Exp/Hour: " + FormatNumber(m_totalExpPerHour));
        m_totalExpLabel->SetLabel("Total Exp: " + FormatNumber(m_totalExp));
        m_totalKillsLabel->SetLabel("Kills: " + FormatNumber(m_totalKills));
        m_goldPerHourLabel->SetLabel("Gold/Hour: " + FormatGold(m_totalGoldPerHour));
        
        UpdateMonsterList();
        UpdateLootList();
    }
}

void HuntingCalculatorWindow::OnKillModeChanged(wxCommandEvent& event)
{
    UpdateKillModeUI();
    
    // Recalculate if we have data
    if (!m_monstersInArea.empty()) {
        CalculateResults();
        
        m_expPerHourLabel->SetLabel("Exp/Hour: " + FormatNumber(m_totalExpPerHour));
        m_totalExpLabel->SetLabel("Total Exp: " + FormatNumber(m_totalExp));
        m_totalKillsLabel->SetLabel("Kills: " + FormatNumber(m_totalKills));
        m_goldPerHourLabel->SetLabel("Gold/Hour: " + FormatGold(m_totalGoldPerHour));
        
        UpdateMonsterList();
        UpdateLootList();
    }
}

std::string HuntingCalculatorWindow::GetMapConfigPath()
{
    // Get the map filename and create a config path based on it
    try {
        Map& map = m_editor.getMap();
        wxString mapPath = wxString(map.getFilename());
        
        if (mapPath.IsEmpty()) {
            return "";
        }
        
        // Create config filename: mapname.hunting.xml
        wxFileName fn(mapPath);
        fn.SetExt("hunting.xml");
        
        return fn.GetFullPath().ToStdString();
    } catch (...) {
        return "";
    }
}

void HuntingCalculatorWindow::LoadMapConfig()
{
    std::string configPath;
    try {
        configPath = GetMapConfigPath();
    } catch (...) {
        return;
    }
    
    if (configPath.empty()) {
        return;
    }
    
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(configPath.c_str());
    
    if (!result) {
        return;  // File doesn't exist or can't be parsed, use defaults
    }
    
    pugi::xml_node root = doc.child("hunting_config");
    if (!root) {
        return;
    }
    
    // Load monster directory
    pugi::xml_node monsterDirNode = root.child("monster_directory");
    if (monsterDirNode) {
        std::string monsterDir = monsterDirNode.text().as_string();
        if (!monsterDir.empty() && wxDir::Exists(monsterDir)) {
            m_monsterDirectory = monsterDir;
            if (m_monsterDirPicker) {
                m_monsterDirPicker->SetPath(monsterDir);
            }
        }
    }
    
    // Load config.lua path
    pugi::xml_node configLuaNode = root.child("config_lua");
    if (configLuaNode) {
        std::string configLua = configLuaNode.text().as_string();
        if (!configLua.empty() && wxFileExists(configLua)) {
            m_configFilePath = configLua;
            if (m_configFilePicker) {
                m_configFilePicker->SetPath(configLua);
            }
            
            // Try to load the config
            try {
                if (LoadConfigLua(m_configFilePath)) {
                    if (m_applyMultipliers) {
                        m_applyMultipliers->Enable(true);
                    }
                    UpdateMultiplierLabels();
                }
            } catch (...) {
                // Ignore config load errors
            }
        }
    }
}

void HuntingCalculatorWindow::SaveMapConfig()
{
    std::string configPath = GetMapConfigPath();
    if (configPath.empty()) {
        return;
    }
    
    // Only save if we have something to save
    if (m_monsterDirectory.empty() && m_configFilePath.empty()) {
        return;
    }
    
    pugi::xml_document doc;
    
    // Add XML declaration
    pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
    decl.append_attribute("version") = "1.0";
    decl.append_attribute("encoding") = "UTF-8";
    
    pugi::xml_node root = doc.append_child("hunting_config");
    
    // Save monster directory
    if (!m_monsterDirectory.empty()) {
        pugi::xml_node monsterDirNode = root.append_child("monster_directory");
        monsterDirNode.text().set(m_monsterDirectory.c_str());
    }
    
    // Save config.lua path
    if (!m_configFilePath.empty()) {
        pugi::xml_node configLuaNode = root.append_child("config_lua");
        configLuaNode.text().set(m_configFilePath.c_str());
    }
    
    // Save to file
    doc.save_file(configPath.c_str());
}

std::string HuntingCalculatorWindow::GetAnalysisFolder()
{
    // Get the map directory and create hunting_analyzer subfolder
    Map& map = m_editor.getMap();
    wxString mapPath = wxString(map.getFilename());
    
    if (mapPath.IsEmpty()) {
        return "";
    }
    
    wxFileName fn(mapPath);
    wxString analyzerDir = fn.GetPath() + wxFileName::GetPathSeparator() + "hunting_analyzer";
    
    // Create directory if it doesn't exist
    if (!wxDir::Exists(analyzerDir)) {
        wxFileName::Mkdir(analyzerDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }
    
    return analyzerDir.ToStdString();
}

std::vector<std::string> HuntingCalculatorWindow::GetSavedAnalyses()
{
    std::vector<std::string> analyses;
    std::string folder = GetAnalysisFolder();
    
    if (folder.empty()) {
        return analyses;
    }
    
    wxDir dir(folder);
    if (!dir.IsOpened()) {
        return analyses;
    }
    
    wxString filename;
    bool cont = dir.GetFirst(&filename, "*.toml", wxDIR_FILES);
    while (cont) {
        // Remove .toml extension for display
        wxFileName fn(filename);
        analyses.push_back(fn.GetName().ToStdString());
        cont = dir.GetNext(&filename);
    }
    
    // Sort alphabetically
    std::sort(analyses.begin(), analyses.end());
    
    return analyses;
}

void HuntingCalculatorWindow::RefreshSavedAnalysesList()
{
    if (!m_savedAnalysesList) return;
    
    m_savedAnalysesList->Clear();
    m_savedAnalysesList->Append("-- Select --");
    
    std::vector<std::string> analyses = GetSavedAnalyses();
    for (const auto& name : analyses) {
        m_savedAnalysesList->Append(name);
    }
    
    m_savedAnalysesList->SetSelection(0);
}

void HuntingCalculatorWindow::SaveAnalysis(const std::string& name)
{
    std::string folder = GetAnalysisFolder();
    if (folder.empty()) {
        wxMessageBox("Please save the map first before saving analysis.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    
    if (name.empty()) {
        wxMessageBox("Please enter a name for the analysis.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    
    if (m_monstersInArea.empty()) {
        wxMessageBox("No analysis data to save. Please calculate first.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    
    std::string filepath = folder + std::string(1, static_cast<char>(wxFileName::GetPathSeparator())) + name + ".toml";
    
    // Build TOML content
    std::ostringstream toml;
    
    // Header
    toml << "# Hunting Analysis: " << name << "\n";
    toml << "# Generated by RME Hunting Calculator\n";
    toml << "# Date: " << wxDateTime::Now().FormatISOCombined() << "\n\n";
    
    // Summary section
    toml << "[summary]\n";
    toml << "name = \"" << name << "\"\n";
    toml << "total_exp_per_hour = " << std::fixed << std::setprecision(0) << m_totalExpPerHour << "\n";
    toml << "total_exp = " << std::fixed << std::setprecision(0) << m_totalExp << "\n";
    toml << "total_kills = " << m_totalKills << "\n";
    toml << "gold_per_hour = " << m_totalGoldPerHour << "\n";
    toml << "hunting_duration_minutes = " << m_huntingDuration->GetValue() << "\n";
    
    if (m_useDPSMode->IsChecked()) {
        toml << "calculation_mode = \"dps\"\n";
        toml << "player_dps = " << m_playerDPS->GetValue() << "\n";
    } else {
        toml << "calculation_mode = \"time\"\n";
        toml << "time_per_kill_seconds = " << m_timePerKill->GetValue() << "\n";
    }
    
    if (m_serverConfig.loaded && m_applyMultipliers->IsChecked()) {
        toml << "exp_rate = " << m_serverConfig.rateExp << "\n";
        toml << "loot_rate = " << m_serverConfig.rateLoot << "\n";
        toml << "spawn_rate = " << m_serverConfig.rateSpawn << "\n";
    }
    toml << "\n";
    
    // Monsters section
    toml << "[monsters]\n";
    toml << "count = " << m_monstersInArea.size() << "\n\n";
    
    for (size_t i = 0; i < m_monstersInArea.size(); ++i) {
        const auto& monster = m_monstersInArea[i];
        toml << "[[monsters.list]]\n";
        toml << "name = \"" << monster.name << "\"\n";
        toml << "count = " << monster.count << "\n";
        toml << "experience = " << monster.experience << "\n";
        toml << "respawn_time = " << std::fixed << std::setprecision(1) << monster.respawnTime << "\n";
        toml << "kills_per_hour = " << std::fixed << std::setprecision(1) << monster.killsPerHour << "\n";
        toml << "exp_per_hour = " << std::fixed << std::setprecision(0) << monster.expPerHour << "\n\n";
    }
    
    // Loot section
    toml << "[loot]\n";
    toml << "count = " << m_aggregatedLoot.size() << "\n\n";
    
    for (size_t i = 0; i < m_aggregatedLoot.size(); ++i) {
        const auto& loot = m_aggregatedLoot[i];
        toml << "[[loot.list]]\n";
        toml << "name = \"" << loot.name << "\"\n";
        toml << "id = " << loot.id << "\n";
        toml << "expected_count = " << std::fixed << std::setprecision(1) << loot.expectedCount << "\n";
        toml << "drop_rate = " << std::fixed << std::setprecision(2) << loot.dropRate << "\n\n";
    }
    
    // Write to file
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << toml.str();
        file.close();
        wxMessageBox("Analysis saved to:\n" + filepath, "Success", wxOK | wxICON_INFORMATION);
        RefreshSavedAnalysesList();
    } else {
        wxMessageBox("Failed to save analysis file.", "Error", wxOK | wxICON_ERROR);
    }
}

void HuntingCalculatorWindow::LoadAnalysis(const std::string& name)
{
    if (name.empty() || name == "-- Select --") {
        return;
    }
    
    std::string folder = GetAnalysisFolder();
    if (folder.empty()) {
        return;
    }
    
    std::string filepath = folder + std::string(1, static_cast<char>(wxFileName::GetPathSeparator())) + name + ".toml";
    
    // For now, just show the file content in a message box
    // A full implementation would parse the TOML and populate the UI
    std::ifstream file(filepath);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        // Show in a scrollable dialog
        wxDialog* dlg = new wxDialog(this, wxID_ANY, "Analysis: " + name, wxDefaultPosition, wxSize(600, 500), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        
        wxTextCtrl* text = new wxTextCtrl(dlg, wxID_ANY, buffer.str(), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        text->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        sizer->Add(text, 1, wxEXPAND | wxALL, 10);
        
        wxButton* closeBtn = new wxButton(dlg, wxID_OK, "Close");
        sizer->Add(closeBtn, 0, wxALIGN_CENTER | wxBOTTOM, 10);
        
        dlg->SetSizer(sizer);
        dlg->ShowModal();
        dlg->Destroy();
    } else {
        wxMessageBox("Failed to load analysis file.", "Error", wxOK | wxICON_ERROR);
    }
}

void HuntingCalculatorWindow::OnSaveAnalysis(wxCommandEvent& event)
{
    wxString name = m_analysisName->GetValue().Trim();
    if (name.IsEmpty()) {
        wxMessageBox("Please enter a name for the analysis.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    
    SaveAnalysis(name.ToStdString());
}

void HuntingCalculatorWindow::OnLoadAnalysis(wxCommandEvent& event)
{
    int selection = m_savedAnalysesList->GetSelection();
    if (selection <= 0) {
        return;
    }
    
    wxString name = m_savedAnalysesList->GetString(selection);
    LoadAnalysis(name.ToStdString());
}

// ============================================================================
// Cache Management
// ============================================================================

void HuntingCalculatorWindow::CacheSelectionTiles()
{
    m_cachedMonsters.clear();
    m_cacheValid = false;
    m_cachedTileCount = 0;
    
    // Safety check - make sure editor has a valid selection
    if (!m_editor.hasSelection()) {
        return;
    }
    
    const Selection& selection = m_editor.getSelection();
    if (selection.empty()) {
        return;
    }
    
    // Get tiles safely - create a copy of the tile pointers
    std::vector<Tile*> tilesToProcess;
    
    try {
        const TileSet& selectedTiles = selection.getTiles();
        tilesToProcess.reserve(selectedTiles.size());
        
        for (Tile* tile : selectedTiles) {
            // Skip null tiles
            if (tile == nullptr) {
                continue;
            }
            
            // Skip tiles with null location (critical safety check)
            if (tile->location == nullptr) {
                continue;
            }
            
            tilesToProcess.push_back(tile);
        }
    } catch (...) {
        // If anything goes wrong during tile collection, abort safely
        return;
    }
    
    if (tilesToProcess.empty()) {
        return;
    }
    
    // Detect the floor from the selected tiles (use the most common floor)
    // This fixes the issue where lasso selection only worked on floor 7
    std::map<int, int> floorCounts;
    for (Tile* tile : tilesToProcess) {
        if (tile != nullptr && tile->location != nullptr) {
            try {
                int tileZ = tile->getZ();
                floorCounts[tileZ]++;
            } catch (...) {
                continue;
            }
        }
    }
    
    // Find the floor with the most tiles (this is the floor the user selected on)
    int detectedFloor = m_cachedCurrentFloor;  // Default to cached floor
    int maxCount = 0;
    for (const auto& pair : floorCounts) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            detectedFloor = pair.first;
        }
    }
    
    // Update cached floor to the detected floor
    m_cachedCurrentFloor = detectedFloor;
    
    // Reserve space for monsters (estimate ~5% of tiles have monsters)
    m_cachedMonsters.reserve(tilesToProcess.size() / 20 + 10);
    
    // Process tiles - only cache monsters on the detected floor
    size_t tilesOnFloor = 0;
    
    for (Tile* tile : tilesToProcess) {
        // Double-check tile validity (defensive programming)
        if (tile == nullptr || tile->location == nullptr) {
            continue;
        }
        
        // Safely get tile Z coordinate
        int tileZ = 0;
        try {
            tileZ = tile->getZ();
        } catch (...) {
            continue;
        }
        
        // Only process tiles on the detected floor
        if (tileZ != detectedFloor) {
            continue;
        }
        
        ++tilesOnFloor;
        
        // Check if tile has a creature (not NPC)
        Creature* creature = tile->creature;
        if (creature != nullptr && !creature->isNpc()) {
            try {
                CachedMonsterData data;
                data.creatureName = creature->getName();
                data.outfit = creature->getLookType();
                m_cachedMonsters.push_back(data);
            } catch (...) {
                // Skip this creature if we can't get its data
                continue;
            }
        }
    }
    
    // Store tile count for display
    m_cachedTileCount = tilesOnFloor;
    
    // Shrink to fit to release unused reserved memory
    m_cachedMonsters.shrink_to_fit();
    m_cacheValid = true;
}

void HuntingCalculatorWindow::InvalidateCache()
{
    m_cacheValid = false;
    m_cachedMonsters.clear();
    m_cachedMonsters.shrink_to_fit();  // Release memory
}

// ============================================================================
// Progress Bar Helpers
// ============================================================================

void HuntingCalculatorWindow::ShowProgress(const wxString& message, int total)
{
    if (m_progressLabel && m_progressBar) {
        m_progressLabel->SetLabel(message);
        // Clamp to reasonable range to avoid overflow
        int safeTotal = (total > 1000000) ? 1000000 : total;
        m_progressBar->SetRange(safeTotal > 0 ? safeTotal : 100);
        m_progressBar->SetValue(0);
        m_progressLabel->Show();
        m_progressBar->Show();
        Layout();
    }
}

void HuntingCalculatorWindow::UpdateProgress(int current)
{
    if (m_progressBar) {
        m_progressBar->SetValue(current);
    }
}

void HuntingCalculatorWindow::HideProgress()
{
    if (m_progressLabel && m_progressBar) {
        m_progressLabel->Hide();
        m_progressBar->Hide();
        Layout();
    }
}
