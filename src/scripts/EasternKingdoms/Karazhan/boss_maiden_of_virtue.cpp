/*
 * Copyright (C) 2013  BlizzLikeGroup
 * BlizzLikeCore integrates as part of this file: CREDITS.md and LICENSE.md
 */

/* ScriptData
Name: Boss_Maiden_of_Virtue
Complete(%): 100
Comment:
Category: Karazhan
EndScriptData */

#include "ScriptPCH.h"

#define SAY_AGGRO               -1532018
#define SAY_SLAY1               -1532019
#define SAY_SLAY2               -1532020
#define SAY_SLAY3               -1532021
#define SAY_REPENTANCE1         -1532022
#define SAY_REPENTANCE2         -1532023
#define SAY_DEATH               -1532024

#define SPELL_REPENTANCE        29511
#define SPELL_HOLYFIRE          29522
#define SPELL_HOLYWRATH         32445
#define SPELL_HOLYGROUND        29512
#define SPELL_BERSERK           26662

struct boss_maiden_of_virtueAI : public ScriptedAI
{
    boss_maiden_of_virtueAI(Creature* c) : ScriptedAI(c) {}

    uint32 Repentance_Timer;
    uint32 Holyfire_Timer;
    uint32 Holywrath_Timer;
    uint32 Holyground_Timer;
    uint32 Enrage_Timer;

    bool Enraged;

    void Reset()
    {
        Repentance_Timer    = 30000+(rand()%15000);
        Holyfire_Timer      = 8000+(rand()%17000);
        Holywrath_Timer     = 20000+(rand()%10000);
        Holyground_Timer    = 3000;
        Enrage_Timer        = 600000;

        Enraged = false;
    }

    void KilledUnit(Unit* /*Victim*/)
    {
        if (urand(0,1) == 0)
            DoScriptText(RAND(SAY_SLAY1,SAY_SLAY2,SAY_SLAY3), me);
    }

    void JustDied(Unit* /*Killer*/)
    {
        DoScriptText(SAY_DEATH, me);
    }

    void EnterCombat(Unit* /*who*/)
    {
        DoScriptText(SAY_AGGRO, me);
    }

    void UpdateAI(const uint32 diff)
    {
        if (!UpdateVictim())
            return;

        if (Enrage_Timer <= diff && !Enraged)
        {
            DoCast(me, SPELL_BERSERK, true);
            Enraged = true;
        } else Enrage_Timer -= diff;

        if (Holyground_Timer <= diff)
        {
            DoCast(me, SPELL_HOLYGROUND, true);   //Triggered so it doesn't interrupt her at all
            Holyground_Timer = 3000;
        } else Holyground_Timer -= diff;

        if (Repentance_Timer <= diff)
        {
            DoCast(me->getVictim(), SPELL_REPENTANCE);
            DoScriptText(RAND(SAY_REPENTANCE1,SAY_REPENTANCE2), me);

            Repentance_Timer = urand(30000,45000);        //A little randomness on that spell
        } else Repentance_Timer -= diff;

        if (Holyfire_Timer <= diff)
        {
            if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                DoCast(pTarget, SPELL_HOLYFIRE);

                Holyfire_Timer = urand(8000,25000);      //Anywhere from 8 to 25 seconds, good luck having several of those in a row!
        } else Holyfire_Timer -= diff;

        if (Holywrath_Timer <= diff)
        {
            if (Unit* pTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 100, true))
                DoCast(pTarget, SPELL_HOLYWRATH);

            Holywrath_Timer = urand(20000,30000);        //20-30 secs sounds nice
        } else Holywrath_Timer -= diff;

        DoMeleeAttackIfReady();
    }

};

CreatureAI* GetAI_boss_maiden_of_virtue(Creature* pCreature)
{
    return new boss_maiden_of_virtueAI (pCreature);
}

void AddSC_boss_maiden_of_virtue()
{
    Script *newscript;
    newscript = new Script;
    newscript->Name = "boss_maiden_of_virtue";
    newscript->GetAI = &GetAI_boss_maiden_of_virtue;
    newscript->RegisterSelf();
}

