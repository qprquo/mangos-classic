/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Dustwallow_Marsh
SD%Complete: 95
SDComment: Quest support: 1173, 1222, 1270, 1273, 1324.
SDCategory: Dustwallow Marsh
EndScriptData */

/* ContentData
npc_morokk
npc_ogron
npc_private_hendel
npc_stinky_ignatz
at_nats_landing
EndContentData */

#include "precompiled.h"
#include "escort_ai.h"

/*######
## npc_morokk
######*/

enum
{
    SAY_MOR_CHALLENGE               = -1000499,
    SAY_MOR_SCARED                  = -1000500,

    QUEST_CHALLENGE_MOROKK          = 1173,

    FACTION_MOR_HOSTILE             = 168,
    FACTION_MOR_RUNNING             = 35
};

struct npc_morokkAI : public npc_escortAI
{
    npc_morokkAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_bIsSuccess = false;
        Reset();
    }

    bool m_bIsSuccess;

    void Reset() override {}

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 0:
                SetEscortPaused(true);
                break;
            case 1:
                if (m_bIsSuccess)
                    DoScriptText(SAY_MOR_SCARED, m_creature);
                else
                {
                    m_creature->SetDeathState(JUST_DIED);
                    m_creature->Respawn();
                }
                break;
        }
    }

    void AttackedBy(Unit* pAttacker) override
    {
        if (m_creature->getVictim())
            return;

        if (m_creature->IsFriendlyTo(pAttacker))
            return;

        AttackStart(pAttacker);
    }

    virtual void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage, DamageEffectType /*damagetype*/) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING))
        {
            if (m_creature->GetHealthPercent() < 30.0f)
            {
                if (Player* pPlayer = GetPlayerForEscort())
                    pPlayer->GroupEventHappens(QUEST_CHALLENGE_MOROKK, m_creature);

                m_creature->setFaction(FACTION_MOR_RUNNING);

                m_bIsSuccess = true;
                EnterEvadeMode();

                uiDamage = 0;
            }
        }
    }

    void UpdateEscortAI(const uint32 /*uiDiff*/) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (HasEscortState(STATE_ESCORT_PAUSED))
            {
                if (Player* pPlayer = GetPlayerForEscort())
                {
                    m_bIsSuccess = false;
                    DoScriptText(SAY_MOR_CHALLENGE, m_creature, pPlayer);
                    m_creature->setFaction(FACTION_MOR_HOSTILE);
                    AttackStart(pPlayer);
                }

                SetEscortPaused(false);
            }

            return;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_morokk(Creature* pCreature)
{
    return new npc_morokkAI(pCreature);
}

bool QuestAccept_npc_morokk(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_CHALLENGE_MOROKK)
    {
        if (npc_morokkAI* pEscortAI = dynamic_cast<npc_morokkAI*>(pCreature->AI()))
            pEscortAI->Start(true, pPlayer, pQuest);

        return true;
    }

    return false;
}

/*######
## npc_ogron
######*/

enum
{
    SAY_OGR_START                       = -1000452,
    SAY_OGR_SPOT                        = -1000453,
    SAY_OGR_RET_WHAT                    = -1000454,
    SAY_OGR_RET_SWEAR                   = -1000455,
    SAY_OGR_REPLY_RET                   = -1000456,
    SAY_OGR_RET_TAKEN                   = -1000457,
    SAY_OGR_TELL_FIRE                   = -1000458,
    SAY_OGR_RET_NOCLOSER                = -1000459,
    SAY_OGR_RET_NOFIRE                  = -1000460,
    SAY_OGR_RET_HEAR                    = -1000461,
    SAY_OGR_CAL_FOUND                   = -1000462,
    SAY_OGR_CAL_MERCY                   = -1000463,
    SAY_OGR_HALL_GLAD                   = -1000464,
    EMOTE_OGR_RET_ARROW                 = -1000465,
    SAY_OGR_RET_ARROW                   = -1000466,
    SAY_OGR_CAL_CLEANUP                 = -1000467,
    SAY_OGR_NODIE                       = -1000468,
    SAY_OGR_SURVIVE                     = -1000469,
    SAY_OGR_RET_LUCKY                   = -1000470,
    SAY_OGR_THANKS                      = -1000471,

    QUEST_QUESTIONING                   = 1273,

    FACTION_GENERIC_FRIENDLY            = 35,
    FACTION_THER_HOSTILE                = 151,

