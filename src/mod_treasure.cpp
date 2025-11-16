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
#include "GameTime.h"

#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <ctime>
#include <unordered_map>

/* ============================
 * Settings / IDs (constants)
 * ============================ */

static constexpr uint32 TREASURE_ENTRY_BASIC_CS = 990200;
static constexpr uint32 TREASURE_ENTRY_RARE_CS  = 990201;
static constexpr uint32 TREASURE_ENTRY_EPIC_CS  = 990202;
static constexpr uint32 TREASURE_ENTRY_BASIC_EN = 990210;
static constexpr uint32 TREASURE_ENTRY_RARE_EN  = 990211;
static constexpr uint32 TREASURE_ENTRY_EPIC_EN  = 990212;
static constexpr uint32 TREASURE_LOOT_BASIC = 990200;
static constexpr uint32 TREASURE_LOOT_RARE  = 990201;
static constexpr uint32 TREASURE_LOOT_EPIC  = 990202;
static constexpr uint8 GO_TYPE_CHEST = 3;
static constexpr uint32 GO_JUST_DEACTIVATED_STATE = 3;
enum class TreasureQuality : uint8 { BASIC = 1, RARE = 2, EPIC = 3 };

/* -----------------------------
 * Helpery
 * ----------------------------- */

static inline std::string EscapeSQL(std::string s)
{
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

/* ============================
 * Config cache
 * ============================ */

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

    bool  notify_enable        = true;
    float notify_radius_basic  = 20.0f;
    float notify_radius_rare   = 20.0f;
    float notify_radius_epic   = 20.0f;

    std::string notify_msg_basic_cs = "|cffFFD700Cítíš, že se poblíž nachází prostá truhla s pokladem!|r";
    std::string notify_msg_rare_cs  = "|cffFFD700Cítíš, že se poblíž nachází vzácná truhla s pokladem!|r";
    std::string notify_msg_epic_cs  = "|cffFFD700Cítíš, že se poblíž nachází epická truhla s pokladem!|r";

    std::string notify_msg_basic_en = "|cffFFD700You sense a plain treasure chest nearby!|r";
    std::string notify_msg_rare_en  = "|cffFFD700You sense a rare treasure chest nearby!|r";
    std::string notify_msg_epic_en  = "|cffFFD700You sense an epic treasure chest nearby!|r";

    static std::vector<std::pair<uint32, uint32>> disableAccRanges;

    static inline void trim(std::string& s)
    {
        auto notsp = [](int ch){ return !std::isspace(ch); };
        while (!s.empty() && !notsp(s.front())) s.erase(s.begin());
        while (!s.empty() && !notsp(s.back()))  s.pop_back();
    }

    static void ParseDisableRanges(std::string const& cfg)
    {
        disableAccRanges.clear();
        if (cfg.empty())
            return;

        std::string s = cfg;
        for (char& c : s) if (c == ',') c = ' ';

        std::istringstream iss(s);
        std::string tok;
        while (iss >> tok)
        {
            trim(tok);
            if (tok.empty())
                continue;

            size_t dash = tok.find('-');
            uint32 a = 0, b = 0;
            if (dash == std::string::npos)
            {
                a = b = uint32(std::strtoul(tok.c_str(), nullptr, 10));
            }
            else
            {
                std::string A = tok.substr(0, dash);
                std::string B = tok.substr(dash + 1);
                trim(A); trim(B);
                a = uint32(std::strtoul(A.c_str(), nullptr, 10));
                b = uint32(std::strtoul(B.c_str(), nullptr, 10));
                if (b < a) std::swap(a, b);
            }

            if (a > 0 && b > 0)
                disableAccRanges.emplace_back(a, b);
        }
    }

    static inline bool IsDisabledAccount(uint32 accId)
    {
        if (accId == 0) return false;
        for (auto const& r : disableAccRanges)
            if (accId >= r.first && accId <= r.second)
                return true;
        return false;
    }

    void Load()
    {
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

        // +++ načtení nových klíčů +++
        notify_enable        = sConfigMgr->GetOption<bool> ("Treasure.Notify.Enable", true);
        notify_radius_basic  = sConfigMgr->GetOption<float>("Treasure.Notify.Radius.basic", 20.0f);
        notify_radius_rare   = sConfigMgr->GetOption<float>("Treasure.Notify.Radius.rare",  20.0f);
        notify_radius_epic   = sConfigMgr->GetOption<float>("Treasure.Notify.Radius.epic",  20.0f);

        // Texty zpráv
        notify_msg_basic_cs = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Basic.cs", notify_msg_basic_cs);
        notify_msg_rare_cs  = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Rare.cs",  notify_msg_rare_cs);
        notify_msg_epic_cs  = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Epic.cs",  notify_msg_epic_cs);

        notify_msg_basic_en = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Basic.en", notify_msg_basic_en);
        notify_msg_rare_en  = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Rare.en",  notify_msg_rare_en);
        notify_msg_epic_en  = sConfigMgr->GetOption<std::string>("Treasure.Notify.Msg.Epic.en",  notify_msg_epic_en);
		
        std::string dis = sConfigMgr->GetOption<std::string>("Treasure.Notify.RndBots.Disable", "");
        ParseDisableRanges(dis);
    }
}

