#include "ScriptMgr.h"
#include "Config.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Player.h"
#include "WorldSession.h"
#include "GameObject.h"
#include "ObjectMgr.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "StringFormat.h"
#include "Map.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <ctime>

// ============================
// Settings / IDs (constants)
// ============================

// Sdílené template/loot entry per kvalita (držíme je v bezpečném custom rozsahu)
// CS entries
static constexpr uint32 TREASURE_ENTRY_BASIC_CS = 990200;
static constexpr uint32 TREASURE_ENTRY_RARE_CS  = 990201;
static constexpr uint32 TREASURE_ENTRY_EPIC_CS  = 990202;

// EN entries
static constexpr uint32 TREASURE_ENTRY_BASIC_EN = 990210;
static constexpr uint32 TREASURE_ENTRY_RARE_EN  = 990211;
static constexpr uint32 TREASURE_ENTRY_EPIC_EN  = 990212;

// Loot entry zůstává 1× na kvalitu (sdílený mezi CS/EN)
static constexpr uint32 TREASURE_LOOT_BASIC = 990200;
static constexpr uint32 TREASURE_LOOT_RARE  = 990201;
static constexpr uint32 TREASURE_LOOT_EPIC  = 990202;

// GO type chest
static constexpr uint8 GO_TYPE_CHEST = 3;

// LootState – používáme GO_JUST_DEACTIVATED (3)
static constexpr uint32 GO_JUST_DEACTIVATED_STATE = 3;

// Quality enum
enum class TreasureQuality : uint8 { BASIC = 1, RARE = 2, EPIC = 3 };

// -----------------------------
// Helpery
// -----------------------------

static inline std::string EscapeSQL(std::string s)
{
    // Minimální escapování apostrofů pro bezpečné vložení do SQL string literal
    size_t pos = 0;
    while ((pos = s.find('\'', pos)) != std::string::npos)
    {
        s.insert(pos, "'");
        pos += 2;
    }
    return s;
}

static inline std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static inline bool ParseQuality(std::string const& in, TreasureQuality& out)
{
    std::string s = ToLower(in);
    if (s == "basic") { out = TreasureQuality::BASIC; return true; }
    if (s == "rare")  { out = TreasureQuality::RARE;  return true; }
    if (s == "epic")  { out = TreasureQuality::EPIC;  return true; }
    return false;
}

static inline uint32 EntryByQualityLang(TreasureQuality q, bool cs)
{
    if (cs) {
        switch (q) {
            case TreasureQuality::BASIC: return TREASURE_ENTRY_BASIC_CS;
            case TreasureQuality::RARE:  return TREASURE_ENTRY_RARE_CS;
            case TreasureQuality::EPIC:  return TREASURE_ENTRY_EPIC_CS;
        }
    } else {
        switch (q) {
            case TreasureQuality::BASIC: return TREASURE_ENTRY_BASIC_EN;
            case TreasureQuality::RARE:  return TREASURE_ENTRY_RARE_EN;
            case TreasureQuality::EPIC:  return TREASURE_ENTRY_EPIC_EN;
        }
    }
    return cs ? TREASURE_ENTRY_BASIC_CS : TREASURE_ENTRY_BASIC_EN;
}

// Mapování kvality -> sdílený loot entry (společné pro CS/EN varianty truhel)
static inline uint32 LootEntryByQuality(TreasureQuality q)
{
    switch (q)
    {
        case TreasureQuality::BASIC: return TREASURE_LOOT_BASIC;
        case TreasureQuality::RARE:  return TREASURE_LOOT_RARE;
        case TreasureQuality::EPIC:  return TREASURE_LOOT_EPIC;
    }
    return TREASURE_LOOT_BASIC;
}

static inline std::string QualityName(TreasureQuality q, bool cs)
{
    if (cs)
    {
        switch (q)
        {
            case TreasureQuality::BASIC: return "Prostá truhla s pokladem";
            case TreasureQuality::RARE:  return "Vzácná truhla s pokladem";
            case TreasureQuality::EPIC:  return "Epická truhla s pokladem";
        }
    }
    else
    {
        switch (q)
        {
            case TreasureQuality::BASIC: return "Plain Treasure Chest";
            case TreasureQuality::RARE:  return "Rare Treasure Chest";
            case TreasureQuality::EPIC:  return "Epic Treasure Chest";
        }
    }
    return cs ? "Prostá truhla s pokladem" : "Plain Treasure Chest";
}