    NPC_REETHE                          = 4980,
    NPC_CALDWELL                        = 5046,
    NPC_HALLAN                          = 5045,
    NPC_SKIRMISHER                      = 5044,

    SPELL_FAKE_SHOT                     = 7105,

    PHASE_INTRO                         = 0,
    PHASE_GUESTS                        = 1,
    PHASE_FIGHT                         = 2,
    PHASE_COMPLETE                      = 3
};

static float m_afSpawn[] = { -3383.501953f, -3203.383301f, 36.149f};
static float m_afMoveTo[] = { -3371.414795f, -3212.179932f, 34.210f};

struct npc_ogronAI : public npc_escortAI
{
    npc_ogronAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        lCreatureList.clear();
        m_uiPhase = 0;
        m_uiPhaseCounter = 0;
        Reset();
    }

    std::list<Creature*> lCreatureList;

    uint32 m_uiPhase;
    uint32 m_uiPhaseCounter;
    uint32 m_uiGlobalTimer;

    void Reset() override
    {
        m_uiGlobalTimer = 5000;

        if (HasEscortState(STATE_ESCORT_PAUSED) && m_uiPhase == PHASE_FIGHT)
            m_uiPhase = PHASE_COMPLETE;

        if (!HasEscortState(STATE_ESCORT_ESCORTING))
        {
            lCreatureList.clear();
            m_uiPhase = 0;
            m_uiPhaseCounter = 0;
        }
    }

    void MoveInLineOfSight(Unit* pWho) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING) && pWho->GetEntry() == NPC_REETHE && lCreatureList.empty())
            lCreatureList.push_back((Creature*)pWho);

        npc_escortAI::MoveInLineOfSight(pWho);
    }

    Creature* GetCreature(uint32 uiCreatureEntry)
    {
        if (!lCreatureList.empty())
        {
            for (std::list<Creature*>::iterator itr = lCreatureList.begin(); itr != lCreatureList.end(); ++itr)
            {
                if ((*itr)->GetEntry() == uiCreatureEntry && (*itr)->isAlive())
                    return (*itr);
            }
        }

        return nullptr;
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 9:
                DoScriptText(SAY_OGR_SPOT, m_creature);
                break;
            case 10:
                if (Creature* pReethe = GetCreature(NPC_REETHE))
                    DoScriptText(SAY_OGR_RET_WHAT, pReethe);
                break;
            case 11:
                SetEscortPaused(true);
                break;
        }
    }

    void JustSummoned(Creature* pSummoned) override
    {
        lCreatureList.push_back(pSummoned);

        pSummoned->setFaction(FACTION_GENERIC_FRIENDLY);

        if (pSummoned->GetEntry() == NPC_CALDWELL)
            pSummoned->GetMotionMaster()->MovePoint(0, m_afMoveTo[0], m_afMoveTo[1], m_afMoveTo[2]);
        else
        {
            if (Creature* pCaldwell = GetCreature(NPC_CALDWELL))
            {
                // will this conversion work without compile warning/error?
                size_t iSize = lCreatureList.size();
                pSummoned->GetMotionMaster()->MoveFollow(pCaldwell, 0.5f, (M_PI / 2) * (int)iSize);
            }
        }
    }

    void DoStartAttackMe()
    {
        if (!lCreatureList.empty())
        {
            for (std::list<Creature*>::iterator itr = lCreatureList.begin(); itr != lCreatureList.end(); ++itr)
            {
                if ((*itr)->GetEntry() == NPC_REETHE)
                    continue;

                if ((*itr)->isAlive())
                {
                    (*itr)->setFaction(FACTION_THER_HOSTILE);
                    (*itr)->AI()->AttackStart(m_creature);
                }
            }
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (HasEscortState(STATE_ESCORT_PAUSED))
            {
                if (m_uiGlobalTimer < uiDiff)
                {
                    m_uiGlobalTimer = 5000;

                    switch (m_uiPhase)
                    {
                        case PHASE_INTRO:
                        {
                            switch (m_uiPhaseCounter)
                            {
                                case 0:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_SWEAR, pReethe);
                                    break;
                                case 1:
                                    DoScriptText(SAY_OGR_REPLY_RET, m_creature);
                                    break;
                                case 2:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_TAKEN, pReethe);
                                    break;
                                case 3:
                                    DoScriptText(SAY_OGR_TELL_FIRE, m_creature);
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_NOCLOSER, pReethe);
                                    break;
                                case 4:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_NOFIRE, pReethe);
                                    break;
                                case 5:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_HEAR, pReethe);

                                    m_creature->SummonCreature(NPC_CALDWELL, m_afSpawn[0], m_afSpawn[1], m_afSpawn[2], 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 300000);
                                    m_creature->SummonCreature(NPC_HALLAN, m_afSpawn[0], m_afSpawn[1], m_afSpawn[2], 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 300000);
                                    m_creature->SummonCreature(NPC_SKIRMISHER, m_afSpawn[0], m_afSpawn[1], m_afSpawn[2], 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 300000);
                                    m_creature->SummonCreature(NPC_SKIRMISHER, m_afSpawn[0], m_afSpawn[1], m_afSpawn[2], 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 300000);

                                    m_uiPhase = PHASE_GUESTS;
                                    break;
                            }
                            break;
                        }
                        case PHASE_GUESTS:
                        {
                            switch (m_uiPhaseCounter)
                            {
                                case 6:
                                    if (Creature* pCaldwell = GetCreature(NPC_CALDWELL))
                                        DoScriptText(SAY_OGR_CAL_FOUND, pCaldwell);
                                    break;
                                case 7:
                                    if (Creature* pCaldwell = GetCreature(NPC_CALDWELL))
                                        DoScriptText(SAY_OGR_CAL_MERCY, pCaldwell);
                                    break;
                                case 8:
                                    if (Creature* pHallan = GetCreature(NPC_HALLAN))
                                    {
                                        DoScriptText(SAY_OGR_HALL_GLAD, pHallan);

                                        if (Creature* pReethe = GetCreature(NPC_REETHE))
                                            pHallan->CastSpell(pReethe, SPELL_FAKE_SHOT, TRIGGERED_NONE);
                                    }
                                    break;
                                case 9:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                    {
                                        DoScriptText(EMOTE_OGR_RET_ARROW, pReethe);
                                        DoScriptText(SAY_OGR_RET_ARROW, pReethe);
                                    }
                                    break;
                                case 10:
                                    if (Creature* pCaldwell = GetCreature(NPC_CALDWELL))
                                        DoScriptText(SAY_OGR_CAL_CLEANUP, pCaldwell);

                                    DoScriptText(SAY_OGR_NODIE, m_creature);
                                    break;
                                case 11:
                                    DoStartAttackMe();
                                    m_uiPhase = PHASE_FIGHT;
                                    break;
                            }
                            break;
                        }
                        case PHASE_COMPLETE:
                        {
                            switch (m_uiPhaseCounter)
                            {
                                case 12:
                                    if (Player* pPlayer = GetPlayerForEscort())
                                        pPlayer->GroupEventHappens(QUEST_QUESTIONING, m_creature);

                                    DoScriptText(SAY_OGR_SURVIVE, m_creature);
                                    break;
                                case 13:
                                    if (Creature* pReethe = GetCreature(NPC_REETHE))
                                        DoScriptText(SAY_OGR_RET_LUCKY, pReethe);
                                    break;
                                case 14:
                                    DoScriptText(SAY_OGR_THANKS, m_creature);
                                    SetRun();
                                    SetEscortPaused(false);
                                    break;
                            }
                            break;
                        }
                    }

                    if (m_uiPhase != PHASE_FIGHT)
                        ++m_uiPhaseCounter;
                }
                else
                    m_uiGlobalTimer -= uiDiff;
            }

            return;
        }

        DoMeleeAttackIfReady();
    }
};