/* ============================
 * DB bootstrap (templates)
 * ============================ */

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
       << "size=VALUES(size), "
       << "Data0=VALUES(Data0), Data1=VALUES(Data1), Data3=VALUES(Data3), "
       << "Data10=VALUES(Data10), Data12=VALUES(Data12), Data15=VALUES(Data15);";

    WorldDatabase.Execute(ss.str().c_str());
}

static void EnsureTemplatesAndLoot()
{
    if (!TreasureConf::enable)
        return;

    std::string nBasicCS = "Prostá truhla s pokladem";
    std::string nRareCS  = "Vzácná truhla s pokladem";
    std::string nEpicCS  = "Epická truhla s pokladem";

    std::string nBasicEN = "Plain Treasure Chest";
    std::string nRareEN  = "Rare Treasure Chest";
    std::string nEpicEN  = "Epic Treasure Chest";

    UpsertTemplate(TREASURE_ENTRY_BASIC_CS, nBasicCS, TreasureConf::display_basic, TREASURE_LOOT_BASIC, TreasureConf::size_basic);
    UpsertTemplate(TREASURE_ENTRY_RARE_CS,  nRareCS,  TreasureConf::display_rare,  TREASURE_LOOT_RARE,  TreasureConf::size_rare);
    UpsertTemplate(TREASURE_ENTRY_EPIC_CS,  nEpicCS,  TreasureConf::display_epic,  TREASURE_LOOT_EPIC,  TreasureConf::size_epic);

    UpsertTemplate(TREASURE_ENTRY_BASIC_EN, nBasicEN, TreasureConf::display_basic, TREASURE_LOOT_BASIC, TreasureConf::size_basic);
    UpsertTemplate(TREASURE_ENTRY_RARE_EN,  nRareEN,  TreasureConf::display_rare,  TREASURE_LOOT_RARE,  TreasureConf::size_rare);
    UpsertTemplate(TREASURE_ENTRY_EPIC_EN,  nEpicEN,  TreasureConf::display_epic,  TREASURE_LOOT_EPIC,  TreasureConf::size_epic);

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
	
    if (basicMax < basicMin) std::swap(basicMin, basicMax);
    if (rareMax  < rareMin)  std::swap(rareMin,  rareMax);
    if (epicMax  < epicMin)  std::swap(epicMin,  epicMax);
	
    WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_BASIC, basicItem, basicChance, basicMin, basicMax, "treasure-basic-initial").c_str());
    WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_RARE,  rareItem,  rareChance,  rareMin,  rareMax,  "treasure-rare-initial").c_str());
    WorldDatabase.Execute(makeLootInsert(TREASURE_LOOT_EPIC,  epicItem,  epicChance,  epicMin,  epicMax,  "treasure-epic-initial").c_str());
}