// ============================
// Config cache
// ============================

namespace TreasureConf
{
    bool enable = true;
    bool langCs = true;

    uint32 display_basic = 259;
    uint32 display_rare  = 259;
    uint32 display_epic  = 259;
	
	float size_basic = 1.0f;
    float size_rare  = 1.0f;
    float size_epic  = 1.0f;

    uint32 respawn_basic = 86400;
    uint32 respawn_rare  = 432000;
    uint32 respawn_epic  = 864000;
	
	static uint32 default_item_basic;
	static uint32 default_item_basic_chance;
	static uint32 default_item_rare;
	static uint32 default_item_rare_chance;
	static uint32 default_item_epic;
	static uint32 default_item_epic_chance;
	
	static uint32 default_item_basic_min = 1;
	static uint32 default_item_basic_max = 1;
	static uint32 default_item_rare_min  = 1;
	static uint32 default_item_rare_max  = 1;
	static uint32 default_item_epic_min  = 1;
	static uint32 default_item_epic_max  = 1;

    uint32 lockId = 43;
    uint32 d3 = 1, d10 = 1, d12 = 1, d15 = 1;

	void Load()
	{
		// Na této větvi AC použij GetOption<T>(key, def)
		enable = sConfigMgr->GetOption<bool>("Treasure.Enable", true);
	
		std::string lang = sConfigMgr->GetOption<std::string>("Treasure.Language", "cs");
		langCs = (ToLower(lang) == "cs");
	
		display_basic = sConfigMgr->GetOption<uint32>("Treasure.Basic.DisplayId", 259);
		display_rare  = sConfigMgr->GetOption<uint32>("Treasure.Rare.DisplayId", 259);
		display_epic  = sConfigMgr->GetOption<uint32>("Treasure.Epic.DisplayId", 259);
		
		size_basic = sConfigMgr->GetOption<float>("Treasure.Basic.Size", 1.0f);
        size_rare  = sConfigMgr->GetOption<float>("Treasure.Rare.Size",  1.0f);
        size_epic  = sConfigMgr->GetOption<float>("Treasure.Epic.Size",  1.0f);
	
		respawn_basic = sConfigMgr->GetOption<uint32>("Treasure.Basic.RespawnSeconds", 86400);
		respawn_rare  = sConfigMgr->GetOption<uint32>("Treasure.Rare.RespawnSeconds", 432000);
		respawn_epic  = sConfigMgr->GetOption<uint32>("Treasure.Epic.RespawnSeconds", 864000);
	
		lockId = sConfigMgr->GetOption<uint32>("Treasure.LockId", 43);
		d3  = sConfigMgr->GetOption<uint32>("Treasure.CHEST.Data3", 1);
		d10 = sConfigMgr->GetOption<uint32>("Treasure.CHEST.Data10", 1);
		d12 = sConfigMgr->GetOption<uint32>("Treasure.CHEST.Data12", 1);
		d15 = sConfigMgr->GetOption<uint32>("Treasure.CHEST.Data15", 1);
		
        default_item_basic        = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Basic", 0);
		default_item_basic_chance = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Basic.Chance", 100);
		default_item_basic_min    = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Basic.MinCount", 1);
		default_item_basic_max    = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Basic.MaxCount", 1);
		
		default_item_rare         = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Rare", 0);
		default_item_rare_chance  = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Rare.Chance", 100);
		default_item_rare_min     = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Rare.MinCount", 1);
		default_item_rare_max     = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Rare.MaxCount", 1);
		
		default_item_epic         = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Epic", 0);
		default_item_epic_chance  = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Epic.Chance", 100);
		default_item_epic_min     = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Epic.MinCount", 1);
		default_item_epic_max     = sConfigMgr->GetOption<uint32>("Treasure.DefaultItem.Epic.MaxCount", 1);		
	}
}

// ============================
// DB bootstrap (templates)
// ============================