bool QuestAccept_npc_ogron(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_QUESTIONING)
    {
        if (npc_ogronAI* pEscortAI = dynamic_cast<npc_ogronAI*>(pCreature->AI()))
        {
            pEscortAI->Start(false, pPlayer, pQuest, true);
            pCreature->SetFactionTemporary(FACTION_ESCORT_N_FRIEND_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
            DoScriptText(SAY_OGR_START, pCreature, pPlayer);
        }
    }

    return true;
}

CreatureAI* GetAI_npc_ogron(Creature* pCreature)
{
    return new npc_ogronAI(pCreature);
}

/*######
## npc_private_hendel
######*/

enum
{
    SPELL_ENCAGE                = 7081,                     // guessed
    TELEPORT_VISUAL             = 12980,                    // guessed
    SPELL_SELF_STUN             = 9032,                     // guessed
    SPELL_TELEPORT              = 7794,
    SPELL_TELEPORT_GROUP        = 7143,

    SAY_PROGRESS_1_TER          = -1000411,
    SAY_PROGRESS_2_HEN          = -1000412,
    SAY_PROGRESS_3_TER          = -1000413,
    SAY_PROGRESS_4_TER          = -1000414,
    SAY_FAREWELL_TER            = -1999926,

    EMOTE_SURRENDER             = -1000415,