static uint32 GetAccountIdSafe(Player* plr)
{
    if (!plr) return 0;

    if (WorldSession* sess = plr->GetSession())
        return sess->GetAccountId();

    static std::unordered_map<uint32, uint32> cache;
    uint32 low = plr->GetGUID().GetCounter();

    auto it = cache.find(low);
    if (it != cache.end())
        return it->second;

    std::ostringstream q;
    q << "SELECT `account` FROM `characters` WHERE `guid`=" << low << " LIMIT 1;";
    if (QueryResult qr = CharacterDatabase.Query(q.str().c_str()))
    {
        uint32 acc = (*qr)[0].Get<uint32>();
        cache.emplace(low, acc);
        return acc;
    }

    cache.emplace(low, 0);
    return 0;
}


/* ============================
 * WorldScript – načtení configu a bootstrap
 * ============================ */

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

/* ============================
 * GameObjectScript
 * ============================ */

class TreasureChestPersistGO : public GameObjectScript
{
public:
    TreasureChestPersistGO() : GameObjectScript("TreasureChestPersistGO") {}

    void OnLootStateChanged(GameObject* go, uint32 state, Unit* unit) override
    {
        if (!TreasureConf::enable)
            return;
    
        if (!go || state != GO_JUST_DEACTIVATED_STATE || go->GetGoType() != GO_TYPE_CHEST)
            return;
    
        uint32 entry = go->GetEntry();
        bool ours =
            entry == TREASURE_ENTRY_BASIC_CS || entry == TREASURE_ENTRY_BASIC_EN ||
            entry == TREASURE_ENTRY_RARE_CS  || entry == TREASURE_ENTRY_RARE_EN  ||
            entry == TREASURE_ENTRY_EPIC_CS  || entry == TREASURE_ENTRY_EPIC_EN;
    
        if (!ours)
            return;
    
        uint32 const delay = go->GetRespawnDelay();
        if (delay == 0)
            return;
    
        uint64 when = static_cast<uint64>(GameTime::GetGameTime().count()) + static_cast<uint64>(delay);
        if (Map* map = go->GetMap())
        {
            time_t when_t = static_cast<time_t>(when);
            map->SaveGORespawnTime(go->GetSpawnId(), when_t);
        }
    }
};

/* ============================
 * Movement-driven proximity notify (edge detection)
 * ============================ */

namespace {
	struct TrackState
	{
		bool  wasBasic = false;
		bool  wasRare  = false;
		bool  wasEpic  = false;
	
		uint32 lastMap  = 0;
		float  lastX = 0.f, lastY = 0.f, lastZ = 0.f;
		bool   inited = false;
	};

    std::unordered_map<uint32, TrackState> g_tracks;

    inline bool NearEntrySpawned(Player* p, uint32 entry, float radius)
	{
		if (radius <= 0.f) return false;
	
		GameObject* go = p->FindNearestGameObject(entry, radius);
		if (!go)
			return false;
	
		if (go->GetGoType() != GO_TYPE_CHEST)
			return false;
	
		if (go->GetRespawnTime() != 0)
			return false;
	
		if (go->GetGoState() != GO_STATE_READY)
			return false;
	
		return true;
	}

	inline bool NearAnyBasic(Player* p)
	{
		return NearEntrySpawned(p, TREASURE_ENTRY_BASIC_CS, TreasureConf::notify_radius_basic) ||
			   NearEntrySpawned(p, TREASURE_ENTRY_BASIC_EN, TreasureConf::notify_radius_basic);
	}
	
	inline bool NearAnyRare(Player* p)
	{
		return NearEntrySpawned(p, TREASURE_ENTRY_RARE_CS, TreasureConf::notify_radius_rare) ||
			   NearEntrySpawned(p, TREASURE_ENTRY_RARE_EN, TreasureConf::notify_radius_rare);
	}
	