static void UpsertTemplate(uint32 entry, std::string const& name, uint32 displayId, uint32 lootEntry, float size)
{
    std::string safeName = EscapeSQL(name);

    std::ostringstream ss;
    ss << "INSERT INTO gameobject_template "
       << "(entry, type, displayId, name, IconName, castBarCaption, unk1, size, "
       << "Data0, Data1, Data3, Data10, Data12, Data15, AIName, ScriptName, VerifiedBuild) VALUES ("
       << entry << ", 3, " << displayId << ", '" << safeName << "', '', '', '', " << size << ", "
       << TreasureConf::lockId << ", " << lootEntry << ", "
       << TreasureConf::d3 << ", " << TreasureConf::d10 << ", "
       << TreasureConf::d12 << ", " << TreasureConf::d15 << ", '', '', NULL) "
       << "ON DUPLICATE KEY UPDATE "
       << "type=VALUES(type), displayId=VALUES(displayId), name=VALUES(name), "
       << "size=VALUES(size), " // ➕ NEW: update size
       << "Data0=VALUES(Data0), Data1=VALUES(Data1), Data3=VALUES(Data3), "
       << "Data10=VALUES(Data10), Data12=VALUES(Data12), Data15=VALUES(Data15);";

    WorldDatabase.Execute(ss.str().c_str());
}

static void EnsureTemplatesAndLoot()
{
    if (!TreasureConf::enable)
        return;

    // Názvy
    std::string nBasicCS = "Prostá truhla s pokladem";
    std::string nRareCS  = "Vzácná truhla s pokladem";
    std::string nEpicCS  = "Epická truhla s pokladem";

    std::string nBasicEN = "Plain Treasure Chest";
    std::string nRareEN  = "Rare Treasure Chest";
    std::string nEpicEN  = "Epic Treasure Chest";

    // CS šablony (Data1 = sdílené loot entry)
    UpsertTemplate(TREASURE_ENTRY_BASIC_CS, nBasicCS, TreasureConf::display_basic, TREASURE_LOOT_BASIC, TreasureConf::size_basic);
	UpsertTemplate(TREASURE_ENTRY_RARE_CS,  nRareCS,  TreasureConf::display_rare,  TREASURE_LOOT_RARE,  TreasureConf::size_rare);
	UpsertTemplate(TREASURE_ENTRY_EPIC_CS,  nEpicCS,  TreasureConf::display_epic,  TREASURE_LOOT_EPIC,  TreasureConf::size_epic);

    // EN šablony (stejný loot entry dle kvality)
    UpsertTemplate(TREASURE_ENTRY_BASIC_EN, nBasicEN, TreasureConf::display_basic, TREASURE_LOOT_BASIC, TreasureConf::size_basic);
	UpsertTemplate(TREASURE_ENTRY_RARE_EN,  nRareEN,  TreasureConf::display_rare,  TREASURE_LOOT_RARE,  TreasureConf::size_rare);
	UpsertTemplate(TREASURE_ENTRY_EPIC_EN,  nEpicEN,  TreasureConf::display_epic,  TREASURE_LOOT_EPIC,  TreasureConf::size_epic);

	// Helper pro SQL insert jedné loot tabulky – bez % formátování
	auto makeLootInsert = [](uint32 lootEntry, uint32 itemId, double chance, uint32 minCount, uint32 maxCount, char const* comment)
	{
		std::ostringstream ss;
		ss << "INSERT IGNORE INTO `gameobject_loot_template` "
		<< "(`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) "
		<< "VALUES ("
		<< lootEntry << ","
		<< itemId << ","
		<< 0 << ","
		<< chance << ","
		<< 0 << ","
		<< 1 << ","
		<< 0 << ","
		<< minCount << ","
		<< maxCount << ","
		<< "'" << EscapeSQL(std::string(comment)) << "'"
		<< ");";
		return ss.str();
	};

    // Pokud je v konfáku výchozí item > 0, použij ho (Chance dle konfáku, typicky 100).
    // Jinak necháme původní placeholder Item=1 s Chance=0 (nepadá).
	uint32 basicItem    = TreasureConf::default_item_basic  > 0 ? TreasureConf::default_item_basic  : 1;
	double basicChance  = TreasureConf::default_item_basic  > 0 ? double(TreasureConf::default_item_basic_chance) : 0.0;
	uint32 basicMin     = TreasureConf::default_item_basic  > 0 ? TreasureConf::default_item_basic_min : 1;
	uint32 basicMax     = TreasureConf::default_item_basic  > 0 ? TreasureConf::default_item_basic_max : 1;
	
	uint32 rareItem     = TreasureConf::default_item_rare   > 0 ? TreasureConf::default_item_rare   : 1;
	double rareChance   = TreasureConf::default_item_rare   > 0 ? double(TreasureConf::default_item_rare_chance)  : 0.0;
	uint32 rareMin      = TreasureConf::default_item_rare   > 0 ? TreasureConf::default_item_rare_min : 1;
	uint32 rareMax      = TreasureConf::default_item_rare   > 0 ? TreasureConf::default_item_rare_max : 1;
	
	uint32 epicItem     = TreasureConf::default_item_epic   > 0 ? TreasureConf::default_item_epic   : 1;
	double epicChance   = TreasureConf::default_item_epic   > 0 ? double(TreasureConf::default_item_epic_chance)  : 0.0;
	uint32 epicMin      = TreasureConf::default_item_epic   > 0 ? TreasureConf::default_item_epic_min : 1;
	uint32 epicMax      = TreasureConf::default_item_epic   > 0 ? TreasureConf::default_item_epic_max : 1;
	
	// sanity: prohodit, když min > max
	if (basicMax < basicMin) std::swap(basicMin, basicMax);
	if (rareMax  < rareMin)  std::swap(rareMin,  rareMax);
	if (epicMax  < epicMin)  std::swap(epicMin,  epicMax);
	
	WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_BASIC, basicItem, basicChance, basicMin, basicMax, "treasure-basic-initial").c_str());
	WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_RARE,  rareItem,  rareChance,  rareMin,  rareMax,  "treasure-rare-initial").c_str());
	WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_EPIC,  epicItem,  epicChance,  epicMin,  epicMax,  "treasure-epic-initial").c_str());
}