    QUEST_MISSING_DIPLO_PT16    = 1324,
    FACTION_HOSTILE             = 168,                      // guessed, may be different

    NPC_SENTRY                  = 5184,                     // helps hendel
    NPC_JAINA                   = 4968,                     // appears once hendel gives up
    NPC_TERVOSH                 = 4967,
    NPC_PAINED                  = 4965,
    NPC_HENDEL                  = 4966,

    MD_PHASE_NONE               = 0,                        // do nothing
    MD_PHASE_FIGHT              = 1,                        // fight with a player
    MD_PHASE_SUMMON             = 2                         // handle the end event
};


const float tervoshSpawn[4]  = { -2857.604492f, -3354.784912f, 35.369640f, 3.466099f };
const float tervoshDest[3]   = { -2881.546631f, -3346.477539f, 34.143719f };


struct Spawn
{
    float x, y, z, o;
    uint32 entry;
};

const Spawn spawns[] = 
{
    { -2858.120117f, -3358.469971f, 36.086300f, 3.466099f , NPC_JAINA },
    { -2857.379883f, -3351.370117f, 34.178001f, 3.466099f , NPC_PAINED}
};

const float dests[][3] = 
{
    { -2878.110107f, -3349.449951f, 35.199100f }, //Jaina
    { -2877.679932f, -3342.479980f, 35.116001f }  // Pained

};
const float guardsSpawn[][4] = 
{
    { -2889.72f, -3337.92f, 32.4705f, 5.58505f }, //1-st guard
    { -2891.22f, -3344.70f, 32.3499f, 2.9721f  }  //2-nd guard
};

// TODO: develop this further, end event not created

#define TEMPSUMMON_DURATION 60000 // 60 sec
#define SPEECH_DELAY        4000  // 4 sec


#define WAVE_TIMER          TEMPSUMMON_DURATION - 7500
#define CAST_TELEPORT_TIMER TEMPSUMMON_DURATION - 2500
#define TELEPORT_BACK_TIMER TEMPSUMMON_DURATION - 1000
enum
{
    TERVOSH_STATE_NONE,
    TERVOSH_STATE_MD_ACTIVE,
    TERVOSH_STATE_MD_ARRIVED,
    TERVOSH_STATE_MD_SPEECH,
    TERVOSH_STATE_MD_COMPLETE,
    TERVOSH_STATE_MD_FINAL
};

struct HelperAI: ScriptedAI
{
    HelperAI(Creature *pCreature) : ScriptedAI(pCreature) {};

    GuidList m_guids;

    Creature* GetCreature(uint32 entry)
    {
        if (!m_guids.empty())
        {
            for (std::list<ObjectGuid>::iterator itr = m_guids.begin(); itr != m_guids.end(); ++itr)
            {
                Creature *pCreature = m_creature->GetMap()->GetCreature(*itr);

                if (pCreature && pCreature->GetEntry() == entry)
                    return pCreature;
            }
        }
        return nullptr;
    }
};
// TODO: initial teleport delay timer that prevents Jaina, Tervosh, Pained from moving
// TODO: guards who defend Hendel should flee when tervosh and jaina arrive
// TODO: buff all party members depending on their class
// TODO: find a way to access original NPCs
struct npc_tervoshAI : public HelperAI
{
    npc_tervoshAI(Creature* pCreature) :HelperAI(pCreature){ Reset(); }
    ObjectGuid m_guidPlayer;
    uint32 m_uiState;
    uint32 m_uiEncage;
    uint32 m_uiNextPhase;
    uint32 m_uiSayCounter;
    uint32 m_uiSpeechDelay;
    uint32 m_uiDespawnTimer;
    uint32 m_uiWave;
    uint32 m_uiTeleportBack;
    uint32 m_uiCastTeleport;
    uint32 m_uiSummCount;