	inline bool NearAnyEpic(Player* p)
	{
		return NearEntrySpawned(p, TREASURE_ENTRY_EPIC_CS, TreasureConf::notify_radius_epic) ||
			   NearEntrySpawned(p, TREASURE_ENTRY_EPIC_EN, TreasureConf::notify_radius_epic);
	}
	
	inline char const* MsgBasic()
	{
		return TreasureConf::langCs ? TreasureConf::notify_msg_basic_cs.c_str()
									: TreasureConf::notify_msg_basic_en.c_str();
	}
	inline char const* MsgRare()
	{
		return TreasureConf::langCs ? TreasureConf::notify_msg_rare_cs.c_str()
									: TreasureConf::notify_msg_rare_en.c_str();
	}
	inline char const* MsgEpic()
	{
		return TreasureConf::langCs ? TreasureConf::notify_msg_epic_cs.c_str()
									: TreasureConf::notify_msg_epic_en.c_str();
	}
}

class TreasureProximityPlayerScript : public PlayerScript
{
public:
    TreasureProximityPlayerScript() : PlayerScript("TreasureProximityPlayerScript") {}

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        if (!player || !TreasureConf::enable || !TreasureConf::notify_enable)
            return;

        uint32 accId = GetAccountIdSafe(player);
        if (TreasureConf::IsDisabledAccount(accId))
            return;

        uint32 low = player->GetGUID().GetCounter();
        TrackState& st = g_tracks[low];

        uint32 mapId = player->GetMapId();
        float x = player->GetPositionX();
        float y = player->GetPositionY();
        float z = player->GetPositionZ();

		if (!st.inited)
		{
			st.inited = true;
			st.lastMap = mapId;
			st.lastX = x; st.lastY = y; st.lastZ = z;
		
			st.wasBasic = NearAnyBasic(player);
			st.wasRare  = NearAnyRare(player);
			st.wasEpic  = NearAnyEpic(player);
			return;
		}

        bool mapChanged = (mapId != st.lastMap);
        float dx = x - st.lastX, dy = y - st.lastY, dz = z - st.lastZ;
        float dist2 = dx*dx + dy*dy + dz*dz;

        if (!mapChanged && dist2 < 0.25f)
            return;

        st.lastMap = mapId;
        st.lastX = x; st.lastY = y; st.lastZ = z;

		bool nowBasic = NearAnyBasic(player);
		bool nowRare  = NearAnyRare(player);
		bool nowEpic  = NearAnyEpic(player);
		
		if (WorldSession* sess = player->GetSession())
		{
			if (!st.wasEpic && nowEpic)
				ChatHandler(sess).SendSysMessage(MsgEpic());
			if (!st.wasRare && nowRare)
				ChatHandler(sess).SendSysMessage(MsgRare());
			if (!st.wasBasic && nowBasic)
				ChatHandler(sess).SendSysMessage(MsgBasic());
		}
		
		st.wasBasic = nowBasic;
		st.wasRare  = nowRare;
		st.wasEpic  = nowEpic;

    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player) return;
        uint32 low = player->GetGUID().GetCounter();
        g_tracks.erase(low);
    }
};