// ============================
// WorldScript – načtení configu a bootstrap
// ============================

class TreasureWorldScript : public WorldScript
{
public:
    TreasureWorldScript() : WorldScript("TreasureWorldScript") {}

    void OnStartup() override
    {
        TreasureConf::Load();
        if (!TreasureConf::enable)
        {
            LOG_INFO("module", "mod_treasure: disabled by config");
            return;
        }
        EnsureTemplatesAndLoot();
        LOG_INFO("module", "mod_treasure: ready");
    }
};

// ============================
// GameObjectScript
// ============================

class TreasureGO : public GameObjectScript
{
public:
    TreasureGO() : GameObjectScript("TreasureGO") {}

    void OnLootStateChanged(GameObject* go, uint32 state, Unit* unit) override
    {
        if (!TreasureConf::enable)
            return;

        if (!go || state != GO_JUST_DEACTIVATED_STATE || go->GetGoType() != GO_TYPE_CHEST)
            return;

        if (!unit || unit->GetTypeId() != TYPEID_PLAYER)
            return;

        uint32 entry = go->GetEntry();
        TreasureQuality q;

        if (entry == TREASURE_ENTRY_BASIC_CS || entry == TREASURE_ENTRY_BASIC_EN)
            q = TreasureQuality::BASIC;
        else if (entry == TREASURE_ENTRY_RARE_CS || entry == TREASURE_ENTRY_RARE_EN)
            q = TreasureQuality::RARE;
        else if (entry == TREASURE_ENTRY_EPIC_CS || entry == TREASURE_ENTRY_EPIC_EN)
            q = TreasureQuality::EPIC;
        else
            return;

        // nic – loot (předměty) řeší DB přes gameobject_loot_template
        return;
    }
};