    void CompleteQuest(uint32 uiQuestID)
    {
        if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_guidPlayer))
            pPlayer->GroupEventHappens(uiQuestID, m_creature);
        
    };

    void JustSummoned(Creature* pCreature ) override
    {
        uint32 entry = pCreature->GetEntry();

        if (!(entry == NPC_JAINA || entry == NPC_PAINED))
            return;

        pCreature->CastSpell(pCreature, TELEPORT_VISUAL, TRIGGERED_NONE);
       
        MotionMaster* pMotionMaster = pCreature->GetMotionMaster();

        if (pMotionMaster)
            pMotionMaster->MovePoint(0, dests[m_uiSummCount][0], dests[m_uiSummCount][1], dests[m_uiSummCount][2]);

        m_guids.push_back(pCreature->GetObjectGuid());
        ++m_uiSummCount;

    }

    void OnSummon(Creature* pcSummoner, ObjectGuid& player)
    {
        m_guids.push_back(pcSummoner->GetObjectGuid());
        m_guidPlayer = player;

        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP); 

        for (int i = 0; i < sizeof(spawns) / sizeof(Spawn) ;++i)
            m_creature->SummonCreature(spawns[i].entry, spawns[i].x, spawns[i].y, spawns[i].z, spawns[i].o, TEMPSUMMON_TIMED_DESPAWN, TEMPSUMMON_DURATION, false, true);

        m_creature->CastSpell(m_creature, TELEPORT_VISUAL, TRIGGERED_NONE);

        if(MotionMaster* pMotionMaster = m_creature->GetMotionMaster())
            pMotionMaster->MovePoint(0, tervoshDest[0], tervoshDest[1], tervoshDest[2]);
        
        m_uiState = TERVOSH_STATE_MD_ACTIVE;
    }

    void SetFacing(WorldObject* obj, bool bTurnAll = true)
    {
        if (!obj)
            return;

        m_creature->SetFacingTo(m_creature->GetAngle(obj));

        if (!bTurnAll)
            return;

        for (std::list<ObjectGuid>::iterator itr = m_guids.begin(); itr != m_guids.end(); ++itr)
        {
            Creature* pCreature = m_creature->GetMap()->GetCreature(*itr);
            if (pCreature && (pCreature->GetEntry() == NPC_JAINA || pCreature->GetEntry() == NPC_PAINED))
                pCreature->SetFacingTo(pCreature->GetAngle(obj));
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        switch (m_uiState)
        {
            case TERVOSH_STATE_NONE: // default state
                break;
            case TERVOSH_STATE_MD_ACTIVE:
            {
                float x, y, z;
                m_creature->GetPosition(x,y,z);

                if (x == tervoshDest[0] && y == tervoshDest[1] && z == tervoshDest[2])
                {
                    // set facing
                    if (Creature* pCreature = GetCreature(NPC_HENDEL))
                        SetFacing(pCreature);
                    m_uiState = TERVOSH_STATE_MD_ARRIVED;
                }
            }
                break;
            case TERVOSH_STATE_MD_ARRIVED:
                if (m_uiEncage < uiDiff)
                {
                    if (Creature* pCreature = GetCreature(NPC_HENDEL))
                        DoCastSpellIfCan(pCreature, SPELL_ENCAGE, CAST_FORCE_CAST);
                    m_uiEncage = 3000;
                }
                else
                    m_uiEncage -= uiDiff;

                if (m_uiNextPhase < uiDiff)
                    m_uiState     = TERVOSH_STATE_MD_SPEECH;
                else
                    m_uiNextPhase -= uiDiff;
                break;
            case TERVOSH_STATE_MD_SPEECH:
                if (m_uiSpeechDelay < uiDiff)
                {
                    switch (m_uiSayCounter)
                    {
                        case 0:
                            DoScriptText(SAY_PROGRESS_1_TER, m_creature);
                            break;
                        case 1:
                            if (Creature *pCreature = GetCreature(NPC_HENDEL))
                            {
                                DoScriptText(SAY_PROGRESS_2_HEN, pCreature);
                                m_creature->CastSpell(pCreature, SPELL_TELEPORT, TRIGGERED_NONE);
                            }
                            m_uiSpeechDelay = 1000 + 500; // teleport casting time + little delay for smooth effect
                            break;
                        case 2:
                            if (Creature *pCreature = GetCreature(NPC_HENDEL))
                                pCreature->ForcedDespawn();
                            DoScriptText(SAY_PROGRESS_3_TER, m_creature);
                            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
                            break;
                        case 3:
                            DoScriptText(SAY_PROGRESS_4_TER, m_creature);
                            m_uiState = TERVOSH_STATE_MD_COMPLETE;
                            return;
                    }
                    if (m_uiSayCounter != 1)
                        m_uiSpeechDelay = SPEECH_DELAY;
                   
                    ++m_uiSayCounter;
                }
                else
                    m_uiSpeechDelay -= uiDiff;
                break;
            case TERVOSH_STATE_MD_COMPLETE:
                CompleteQuest(QUEST_MISSING_DIPLO_PT16);
                m_uiState = TERVOSH_STATE_MD_FINAL;
                break;
        }
        if (m_uiState != TERVOSH_STATE_NONE)
        {
            // handle despawn timer
            if (m_uiDespawnTimer < uiDiff)
            {
                m_uiState = TERVOSH_STATE_NONE;
                return;
            }
            else
                m_uiDespawnTimer -= uiDiff;

            if (m_uiWave < uiDiff)
            {
                if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_guidPlayer))
                    SetFacing(pPlayer);
                // Jaina waves :)
                if (Creature* pCreature = GetCreature(NPC_JAINA))
                    pCreature->HandleEmote(EMOTE_ONESHOT_WAVE);
                DoScriptText(SAY_FAREWELL_TER, m_creature);
                m_uiWave = WAVE_TIMER;
            }
            else
                m_uiWave -= uiDiff;

            if (m_uiCastTeleport < uiDiff)
            {
                if (Creature* pCreature = GetCreature(NPC_JAINA))
                    pCreature->CastSpell(m_creature, SPELL_TELEPORT, TRIGGERED_NONE);
                m_uiCastTeleport = CAST_TELEPORT_TIMER;
            }
            else
                m_uiCastTeleport -= uiDiff;

            if (m_uiTeleportBack < uiDiff)
            {
                if (Creature* pJaina = GetCreature(NPC_JAINA))
                    pJaina->CastSpell(pJaina, SPELL_TELEPORT_GROUP, TRIGGERED_NONE);

                m_uiTeleportBack = TELEPORT_BACK_TIMER;
            }
            else
                m_uiTeleportBack -= uiDiff;
        }
    }

    void SummonedCreatureDespawn(Creature* pCreature) override
    {
        m_guids.remove(pCreature->GetObjectGuid());
    }

    void Reset() override
    {
        m_uiState        = TERVOSH_STATE_NONE;
        m_uiEncage       = 0;
        m_uiNextPhase    = 1500;
        m_uiSayCounter   = 0;
        m_uiSpeechDelay  = SPEECH_DELAY;
        m_uiDespawnTimer = TEMPSUMMON_DURATION;
        m_uiSummCount    = 0;
        m_uiTeleportBack = TELEPORT_BACK_TIMER;
        m_uiWave         = WAVE_TIMER;
        m_uiCastTeleport = CAST_TELEPORT_TIMER;
        m_guids.clear();
        m_guidPlayer.Clear();
    }
};