/* ============================
 * CommandScript – .treasure
 * ============================ */

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
            treasureSub.emplace_back(ChatCommandBuilder("add",     Treasure_HandleAdd,     SEC_GAMEMASTER, Console::No));
            treasureSub.emplace_back(ChatCommandBuilder("list",    Treasure_HandleList,    SEC_GAMEMASTER, Console::No));
            treasureSub.emplace_back(ChatCommandBuilder("remove",  Treasure_HandleRemove,  SEC_GAMEMASTER, Console::No));
            treasureSub.emplace_back(ChatCommandBuilder("additem", Treasure_HandleAddItem, SEC_GAMEMASTER, Console::No));
            treasureSub.emplace_back(ChatCommandBuilder("tp",      Treasure_HandleTp,      SEC_GAMEMASTER, Console::No));
        }

        static ChatCommandTable root;
        if (root.empty())
        {
            root.emplace_back(ChatCommandBuilder("treasure", treasureSub));
        }
        return root;
    }
};

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

    uint32 map  = plr->GetMapId();
    uint32 zone = plr->GetZoneId();
    uint32 area = plr->GetAreaId();
    float x = plr->GetPositionX();
    float y = plr->GetPositionY();
    float z = plr->GetPositionZ();
    float o = plr->GetOrientation();

    uint32 pGuidLow = plr->GetGUID().GetCounter();
    uint32 now      = uint32(::time(nullptr));
    std::ostringstream mss;
    mss << "treasure-auto-" << pGuidLow << "-" << now;
    std::string marker = mss.str();

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

    uint32 guid = 0;
    {
        std::ostringstream sq;
        sq << "SELECT `guid` FROM `gameobject` "
            "WHERE `Comment`='" << EscapeSQL(marker) << "' "
            "ORDER BY `guid` DESC LIMIT 1;";
        if (QueryResult gqr = WorldDatabase.Query(sq.str().c_str()))
            guid = (*gqr)[0].Get<uint32>();
    }

    if (!guid)
    {
        if (QueryResult gqr2 = WorldDatabase.Query("SELECT `guid` FROM `gameobject` ORDER BY `guid` DESC LIMIT 1;"))
            guid = (*gqr2)[0].Get<uint32>();
    }

    {
        std::ostringstream uq;
        uq << "UPDATE `gameobject` SET `Comment`='"
        << (TreasureConf::langCs ? "Treasure modul – auto" : "Treasure module – auto")
        << "' WHERE `guid`=" << guid << " LIMIT 1;";
        WorldDatabase.Execute(uq.str().c_str());
    }

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

    Map* mapPtr = plr->GetMap();
    if (mapPtr && guid != 0)
    {
        GameObject* ngo = new GameObject();
    
        float sinHalf = std::sin(o * 0.5f);
        float cosHalf = std::cos(o * 0.5f);
        G3D::Quat rot;
        rot.x = 0.0f;
        rot.y = 0.0f;
        rot.z = sinHalf;
        rot.w = cosHalf;
    
        uint32 phaseMask = plr->GetPhaseMask();
    
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
            ngo->SetRespawnDelay(respawn);
            ngo->SetRespawnTime(0);
            ngo->SetGoState(GO_STATE_READY);
    
            if (!mapPtr->AddToMap(ngo))
            {
                LOG_ERROR("module", "mod_treasure: AddToMap() failed for guid={} entry={}", guid, entry);
                handler->SendSysMessage(TreasureConf::langCs ? "Chyba: AddToMap() selhalo." : "Error: AddToMap() failed.");
                delete ngo;
            }
            else
            {
                ngo->setActive(true);
                ngo->SetPhaseMask(phaseMask, true);
            }
        }
    }
    else
    {
        handler->SendSysMessage(TreasureConf::langCs ? "Chyba: Map* neplatná nebo GUID=0." : "Error: invalid Map* or GUID=0.");
    }

    std::ostringstream ok;
    if (TreasureConf::langCs)
        ok << "Přidána truhla (kvalita " << a << "), GUID " << guid << ". ID v customs přiřazeno automaticky.";
    else
        ok << "Chest added (quality " << a << "), GUID " << guid << ". Custom ID assigned automatically.";
    handler->SendSysMessage(ok.str().c_str());

    return true;
}

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

    uint32 itemId = uint32(std::strtoul(si.c_str(), nullptr, 10));

    uint32 minCount = 1, maxCount = 1;
    {
        size_t dash = srange.find('-');
        if (dash == std::string::npos)
        {
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

/* ============================
 * registrace
 * ============================ */

void Addmod_treasureScripts()
{
    new TreasureWorldScript();
    new TreasureChestPersistGO();
    new TreasureCommands();
    new TreasureProximityPlayerScript(); // +++ registrace proximity skriptu +++
}