// ============================
// CommandScript – .treasure
// ============================

	using Acore::ChatCommands::ChatCommandTable;
	using Acore::ChatCommands::ChatCommandBuilder;
	using Acore::ChatCommands::Console;
	
	// ---- forward deklarace handlerů (free-funkce, ne členské) ----
	static bool Treasure_HandleAdd(ChatHandler* handler, char const* args);
	static bool Treasure_HandleList(ChatHandler* handler, char const* args);
	static bool Treasure_HandleRemove(ChatHandler* handler, char const* args);
	static bool Treasure_HandleAddItem(ChatHandler* handler, char const* args);
	static bool Treasure_HandleTp(ChatHandler* handler, char const* args);
	
	class TreasureCommands : public CommandScript
	{
	public:
		TreasureCommands() : CommandScript("TreasureCommands") {}
	
		ChatCommandTable GetCommands() const override
		{
			static ChatCommandTable treasureSub;
			if (treasureSub.empty())
			{
				// pořadí: name, handler, security, console
				treasureSub.emplace_back(ChatCommandBuilder("add",     Treasure_HandleAdd,     SEC_GAMEMASTER, Console::No));
				treasureSub.emplace_back(ChatCommandBuilder("list",    Treasure_HandleList,    SEC_GAMEMASTER, Console::No));
				treasureSub.emplace_back(ChatCommandBuilder("remove",  Treasure_HandleRemove,  SEC_GAMEMASTER, Console::No));
				treasureSub.emplace_back(ChatCommandBuilder("additem", Treasure_HandleAddItem, SEC_GAMEMASTER, Console::No));
				treasureSub.emplace_back(ChatCommandBuilder("tp",      Treasure_HandleTp,      SEC_GAMEMASTER, Console::No));
			}
	
			static ChatCommandTable root;
			if (root.empty())
			{
				// root s pod-příkazy (bez extra parametrů – security se řeší na leaf-commandech)
				root.emplace_back(ChatCommandBuilder("treasure", treasureSub));
			}
			return root;
		}
	};

	// .treasure add basic/rare/epic
	static bool Treasure_HandleAdd(ChatHandler* handler, char const* args)
	{
		if (!TreasureConf::enable)
		{
			handler->SendSysMessage("mod_treasure: disabled");
			return true;
		}
	
		Player* plr = handler->GetSession()->GetPlayer();
		if (!plr) return true;
	
		std::string a = args ? std::string(args) : "";
		a = ToLower(a);
		a.erase(std::remove_if(a.begin(), a.end(), ::isspace), a.end());
	
		TreasureQuality q;
		if (!ParseQuality(a, q))
		{
			handler->SendSysMessage(".treasure add basic/rare/epic");
			return true;
		}
	
		uint32 entry = EntryByQualityLang(q, TreasureConf::langCs);
		uint32 respawn = 0;
		switch (q)
		{
			case TreasureQuality::BASIC: respawn = TreasureConf::respawn_basic; break;
			case TreasureQuality::RARE:  respawn = TreasureConf::respawn_rare;  break;
			case TreasureQuality::EPIC:  respawn = TreasureConf::respawn_epic;  break;
		}
	
		// Insert do gameobject (DB persistent)
		uint32 map  = plr->GetMapId();
		uint32 zone = plr->GetZoneId();
		uint32 area = plr->GetAreaId();
		float x = plr->GetPositionX();
		float y = plr->GetPositionY();
		float z = plr->GetPositionZ();
		float o = plr->GetOrientation();
	
		// --- 1) Unikátní marker pro tenhle spawn (do sloupce Comment) ---
		uint32 pGuidLow = plr->GetGUID().GetCounter();
		uint32 now      = uint32(::time(nullptr));
		std::ostringstream mss;
		mss << "treasure-auto-" << pGuidLow << "-" << now;
		std::string marker = mss.str();
	
		// --- 2) INSERT s markerem v Comment ---
		{
			std::ostringstream iq;
			iq << "INSERT INTO `gameobject` "
				"(`id`,`map`,`zoneId`,`areaId`,`spawnMask`,`phaseMask`,"
				"`position_x`,`position_y`,`position_z`,`orientation`,"
				"`rotation0`,`rotation1`,`rotation2`,`rotation3`,"
				"`spawntimesecs`,`animprogress`,`state`,`ScriptName`,`VerifiedBuild`,`Comment`) "
				"VALUES ("
			<< entry << ","
			<< map << "," << zone << "," << area << ","
			<< 1 << "," << 1 << ","
			<< x << "," << y << "," << z << "," << o << ","
			<< 0 << "," << 0 << "," << 0 << "," << 1 << ","
			<< int32(respawn) << "," << 0 << "," << 1 << ","
			<< "''" << ",NULL,"
			<< "'" << EscapeSQL(marker) << "');";
			WorldDatabase.Execute(iq.str().c_str());
		}
	
		// --- 3) Spolehlivě načti GUID podle markeru ---
		uint32 guid = 0;
		{
			std::ostringstream sq;
			sq << "SELECT `guid` FROM `gameobject` "
				"WHERE `Comment`='" << EscapeSQL(marker) << "' "
				"ORDER BY `guid` DESC LIMIT 1;";
			if (QueryResult gqr = WorldDatabase.Query(sq.str().c_str()))
				guid = (*gqr)[0].Get<uint32>();
		}
	
		// Nouzová pojistka (nemělo by být třeba)
		if (!guid)
		{
			if (QueryResult gqr2 = WorldDatabase.Query("SELECT `guid` FROM `gameobject` ORDER BY `guid` DESC LIMIT 1;"))
				guid = (*gqr2)[0].Get<uint32>();
		}
	
		// --- 4) (volitelně) přepiš Comment na „hezký“ text ---
		{
			std::ostringstream uq;
			uq << "UPDATE `gameobject` SET `Comment`='"
			<< (TreasureConf::langCs ? "Treasure modul – auto" : "Treasure module – auto")
			<< "' WHERE `guid`=" << guid << " LIMIT 1;";
			WorldDatabase.Execute(uq.str().c_str());
		}
	
		// Zapiš do customs.treasure_chest (už s korektním GUID)
		{
			std::ostringstream cq;
			cq << "INSERT INTO `customs`.`treasure_chest` "
				"(`quality`,`guid`,`map`,`zoneId`,`areaId`,`position_x`,`position_y`,`position_z`,`orientation`,`respawnSecs`,`created_by`) VALUES ("
			<< uint32(q) << ","
			<< guid << ","
			<< map << "," << zone << "," << area << ","
			<< x << "," << y << "," << z << "," << o << ","
			<< int32(respawn) << ","
			<< handler->GetSession()->GetAccountId() << ");";
			WorldDatabase.Execute(cq.str().c_str());
		}
	
		// ===== Runtime spawn bez restartu =====
		Map* mapPtr = plr->GetMap();
		if (mapPtr && guid != 0)
		{
			GameObject* ngo = new GameObject();
		
			// quaternion kolem Z (x=0, y=0, z=sin(o/2), w=cos(o/2))
			float sinHalf = std::sin(o * 0.5f);
			float cosHalf = std::cos(o * 0.5f);
			G3D::Quat rot;
			rot.x = 0.0f;
			rot.y = 0.0f;
			rot.z = sinHalf;
			rot.w = cosHalf;
		
			uint32 phaseMask = plr->GetPhaseMask();
		
			// 1) vytvoř objekt
			if (!ngo->Create(guid, entry, mapPtr, phaseMask,
							x, y, z, o, rot,
							/*animprogress*/ 0,
							GO_STATE_READY, /*artKit*/ 0))
			{
				LOG_ERROR("module", "mod_treasure: Create() failed for guid={} entry={}", guid, entry);
				handler->SendSysMessage(TreasureConf::langCs ? "Chyba: Create() selhalo." : "Error: Create() failed.");
				delete ngo;
			}
			else
			{
				// Ujisti se, že je teď hned „spawnutý“ (ne jen naplánovaný k respawnu)
				// Nastavíme respawn delay (jak dlouho po sebrání), ale okamžitý stav necháme spawnutý.
				ngo->SetRespawnDelay(respawn);   // jak dlouho po despawnu (lootnutí) má znovu naskočit
				ngo->SetRespawnTime(0);          // teď hned je aktivní
				ngo->SetGoState(GO_STATE_READY); // vizuální stav „otevřitelné truhly“
		
				// 2) přidej do mapy
				if (!mapPtr->AddToMap(ngo))
				{
					LOG_ERROR("module", "mod_treasure: AddToMap() failed for guid={} entry={}", guid, entry);
					handler->SendSysMessage(TreasureConf::langCs ? "Chyba: AddToMap() selhalo." : "Error: AddToMap() failed.");
					delete ngo;
				}
				else
				{
					// nech objekt aktivní (užitečné, když je hráč blízko)
					ngo->setActive(true);
					// phase ještě jednou „natvrdo“, kdyby Create převzalo něco jiného
					ngo->SetPhaseMask(phaseMask, true);
				}
			}
		}
		else
		{
			handler->SendSysMessage(TreasureConf::langCs ? "Chyba: Map* neplatná nebo GUID=0." : "Error: invalid Map* or GUID=0.");
		}

		// zpráva
		std::ostringstream ok;
		if (TreasureConf::langCs)
			ok << "Přidána truhla (kvalita " << a << "), GUID " << guid << ". ID v customs přiřazeno automaticky.";
		else
			ok << "Chest added (quality " << a << "), GUID " << guid << ". Custom ID assigned automatically.";
		handler->SendSysMessage(ok.str().c_str());
	
		return true;
	}

	
	// .treasure list basic/rare/epic
	static bool Treasure_HandleList(ChatHandler* handler, char const* args)
	{
		std::string a = args ? std::string(args) : "";
		a = ToLower(a);
		a.erase(std::remove_if(a.begin(), a.end(), ::isspace), a.end());
	
		TreasureQuality q;
		if (!ParseQuality(a, q))
		{
			handler->SendSysMessage(".treasure list basic/rare/epic");
			return true;
		}
	
		std::ostringstream qy;
		qy << "SELECT id FROM customs.treasure_chest WHERE quality=" << uint32(q) << " ORDER BY id ASC;";
		QueryResult qr = WorldDatabase.Query(qy.str().c_str());
	
		uint32 count = 0;
		std::ostringstream ids;
		if (qr)
		{
			do
			{
				Field* f = qr->Fetch();
				uint32 id = f[0].Get<uint32>();
				if (count) ids << ", ";
				ids << id;
				++count;
			} while (qr->NextRow());
		}
	
		std::ostringstream out;
		if (TreasureConf::langCs)
		{
			out << "Počet truhel (" << a << "): " << count;
			handler->SendSysMessage(out.str().c_str());
			if (count) handler->SendSysMessage(("ID: " + ids.str()).c_str());
		}
		else
		{
			out << "Chest count (" << a << "): " << count;
			handler->SendSysMessage(out.str().c_str());
			if (count) handler->SendSysMessage(("IDs: " + ids.str()).c_str());
		}
		return true;
	}
	
	// .treasure remove ID
	static bool Treasure_HandleRemove(ChatHandler* handler, char const* args)
	{
		std::string s = args ? std::string(args) : "";
		s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
		if (s.empty())
		{
			handler->SendSysMessage(".treasure remove <ID>");
			return true;
		}
	
		uint32 id = uint32(std::strtoul(s.c_str(), nullptr, 10));
	
		// SELECT guid
		std::string sel = "SELECT guid FROM customs.treasure_chest WHERE id=" + std::to_string(id) + ";";
		QueryResult qr = WorldDatabase.Query(sel.c_str());
	
		uint32 guid = 0;
		if (qr)
			guid = (*qr)[0].Get<uint32>();
	
		if (!guid)
		{
			handler->SendSysMessage(TreasureConf::langCs ? "Nenalezeno." : "Not found.");
			return true;
		}
	
		// DELETEs
		std::string delGo   = "DELETE FROM gameobject WHERE guid=" + std::to_string(guid) + ";";
		std::string delCust = "DELETE FROM customs.treasure_chest WHERE id=" + std::to_string(id) + ";";
		WorldDatabase.Execute(delGo.c_str());
		WorldDatabase.Execute(delCust.c_str());
	
		std::ostringstream msg;
		if (TreasureConf::langCs)
			msg << "Truhla ID " << id << " odstraněna (GUID " << guid << ").";
		else
			msg << "Chest ID " << id << " removed (GUID " << guid << ").";
		handler->SendSysMessage(msg.str().c_str());
		return true;
	}
	
	// .treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>
	static bool Treasure_HandleAddItem(ChatHandler* handler, char const* args)
	{
		std::string a = args ? std::string(args) : "";
		std::istringstream ss(a);
		std::string si, srange, schance, sq;
		ss >> si >> srange >> schance >> sq;
	
		if (si.empty() || srange.empty() || schance.empty() || sq.empty())
		{
			if (TreasureConf::langCs)
				handler->SendSysMessage(".treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>");
			else
				handler->SendSysMessage(".treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>");
			return true;
		}
	
		// itemId
		uint32 itemId = uint32(std::strtoul(si.c_str(), nullptr, 10));
	
		// parse range "min-max" nebo "n"
		uint32 minCount = 1, maxCount = 1;
		{
			size_t dash = srange.find('-');
			if (dash == std::string::npos)
			{
				// jen jedno číslo
				uint32 n = uint32(std::strtoul(srange.c_str(), nullptr, 10));
				minCount = maxCount = (n > 0 ? n : 1);
			}
			else
			{
				std::string smin = srange.substr(0, dash);
				std::string smax = srange.substr(dash + 1);
				uint32 mn = uint32(std::strtoul(smin.c_str(), nullptr, 10));
				uint32 mx = uint32(std::strtoul(smax.c_str(), nullptr, 10));
				if (mn == 0) mn = 1;
				if (mx == 0) mx = 1;
				if (mx < mn) std::swap(mn, mx);
				minCount = mn;
				maxCount = mx;
			}
		}
	
		// chance (povolit čárku)
		for (char& ch : schance) if (ch == ',') ch = '.';
		char* endPtr = nullptr;
		double chanceD = std::strtod(schance.c_str(), &endPtr);
		if (endPtr == schance.c_str() || *endPtr != '\0')
		{
			if (TreasureConf::langCs)
				handler->SendSysMessage("Šance musí být číslo (oddělovač tečka nebo čárka).");
			else
				handler->SendSysMessage("Chance must be a number (use dot or comma).");
			return true;
		}
		if (chanceD < 0.0 || chanceD > 100.0)
		{
			if (TreasureConf::langCs)
				handler->SendSysMessage("Šance musí být v rozmezí 0 až 100.");
			else
				handler->SendSysMessage("Chance must be between 0 and 100.");
			return true;
		}
		float chance = float(chanceD);
	
		// kvalita
		TreasureQuality q;
		if (!ParseQuality(sq, q))
		{
			if (TreasureConf::langCs)
				handler->SendSysMessage(".treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>");
			else
				handler->SendSysMessage(".treasure additem <itemId> <min-max/count> <chance> <basic/rare/epic>");
			return true;
		}
	
		uint32 lootEntry = LootEntryByQuality(q);
	
		// UPSERT záznamu do gameobject_loot_template s naším min/max
		std::ostringstream iq;
		iq << "INSERT INTO `gameobject_loot_template` "
		<< "(`Entry`,`Item`,`Reference`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES ("
		<< lootEntry << ","
		<< itemId   << ","
		<< 0        << ","
		<< chance   << ","
		<< 0        << ","
		<< 1        << ","
		<< 0        << ","
		<< minCount << ","
		<< maxCount << ","
		<< "'treasure-" << ToLower(sq) << "') "
		<< "ON DUPLICATE KEY UPDATE "
		<< "`Chance`=VALUES(`Chance`),"
		<< "`MinCount`=VALUES(`MinCount`),"
		<< "`MaxCount`=VALUES(`MaxCount`),"
		<< "`Comment`=VALUES(`Comment`);";
	
		WorldDatabase.Execute(iq.str().c_str());
	
		std::ostringstream ok;
		if (TreasureConf::langCs)
			ok << "Přidán item " << itemId << " (" << minCount << "-" << maxCount << ") se šancí " << chance << "% do lootu kvality " << ToLower(sq) << ".";
		else
			ok << "Added item " << itemId << " (" << minCount << "-" << maxCount << ") with chance " << chance << "% to " << ToLower(sq) << " loot.";
		handler->SendSysMessage(ok.str().c_str());
		return true;
	}
	
	// .treasure tp ID
	static bool Treasure_HandleTp(ChatHandler* handler, char const* args)
	{
		std::string s = args ? std::string(args) : "";
		s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
		if (s.empty())
		{
			handler->SendSysMessage(".treasure tp <ID>");
			return true;
		}
	
		uint32 id = uint32(std::strtoul(s.c_str(), nullptr, 10));
		std::string q =
			"SELECT map, position_x, position_y, position_z, orientation "
			"FROM customs.treasure_chest WHERE id=" + std::to_string(id) + ";";
		QueryResult qr = WorldDatabase.Query(q.c_str());
		if (!qr)
		{
			handler->SendSysMessage(TreasureConf::langCs ? "Nenalezeno." : "Not found.");
			return true;
		}
	
		Field* f = qr->Fetch();
		uint32 map = f[0].Get<uint32>();
		float x = f[1].Get<float>();
		float y = f[2].Get<float>();
		float z = f[3].Get<float>();
		float o = f[4].Get<float>();
	
		Player* plr = handler->GetSession()->GetPlayer();
		if (!plr) return true;
	
		plr->TeleportTo(map, x, y, z, o);
		return true;
	}

// ============================
// registrace
// ============================

void Addmod_treasureScripts()
{
    new TreasureWorldScript();
    //new TreasureGO();
    new TreasureCommands();
}