CreatureAI* GetAI_npc_tervoshAI(Creature* pCreature)
{
    return new npc_tervoshAI(pCreature);
}

struct npc_private_hendelAI : public HelperAI
{
    npc_private_hendelAI(Creature* pCreature):
        HelperAI(pCreature)
    {
        Reset();
    }

    uint32 m_uiQuestPhase;
    ObjectGuid m_guidPlayer;

    void Reset() override 
    {
        m_guidPlayer.Clear();
        m_uiQuestPhase = MD_PHASE_NONE;
        //m_creature->HandleEmote(0);
        m_creature->setFaction(m_creature->GetCreatureInfo()->FactionAlliance);
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP); 
    }

    void AttackedBy(Unit* pAttacker) override
    {
        if (m_creature->getVictim())
            return;

        if (m_creature->IsFriendlyTo(pAttacker))
            return;

        AttackStart(pAttacker);
    }

    void JustSummoned(Creature* pCreature) override
    {
        uint32 entry = pCreature->GetEntry();
        switch (entry)
        {
            case NPC_TERVOSH:
            {
                npc_tervoshAI* AI = dynamic_cast<npc_tervoshAI*>(pCreature->AI());
                if (AI)
                    AI->OnSummon(m_creature, m_guidPlayer);
            }
                break;
            case NPC_SENTRY:
                break;
        }
        m_guids.push_back(pCreature->GetObjectGuid());
    }

    void DoGiveUp()
    {
        DoScriptText(EMOTE_SURRENDER, m_creature);
        m_creature->RemoveAllAuras();
        m_creature->setFaction(m_creature->GetCreatureInfo()->FactionAlliance);
        m_creature->DeleteThreatList();

        if (MotionMaster* pMotionMaster = m_creature->GetMotionMaster())
            pMotionMaster->MoveTargetedHome();

        SummonTervosh();
        m_uiQuestPhase = MD_PHASE_SUMMON;
    }

    void JustReachedHome() override
    {
        if (m_uiQuestPhase == MD_PHASE_SUMMON)
        {
            /*
            // Actually this doesnt stop random moving around respawn point
            m_creature->StopMoving();
            m_creature->HandleEmote(EMOTE_STATE_STUN); */
            m_creature->CastSpell(m_creature, SPELL_SELF_STUN, TRIGGERED_NONE);
        }

    }

    void UpdateAI(const uint32 uiDiff) override
    {
        switch (m_uiQuestPhase)
        {
            case MD_PHASE_FIGHT:
            case MD_PHASE_NONE:
                ScriptedAI::UpdateAI(uiDiff);
        }
    }

    void SummonTervosh()
    {
        m_creature->SummonCreature(NPC_TERVOSH, tervoshSpawn[0], tervoshSpawn[1], tervoshSpawn[2], tervoshSpawn[3], TEMPSUMMON_TIMED_DESPAWN, TEMPSUMMON_DURATION, false, true);
    }

    void SummonedCreatureDespawn(Creature* pCreature) override
    {
        m_guids.remove(pCreature->GetObjectGuid());
    }

    void DamageTaken(Unit* pDoneBy, uint32& uiDamage, DamageEffectType /*damagetype*/) override
    {
        if (m_uiQuestPhase == MD_PHASE_NONE)
            return;

        if (uiDamage > m_creature->GetHealth() || m_creature->GetHealthPercent() < 20.0f)
        {
            uiDamage = 0;
            DoGiveUp();
        }
    }

    void JustDied(Unit* /*pUnit*/) override
    {

    }
};

