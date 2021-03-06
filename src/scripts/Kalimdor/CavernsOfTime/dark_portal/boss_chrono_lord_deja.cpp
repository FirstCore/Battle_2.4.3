/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

/* ScriptData
Name: Boss_Chrono_Lord_Deja
Complete(%): 65
Comment: All abilities not implemented
Category: Caverns of Time, The Dark Portal
EndScriptData */

#include "ScriptPCH.h"
#include "dark_portal.h"

#define SAY_ENTER                   -1269006
#define SAY_AGGRO                   -1269007
#define SAY_BANISH                  -1269008
#define SAY_SLAY1                   -1269009
#define SAY_SLAY2                   -1269010
#define SAY_DEATH                   -1269011

#define SPELL_ARCANE_BLAST          31457
#define H_SPELL_ARCANE_BLAST        38538
#define SPELL_ARCANE_DISCHARGE      31472
#define H_SPELL_ARCANE_DISCHARGE    38539
#define SPELL_TIME_LAPSE            31467
#define SPELL_ATTRACTION            38540                       //Not Implemented (Heroic mode)

struct boss_chrono_lord_dejaAI : public ScriptedAI
{
    boss_chrono_lord_dejaAI(Creature* c) : ScriptedAI(c)
    {
        pInstance = c->GetInstanceData();
        HeroicMode = me->GetMap()->IsHeroic();
    }

    ScriptedInstance *pInstance;
    bool HeroicMode;

    uint32 ArcaneBlast_Timer;
    uint32 TimeLapse_Timer;

    void Reset()
    {
        ArcaneBlast_Timer = 20000;
        TimeLapse_Timer = 15000;
    }

    void EnterCombat(Unit*)
    {
        DoScriptText(SAY_AGGRO, me);
    }

    void MoveInLineOfSight(Unit* who)
    {
        //Despawn Time Keeper
        if (who->GetTypeId() == TYPEID_UNIT && who->GetEntry() == C_TIME_KEEPER)
        {
            if (me->IsWithinDistInMap(who,20.0f))
            {
                DoScriptText(SAY_BANISH, me);
                me->DealDamage(who, who->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
            }
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void KilledUnit(Unit*)
    {
        switch(rand()%2)
        {
            case 0: DoScriptText(SAY_SLAY1, me); break;
            case 1: DoScriptText(SAY_SLAY2, me); break;
        }
    }

    void JustDied(Unit*)
    {
        DoScriptText(SAY_DEATH, me);

        if (pInstance)
            pInstance->SetData(TYPE_RIFT,SPECIAL);
    }

    void UpdateAI(const uint32 diff)
    {
        //Return since we have no target
        if (!UpdateVictim())
            return;

        //Arcane Blast
        if (ArcaneBlast_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_ARCANE_BLAST);
            ArcaneBlast_Timer = 20000+rand()%5000;
        } else ArcaneBlast_Timer -= diff;

        //Time Lapse
        if (TimeLapse_Timer <= diff)
        {
            DoScriptText(SAY_BANISH, me);
            DoCast(me, SPELL_TIME_LAPSE);
            TimeLapse_Timer = 15000+rand()%10000;
        } else TimeLapse_Timer -= diff;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_chrono_lord_deja(Creature* pCreature)
{
    return new boss_chrono_lord_dejaAI (pCreature);
}

void AddSC_boss_chrono_lord_deja()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_chrono_lord_deja";
    newscript->GetAI = &GetAI_boss_chrono_lord_deja;
    newscript->RegisterSelf();
}