bool QuestAccept_npc_private_hendel(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_MISSING_DIPLO_PT16)
    {
        pCreature->SetFactionTemporary(FACTION_HOSTILE);
        pCreature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);

        if (npc_private_hendelAI* ai = dynamic_cast<npc_private_hendelAI *>(pCreature->AI()))
        {
            ai->m_guidPlayer = pPlayer->GetObjectGuid();
            ai->m_uiQuestPhase = MD_PHASE_FIGHT;
        }
        
    }
    return true;
}

CreatureAI* GetAI_npc_private_hendel(Creature* pCreature)
{
    return new npc_private_hendelAI(pCreature);
}

/*#####
## npc_stinky_ignatz
## TODO Note: Faction change is guessed
#####*/

enum
{
    QUEST_ID_STINKYS_ESCAPE_ALLIANCE    = 1222,
    QUEST_ID_STINKYS_ESCAPE_HORDE       = 1270,

    SAY_STINKY_BEGIN                    = -1000958,
    SAY_STINKY_FIRST_STOP               = -1000959,
    SAY_STINKY_SECOND_STOP              = -1001141,
    SAY_STINKY_THIRD_STOP_1             = -1001142,
    SAY_STINKY_THIRD_STOP_2             = -1001143,
    SAY_STINKY_THIRD_STOP_3             = -1001144,
    SAY_STINKY_PLANT_GATHERED           = -1001145,
    SAY_STINKY_END                      = -1000962,
    SAY_STINKY_AGGRO_1                  = -1000960,
    SAY_STINKY_AGGRO_2                  = -1000961,
    SAY_STINKY_AGGRO_3                  = -1001146,
    SAY_STINKY_AGGRO_4                  = -1001147,

    GO_BOGBEAN_PLANT                    = 20939,
};

struct npc_stinky_ignatzAI : public npc_escortAI
{
    npc_stinky_ignatzAI(Creature* pCreature) : npc_escortAI(pCreature) { Reset(); }

    ObjectGuid m_bogbeanPlantGuid;

    void Reset() override {}

    void Aggro(Unit* pWho) override
    {
        switch (urand(0, 3))
        {
            case 0: DoScriptText(SAY_STINKY_AGGRO_1, m_creature); break;
            case 1: DoScriptText(SAY_STINKY_AGGRO_2, m_creature); break;
            case 2: DoScriptText(SAY_STINKY_AGGRO_3, m_creature); break;
            case 3: DoScriptText(SAY_STINKY_AGGRO_4, m_creature, pWho); break;
        }
    }

    void ReceiveAIEvent(AIEventType eventType, Creature* /*pSender*/, Unit* pInvoker, uint32 uiMiscValue) override
    {
        if (eventType == AI_EVENT_START_ESCORT && pInvoker->GetTypeId() == TYPEID_PLAYER)
        {
            DoScriptText(SAY_STINKY_BEGIN, m_creature);
            Start(false, (Player*)pInvoker, GetQuestTemplateStore(uiMiscValue));
            m_creature->SetFactionTemporary(FACTION_ESCORT_N_NEUTRAL_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
            m_creature->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 5:
                DoScriptText(SAY_STINKY_FIRST_STOP, m_creature);
                break;
            case 10:
                DoScriptText(SAY_STINKY_SECOND_STOP, m_creature);
                break;
            case 24:
                DoScriptText(SAY_STINKY_THIRD_STOP_1, m_creature);
                break;
            case 25:
                DoScriptText(SAY_STINKY_THIRD_STOP_2, m_creature);
                if (GameObject* pBogbeanPlant = GetClosestGameObjectWithEntry(m_creature, GO_BOGBEAN_PLANT, DEFAULT_VISIBILITY_DISTANCE))
                {
                    m_bogbeanPlantGuid = pBogbeanPlant->GetObjectGuid();
                    m_creature->SetFacingToObject(pBogbeanPlant);
                }
                break;
            case 26:
                if (Player* pPlayer = GetPlayerForEscort())
                    DoScriptText(SAY_STINKY_THIRD_STOP_3, m_creature, pPlayer);
                break;
            case 29:
                m_creature->HandleEmote(EMOTE_STATE_USESTANDING);
                break;
            case 30:
                DoScriptText(SAY_STINKY_PLANT_GATHERED, m_creature);
                break;
            case 39:
                if (Player* pPlayer = GetPlayerForEscort())
                {
                    pPlayer->GroupEventHappens(pPlayer->GetTeam() == ALLIANCE ? QUEST_ID_STINKYS_ESCAPE_ALLIANCE : QUEST_ID_STINKYS_ESCAPE_HORDE, m_creature);
                    DoScriptText(SAY_STINKY_END, m_creature, pPlayer);
                }
                break;
        }
    }

    void WaypointStart(uint32 uiPointId)
    {
        if (uiPointId == 30)
        {
            if (GameObject* pBogbeanPlant = m_creature->GetMap()->GetGameObject(m_bogbeanPlantGuid))
                pBogbeanPlant->Use(m_creature);
            m_bogbeanPlantGuid.Clear();
            m_creature->HandleEmote(EMOTE_ONESHOT_NONE);
        }
    }
};

CreatureAI* GetAI_npc_stinky_ignatz(Creature* pCreature)
{
    return new npc_stinky_ignatzAI(pCreature);
}

bool QuestAccept_npc_stinky_ignatz(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ID_STINKYS_ESCAPE_ALLIANCE || pQuest->GetQuestId() == QUEST_ID_STINKYS_ESCAPE_HORDE)
        pCreature->AI()->SendAIEvent(AI_EVENT_START_ESCORT, pPlayer, pCreature, pQuest->GetQuestId());

    return true;
}

void AddSC_dustwallow_marsh()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_morokk";
    pNewScript->GetAI = &GetAI_npc_morokk;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_morokk;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_ogron";
    pNewScript->GetAI = &GetAI_npc_ogron;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_ogron;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_private_hendel";
    pNewScript->GetAI = &GetAI_npc_private_hendel;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_private_hendel;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_tervosh";
    pNewScript->GetAI = &GetAI_npc_tervoshAI;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_stinky_ignatz";
    pNewScript->GetAI = &GetAI_npc_stinky_ignatz;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_stinky_ignatz;
    pNewScript->RegisterSelf();
}
